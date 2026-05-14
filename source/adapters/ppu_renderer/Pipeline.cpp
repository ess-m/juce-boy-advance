//
// forked from nba's ppu pixel pipeline (background.cpp/.inl, sprite.cpp,
// window.cpp, merge.cpp). reads from a snapshot; no bus timestamps.
//
// Copyright (C) 2025 fleroviux — GPLv3+, see LICENSE.
//

#include "Pipeline.h"

namespace ppu_renderer {

void Pipeline::InitBackground() {
    bg.cycle = 0U;

    for(auto& text : bg.text) {
        text.fetches = 0;
    }

    const bool first_scanline = vcount_ == 0;

    for(int id = 0; id < 2; id++) {
        if(first_scanline) {
            bgx_current_[id] = snap_->bgx_initial[id];
            bgy_current_[id] = snap_->bgy_initial[id];
        }
        bg.affine[id].x = bgx_current_[id];
        bg.affine[id].y = bgy_current_[id];
    }
}

void Pipeline::InitSprite() {
    sprite.cycle = 0U;
    sprite.vcount = static_cast<uint>(vcount_);
    sprite.mosaic_y = mosaic_obj_counter_y_;

    sprite.oam_fetch.index = 0U;
    sprite.oam_fetch.step = 0;
    sprite.oam_fetch.wait = 0;
    sprite.oam_fetch.delay_wait = false;
    sprite.drawing = false;
    sprite.state_rd = 0;
    sprite.state_wr = 1;

    sprite.latch_cycle_limit = snap_->hblank_oam_access ? 964U : 1232U;

    std::memset(sprite.buffer_wr, 0, sizeof(Sprite::Pixel) * 240);
}

void Pipeline::InitWindow() {
    for(int i = 0; i < 2; i++) {
        const auto& winv = snap_->winv[i];
        if(vcount_ == winv.min) window.v_flag[i] = true;
        if(vcount_ == winv.max) window.v_flag[i] = false;
    }
    window.cycle = 0;
}

void Pipeline::InitMerge() {
    merge.cycle = 0U;
    merge.mosaic_x[0] = 0U;
    merge.mosaic_x[1] = 0U;
    merge.forced_blank = false;
    merge.sprite_pixel_latch.data = 0U;
}

void Pipeline::RenderMode0BG(uint id, uint cycle) {
    const auto& bgcnt = snap_->bgcnt[id];
    auto& text = bg.text[id];

    if(text.fetches > 0 && text.piso.remaining == 0) {
        u16 data = FetchVRAM_BG<u16>(cycle, text.tile.address);

        if(text.tile.flip_x) {
            data = (data >> 8) | (data << 8);
            if(!bgcnt.full_palette) {
                data = ((data & 0xF0F0U) >> 4) | ((data & 0x0F0FU) << 4);
            }
            text.tile.address -= sizeof(u16);
        } else {
            text.tile.address += sizeof(u16);
        }

        text.piso.data = data;
        text.piso.remaining = 4;
        text.fetches--;
    }

    uint index;
    const int screen_x = (cycle >> 2) - 9;

    if(bgcnt.full_palette) {
        index = text.piso.data & 0xFFU;
        text.piso.data >>= 8;
        text.piso.remaining -= 2;
    } else {
        index = text.piso.data & 0x0FU;
        if(index != 0U) index |= text.tile.palette << 4;
        text.piso.data >>= 4;
        text.piso.remaining--;
    }

    if(screen_x >= 0 && screen_x < 240) {
        bg.buffer[screen_x][id] = index;
    }

    const uint bghofs = snap_->bghofs[id];
    const uint bghofs_div_8 = bghofs >> 3;
    const uint bghofs_mod_8 = bghofs & 7U;
    const uint step = (cycle >> 2) + bghofs_mod_8;

    if(cycle < 1007U && step >= 8 && (step & 7) == 0) {
        const u32 tile_base = bgcnt.tile_block << 14;
        uint map_block = bgcnt.map_block;

        uint line = static_cast<uint>(vcount_) + snap_->bgvofs[id];
        if(bgcnt.mosaic_enable) {
            line -= static_cast<uint>(mosaic_bg_counter_y_);
        }

        const uint grid_x = bghofs_div_8 + (step >> 3) - 1U;
        const uint grid_y = line >> 3;
        const uint tile_y = line & 7U;

        const uint screen_x_grid = (grid_x >> 5) & 1U;
        const uint screen_y_grid = (grid_y >> 5) & 1U;

        switch(bgcnt.size) {
            case 1: map_block += screen_x_grid; break;
            case 2: map_block += screen_y_grid; break;
            case 3: map_block += screen_x_grid + (screen_y_grid << 1); break;
        }

        const u32 address = (map_block << 11) + ((grid_y & 31U) << 6) + ((grid_x & 31U) << 1);
        const u16 tile = FetchVRAM_BG<u16>(cycle, address);

        if(cycle < 1004U) {
            const uint number = tile & 0x3FFU;
            const bool flip_x = tile & (1U << 10);
            const bool flip_y = tile & (1U << 11);

            text.tile.palette = tile >> 12;
            text.tile.flip_x  = flip_x;

            const uint real_tile_y = flip_y ? (7 - tile_y) : tile_y;

            if(bgcnt.full_palette) {
                text.tile.address = tile_base + (number << 6) + (real_tile_y << 3);
                if(flip_x) text.tile.address += 6;
                text.fetches = 4;
            } else {
                text.tile.address = tile_base + (number << 5) + (real_tile_y << 2);
                if(flip_x) text.tile.address += 2;
                text.fetches = 2;
            }

            text.piso.remaining = 0;
        }
    }
}

void Pipeline::RenderMode2BG(uint id, uint cycle) {
    const auto& bgcnt = snap_->bgcnt[2 + id];

    if(cycle < 32U) return;

    if((cycle & 1U) == 0U) {
        const int log_size = bgcnt.size;
        const s32 size = 128 << log_size;
        const s32 mask = size - 1;

        s32 x = bg.affine[id].x >> 8;
        s32 y = bg.affine[id].y >> 8;

        bg.affine[id].x += snap_->bgpa[id];
        bg.affine[id].y += snap_->bgpc[id];

        if(bgcnt.wraparound) {
            x &= mask;
            y &= mask;
            bg.affine[id].out_of_bounds = false;
        } else {
            bg.affine[id].out_of_bounds = ((x | y) & -size) != 0;
        }

        const u16 address = (bgcnt.map_block << 11) + ((y >> 3) << (4 + log_size)) + (x >> 3);
        const u8 tile = FetchVRAM_BG<u8>(cycle, address);

        bg.affine[id].tile_address = (bgcnt.tile_block << 14) + (tile << 6) + ((y & 7) << 3) + (x & 7);
    } else {
        uint index = FetchVRAM_BG<u8>(cycle, bg.affine[id].tile_address);
        if(bg.affine[id].out_of_bounds) index = 0U;

        const uint x = (cycle - 32U) >> 2;
        if(x < 240) bg.buffer[x][2 + id] = index;
    }
}

void Pipeline::RenderMode3BG(uint cycle) {
    if(cycle < 32U || (cycle & 3U) != 3U) return;

    const uint screen_x = (cycle - 32U) >> 2;
    const s32 x = bg.affine[0].x >> 8;
    const s32 y = bg.affine[0].y >> 8;

    const u32 address = (static_cast<u32>(y) * 240U + static_cast<u32>(x)) * 2U;
    const u16 data = FetchVRAM_BG<u16>(cycle, address & 0x1FFFFU);

    u32 color = 0U;
    if(x >= 0 && x < 240 && y >= 0 && y < 160) {
        color = data | 0x8000'0000;
    }

    if(screen_x < 240U) bg.buffer[screen_x][2] = color;

    bg.affine[0].x += snap_->bgpa[0];
    bg.affine[0].y += snap_->bgpc[0];
}

void Pipeline::RenderMode4BG(uint cycle) {
    if(cycle < 32U || (cycle & 3U) != 3U) return;

    const uint screen_x = (cycle - 32U) >> 2;
    const s32 x = bg.affine[0].x >> 8;
    const s32 y = bg.affine[0].y >> 8;

    const u32 address = snap_->frame_select * 0xA000U + static_cast<u32>(y) * 240U + static_cast<u32>(x);
    const u8 data = FetchVRAM_BG<u8>(cycle, address & 0x1FFFFU);

    uint index = 0U;
    if(x >= 0 && x < 240 && y >= 0 && y < 160) index = data;

    if(screen_x < 240U) bg.buffer[screen_x][2] = index;

    bg.affine[0].x += snap_->bgpa[0];
    bg.affine[0].y += snap_->bgpc[0];
}

void Pipeline::RenderMode5BG(uint cycle) {
    if(cycle < 32U || (cycle & 3U) != 3U) return;

    const uint screen_x = (cycle - 32U) >> 2;
    const s32 x = bg.affine[0].x >> 8;
    const s32 y = bg.affine[0].y >> 8;

    const u32 address = snap_->frame_select * 0xA000U + (static_cast<u32>(y) * 160U + static_cast<u32>(x)) * 2U;
    const u16 data = FetchVRAM_BG<u16>(cycle, address & 0x1FFFFU);

    u32 color = 0U;
    if(x >= 0 && x < 160 && y >= 0 && y < 128) {
        color = data | 0x8000'0000;
    }

    if(screen_x < 240U) bg.buffer[screen_x][2] = color;

    bg.affine[0].x += snap_->bgpa[0];
    bg.affine[0].y += snap_->bgpc[0];
}

void Pipeline::DrawBackground() {
    const int cycles = 1232;
    if(bg.cycle >= 1232U) return;

    const int mode = snap_->mode;

    switch(mode) {
        case 0: DrawBackgroundImpl<0>(cycles); break;
        case 1: DrawBackgroundImpl<1>(cycles); break;
        case 2: DrawBackgroundImpl<2>(cycles); break;
        case 3: DrawBackgroundImpl<3>(cycles); break;
        case 4: DrawBackgroundImpl<4>(cycles); break;
        case 5: DrawBackgroundImpl<5>(cycles); break;
        case 6:
        case 7: DrawBackgroundImpl<7>(cycles); break;
    }
}

template<int mode> void Pipeline::DrawBackgroundImpl(int cycles) {
    const u16 latched_dispcnt_and_current_dispcnt = snap_->dispcnt_latch_0 & snap_->dispcnt_hword;

    for(int i = 0; i < cycles; i++) {
        const uint cycle = 1U + bg.cycle;

        if constexpr(mode <= 1) {
            const uint id = cycle & 3U;
            if((id <= 1 || mode == 0) && (latched_dispcnt_and_current_dispcnt & (256U << id))) {
                RenderMode0BG(id, cycle);
            }
        }

        if(cycle < 1007U) {
            if constexpr(mode == 1 || mode == 2) {
                const int id = ~(cycle >> 1) & 1;
                if((id == 0 || mode == 2) && (latched_dispcnt_and_current_dispcnt & (1024U << id))) {
                    RenderMode2BG(id, cycle);
                }
            }
            if constexpr(mode == 3) {
                if(latched_dispcnt_and_current_dispcnt & 1024U) RenderMode3BG(cycle);
            }
            if constexpr(mode == 4) {
                if(latched_dispcnt_and_current_dispcnt & 1024U) RenderMode4BG(cycle);
            }
            if constexpr(mode == 5) {
                if(latched_dispcnt_and_current_dispcnt & 1024U) RenderMode5BG(cycle);
            }
        }

        if(cycle == 1232U) {
            if(vcount_ < 159) {
                if(++mosaic_bg_counter_y_ == snap_->mosaic_bg_size_y) {
                    mosaic_bg_counter_y_ = 0;
                } else {
                    mosaic_bg_counter_y_ &= 15;
                }
            } else {
                mosaic_bg_counter_y_ = 0;
            }

            const auto AdvanceBGXY = [&](int id) {
                const auto bg_id = 2 + id;
                if(latched_dispcnt_and_current_dispcnt & (256U << bg_id)) {
                    if(snap_->bgcnt[bg_id].mosaic_enable) {
                        if(mosaic_bg_counter_y_ == 0) {
                            bgx_current_[id] += snap_->mosaic_bg_size_y * snap_->bgpb[id];
                            bgy_current_[id] += snap_->mosaic_bg_size_y * snap_->bgpd[id];
                        }
                    } else {
                        bgx_current_[id] += snap_->bgpb[id];
                        bgy_current_[id] += snap_->bgpd[id];
                    }
                }
            };

            if constexpr(mode >= 1 && mode <= 5) AdvanceBGXY(0);
            if constexpr(mode == 2) AdvanceBGXY(1);
        }

        if(++bg.cycle == 1232U) break;
    }
}

template void Pipeline::DrawBackgroundImpl<0>(int);
template void Pipeline::DrawBackgroundImpl<1>(int);
template void Pipeline::DrawBackgroundImpl<2>(int);
template void Pipeline::DrawBackgroundImpl<3>(int);
template void Pipeline::DrawBackgroundImpl<4>(int);
template void Pipeline::DrawBackgroundImpl<5>(int);
template void Pipeline::DrawBackgroundImpl<7>(int);

void Pipeline::DrawSpriteImpl(int cycles) {
    const uint cycle_limit = sprite.latch_cycle_limit;

    for(int i = 0; i < cycles; i++) {
        const uint cycle = sprite.cycle;

        if(snap_->enable[LAYER_OBJ] && (cycle & 1U) == 0U) {
            DrawSpriteFetchVRAM(cycle);
            DrawSpriteFetchOAM(cycle);
        }

        if(cycle == 1192U) {
            if(sprite.vcount < 159) {
                if(++mosaic_obj_counter_y_ == snap_->mosaic_obj_size_y) {
                    mosaic_obj_counter_y_ = 0;
                } else {
                    mosaic_obj_counter_y_ &= 15;
                }
            } else {
                mosaic_obj_counter_y_ = 0;
            }
        }

        if(++sprite.cycle == cycle_limit) break;
    }
}

void Pipeline::DrawSpriteFetchOAM(uint cycle) {
    static constexpr int k_sprite_size[4][4][2] = {
        { { 8 , 8  }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
        { { 16, 8  }, { 32, 8  }, { 32, 16 }, { 64, 32 } },
        { { 8 , 16 }, { 8 , 32 }, { 16, 32 }, { 32, 64 } },
        { { 8 , 8  }, { 8 , 8  }, { 8 , 8  }, { 8 , 8  } }
    };

    auto& oam_fetch = sprite.oam_fetch;

    if(oam_fetch.wait > 0 && !oam_fetch.delay_wait) {
        oam_fetch.wait--;
        return;
    }

    oam_fetch.delay_wait = false;
    auto& drawer_state = sprite.drawer_state[sprite.state_wr];

    const auto Submit = [&]() {
        sprite.state_rd ^= 1;
        sprite.state_wr ^= 1;
        sprite.drawing = true;
        oam_fetch.index++;
        oam_fetch.step = 0;
        oam_fetch.wait = oam_fetch.pending_wait;
        oam_fetch.delay_wait = true;
    };

    const int step = oam_fetch.step;

    switch(step) {
        case 0: {
            if(oam_fetch.index == 128U) { oam_fetch.step = 6; break; }
            const u32 attr01 = FetchOAM<u32>(cycle, oam_fetch.index * 8U);

            bool active = false;
            if((attr01 & 0x300U) != 0x200U) {
                const uint mode_attr = (attr01 >> 10) & 3U;
                if(mode_attr != OBJ_PROHIBITED) {
                    s32 x = (attr01 >> 16) & 0x1FF;
                    s32 y =  attr01 & 0xFF;
                    if(x >= 240) x -= 512;

                    const uint shape = (attr01 >> 14) & 3U;
                    const uint size  =  attr01 >> 30;
                    const int width  = k_sprite_size[shape][size][0];
                    const int height = k_sprite_size[shape][size][1];

                    int half_width  = width  >> 1;
                    int half_height = height >> 1;

                    const bool affine = attr01 & 0x100U;
                    if(affine) {
                        const bool double_size = attr01 & 0x200U;
                        if(double_size) {
                            half_width  *= 2;
                            half_height *= 2;
                        }
                    }

                    const int vcount_sprite = sprite.vcount;
                    const int y_max = (y + half_height * 2) & 255;

                    if((vcount_sprite >= y || y_max < y) && vcount_sprite < y_max) {
                        const bool mosaic = (attr01 & (1 << 12)) && mode_attr != OBJ_WINDOW;

                        drawer_state.width = width;
                        drawer_state.height = height;
                        drawer_state.mode = mode_attr;
                        drawer_state.mosaic = mosaic;
                        drawer_state.affine = affine;
                        drawer_state.draw_x = x;
                        drawer_state.remaining_pixels = half_width << 1;
                        drawer_state.is_256 = (attr01 >> 13) & 1;

                        int local_y = (vcount_sprite - y) & 255;
                        if(mosaic) local_y = std::max(0, local_y - sprite.mosaic_y);

                        if(!affine) {
                            const bool flip_v = attr01 & (1 << 29);
                            drawer_state.flip_h = attr01 & (1 << 28);
                            drawer_state.texture_x = 0;
                            drawer_state.texture_y = local_y;
                            if(flip_v) drawer_state.texture_y ^= height - 1;
                            oam_fetch.pending_wait = half_width - 2;
                        } else {
                            oam_fetch.initial_local_x = -half_width;
                            oam_fetch.initial_local_y = local_y - half_height;
                            oam_fetch.pending_wait = half_width * 2 - 1;
                            oam_fetch.matrix_address = (((attr01 >> 25) & 31U) * 32U) + 6U;
                        }

                        active = true;

                        if(x < 0) {
                            const int clip = -x & (affine ? ~0 : ~1);
                            drawer_state.draw_x += clip;
                            drawer_state.remaining_pixels -= clip;
                            if(affine) {
                                oam_fetch.pending_wait -= clip;
                                oam_fetch.initial_local_x += clip;
                            } else {
                                oam_fetch.pending_wait -= clip >> 1;
                                drawer_state.texture_x += clip;
                            }
                            if(drawer_state.remaining_pixels <= 0) active = false;
                        }
                    }
                }
            }

            if(active) oam_fetch.step = 1;
            else oam_fetch.index++;
            break;
        }
        case 1: {
            const u16 attr2 = FetchOAM<u16>(cycle, oam_fetch.index * 8U + 4U);
            drawer_state.tile_number = attr2 & 0x3FFU;
            drawer_state.priority = (attr2 >> 10) & 3U;
            drawer_state.palette = attr2 >> 12;
            if(drawer_state.affine) oam_fetch.step = 2;
            else Submit();
            break;
        }
        case 2:
        case 3:
        case 4:
        case 5: {
            drawer_state.matrix[step - 2] = FetchOAM<s16>(cycle, oam_fetch.matrix_address);
            oam_fetch.matrix_address += 8U;

            if(++oam_fetch.step == 6) {
                const int x0 = oam_fetch.initial_local_x;
                const int y0 = oam_fetch.initial_local_y;
                drawer_state.texture_x = (drawer_state.matrix[0] * x0 + drawer_state.matrix[1] * y0) + (drawer_state.width  << 7);
                drawer_state.texture_y = (drawer_state.matrix[2] * x0 + drawer_state.matrix[3] * y0) + (drawer_state.height << 7);
                Submit();
            }
            break;
        }
    }
}

void Pipeline::DrawSpriteFetchVRAM(uint cycle) {
    if(!sprite.drawing) return;

    auto& drawer_state = sprite.drawer_state[sprite.state_rd];

    const int width  = drawer_state.width;
    const int height = drawer_state.height;
    const uint base_tile = drawer_state.tile_number;

    const auto CalculateTileNumber4BPP = [&](int block_x, int block_y) -> uint {
        if(snap_->oam_mapping_1d) {
            return (base_tile + block_y * (static_cast<uint>(width) >> 3) + block_x) & 0x3FFU;
        }
        return ((base_tile + (block_y << 5)) & 0x3E0U) | ((base_tile + block_x) & 0x1FU);
    };

    const auto CalculateTileNumber8BPP = [&](int block_x, int block_y) -> uint {
        if(snap_->oam_mapping_1d) {
            return (base_tile + block_y * (static_cast<uint>(width) >> 2) + (block_x << 1)) & 0x3FFU;
        }
        return ((base_tile + (block_y << 5)) & 0x3E0U) | (((base_tile & ~1) + (block_x << 1)) & 0x1FU);
    };

    const auto CalculateAddress4BPP = [&](uint tile, int tile_x, int tile_y) -> uint {
        return 0x10000U + (tile << 5) + (tile_y << 2) + (tile_x >> 1);
    };

    const auto CalculateAddress8BPP = [&](uint tile, int tile_x, int tile_y) -> uint {
        return 0x10000U + (tile << 5) + (tile_y << 3) + tile_x;
    };

    const auto Plot = [&](int x, uint color) {
        if(x < 0 || x >= 240) return;
        auto& pixel = sprite.buffer_wr[x];
        const bool opaque = color != 0U;
        const auto mode_ds = drawer_state.mode;
        const uint priority = drawer_state.priority;

        if(mode_ds == OBJ_WINDOW && opaque) {
            pixel.window = 1;
        } else if(priority < pixel.priority || pixel.color == 0U) {
            if(opaque) {
                pixel.color = color;
                pixel.alpha = (mode_ds == OBJ_SEMI) ? 1U : 0U;
            }
            pixel.mosaic = drawer_state.mosaic ? 1U : 0U;
            pixel.priority = priority;
        }
    };

    if(drawer_state.affine) {
        if(sprite.oam_fetch.delay_wait) return;

        const int texture_x = drawer_state.texture_x >> 8;
        const int texture_y = drawer_state.texture_y >> 8;

        if(texture_x >= 0 && texture_x < width &&
           texture_y >= 0 && texture_y < height) {
            const int tile_x  = texture_x & 7;
            const int tile_y  = texture_y & 7;
            const int block_x = texture_x >> 3;
            const int block_y = texture_y >> 3;

            uint color_index = 0U;
            if(drawer_state.is_256) {
                const uint tile = CalculateTileNumber8BPP(block_x, block_y);
                color_index = FetchVRAM_OBJ<u8>(cycle, CalculateAddress8BPP(tile, tile_x, tile_y));
            } else {
                const uint tile = CalculateTileNumber4BPP(block_x, block_y);
                const u8 data = FetchVRAM_OBJ<u8>(cycle, CalculateAddress4BPP(tile, tile_x, tile_y));
                if(tile_x & 1U) color_index = data >> 4;
                else color_index = data & 15U;
                if(color_index > 0U) color_index |= drawer_state.palette << 4;
            }
            Plot(drawer_state.draw_x, color_index);
        }

        drawer_state.draw_x++;
        drawer_state.texture_x += drawer_state.matrix[0];
        drawer_state.texture_y += drawer_state.matrix[2];

        if(--drawer_state.remaining_pixels == 0) sprite.drawing = false;
    } else {
        const bool flip_h = drawer_state.flip_h;
        const int texture_x = drawer_state.texture_x ^ (flip_h ? (width - 1) : 0);
        const int texture_y = drawer_state.texture_y;

        const int tile_x  = texture_x & 7 & ~1;
        const int tile_y  = texture_y & 7;
        const int block_x = texture_x >> 3;
        const int block_y = texture_y >> 3;

        uint palette;
        uint color_indices[2] {0, 0};

        if(drawer_state.is_256) {
            const uint tile = CalculateTileNumber8BPP(block_x, block_y);
            const u16 data = FetchVRAM_OBJ<u16>(cycle, CalculateAddress8BPP(tile, tile_x, tile_y));
            if(flip_h) { color_indices[0] = data >> 8; color_indices[1] = data & 0xFFU; }
            else       { color_indices[0] = data & 0xFFU; color_indices[1] = data >> 8; }
            palette = 0U;
        } else {
            const uint tile = CalculateTileNumber4BPP(block_x, block_y);
            const u8 data = FetchVRAM_OBJ<u8>(cycle, CalculateAddress4BPP(tile, tile_x, tile_y));
            if(flip_h) { color_indices[0] = data >> 4; color_indices[1] = data & 15U; }
            else       { color_indices[0] = data & 15U; color_indices[1] = data >> 4; }
            palette = drawer_state.palette << 4;
        }

        for(int i = 0; i < 2; i++) {
            uint color_index = color_indices[i];
            if(color_index > 0U) color_index |= palette;
            Plot(drawer_state.draw_x++, color_index);
        }

        drawer_state.texture_x += 2;
        drawer_state.remaining_pixels -= 2;
        if(drawer_state.remaining_pixels == 0) sprite.drawing = false;
    }
}

void Pipeline::DrawWindow(int cycles) {
    if(window.cycle >= 1024U) return;

    for(int i = 0; i < cycles; i++) {
        if((window.cycle & 3U) == 0U) {
            const uint x = window.cycle >> 2;
            for(int w = 0; w < 2; w++) {
                const auto& winh = snap_->winh[w];
                if(x == static_cast<uint>(winh.min)) window.h_flag[w] = true;
                if(x == static_cast<uint>(winh.max)) window.h_flag[w] = false;
                if(x < 240) window.buffer[x][w] = window.h_flag[w] && window.v_flag[w];
            }
        }
        if(++window.cycle == 1024U) break;
    }
}

u32 Pipeline::RGB555(u16 rgb555) {
    const uint r = (rgb555 >>  0) & 31U;
    const uint g = (rgb555 >>  5) & 31U;
    const uint b = (rgb555 >> 10) & 31U;
    return 0xFF000000U | (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

u16 Pipeline::Blend(u16 color_a, u16 color_b, int eva, int evb) {
    const int r_a =  (color_a >>  0) & 31;
    const int g_a = ((color_a >>  4) & 62) | (color_a >> 15);
    const int b_a =  (color_a >> 10) & 31;
    const int r_b =  (color_b >>  0) & 31;
    const int g_b = ((color_b >>  4) & 62) | (color_b >> 15);
    const int b_b =  (color_b >> 10) & 31;

    eva = std::min<int>(16, eva);
    evb = std::min<int>(16, evb);

    const int r = std::min<u8>((r_a * eva + r_b * evb + 8) >> 4, 31);
    const int g = std::min<u8>((g_a * eva + g_b * evb + 8) >> 4, 63) >> 1;
    const int b = std::min<u8>((b_a * eva + b_b * evb + 8) >> 4, 31);

    return static_cast<u16>((b << 10) | (g << 5) | r);
}

u16 Pipeline::Brighten(u16 color, int evy) {
    evy = std::min<int>(16, evy);
    int r =  (color >>  0) & 31;
    int g = ((color >>  4) & 62) | (color >> 15);
    int b =  (color >> 10) & 31;
    r += ((31 - r) * evy + 8) >> 4;
    g += ((63 - g) * evy + 8) >> 4;
    b += ((31 - b) * evy + 8) >> 4;
    g >>= 1;
    return static_cast<u16>((b << 10) | (g << 5) | r);
}

u16 Pipeline::Darken(u16 color, int evy) {
    evy = std::min<int>(16, evy);
    int r =  (color >>  0) & 31;
    int g = ((color >>  4) & 62) | (color >> 15);
    int b =  (color >> 10) & 31;
    r -= (r * evy + 7) >> 4;
    g -= (g * evy + 7) >> 4;
    b -= (b * evy + 7) >> 4;
    g >>= 1;
    return static_cast<u16>((b << 10) | (g << 5) | r);
}

void Pipeline::DrawMergeImpl(int cycles) {
    static constexpr int k_min_max_bg[8][2] {
        {0,  3}, {0,  2}, {2,  3}, {2,  2},
        {2,  2}, {2,  2}, {0, -1}, {0, -1},
    };

    const int mode = snap_->mode;
    const int min_bg = k_min_max_bg[mode][0];
    const int max_bg = k_min_max_bg[mode][1];

    const u16 latched_dispcnt_and_current_dispcnt = snap_->dispcnt_latch_0 & snap_->dispcnt_hword;

    int bg_list[4];
    int bg_count = 0;

    for(int priority = 0; priority <= 3; priority++) {
        for(int id = min_bg; id <= max_bg; id++) {
            if(snap_->bgcnt[id].priority == priority && (latched_dispcnt_and_current_dispcnt & (256U << id))) {
                bg_list[bg_count++] = id;
            }
        }
    }

    const bool enable_obj = latched_dispcnt_and_current_dispcnt & (256U << LAYER_OBJ);
    const bool enable_win0 = snap_->enable[ENABLE_WIN0];
    const bool enable_win1 = snap_->enable[ENABLE_WIN1];
    const bool enable_objwin = snap_->enable[ENABLE_OBJWIN] && enable_obj;
    const bool have_windows = enable_win0 || enable_win1 || enable_objwin;

    const int* win_layer_enable = nullptr;

    auto layers = merge.layers;
    auto colors = merge.colors;

    for(int i = 0; i < cycles; i++) {
        const int cycle = static_cast<int>(merge.cycle) - 46;

        if(cycle < 0) {
            merge.cycle++;
            continue;
        }

        const uint x = static_cast<uint>(cycle) >> 2;

        if(have_windows) {
            if(enable_win0 && window.buffer[x][0]) {
                win_layer_enable = snap_->winin_enable[0];
            } else if(enable_win1 && window.buffer[x][1]) {
                win_layer_enable = snap_->winin_enable[1];
            } else if(enable_objwin && sprite.buffer_rd[x].window) {
                win_layer_enable = snap_->winout_enable[1];
            } else {
                win_layer_enable = snap_->winout_enable[0];
            }
        }

        const int phase = cycle & 3;

        if(phase == 0) {
            merge.forced_blank = ForcedBlank();

            if(!merge.forced_blank) {
                uint priorities[2] {3U, 3U};

                layers[0] = LAYER_BD;
                layers[1] = LAYER_BD;
                colors[0] = 0U;
                colors[1] = 0U;

                int bg_list_index = 0;

                for(int j = 0; j < 2; j++) {
                    while(bg_list_index < bg_count) {
                        const int bg_id = bg_list[bg_list_index];
                        bg_list_index++;

                        if(!have_windows || win_layer_enable[bg_id]) {
                            const auto& bgcnt = snap_->bgcnt[bg_id];
                            const uint mx = x - (bgcnt.mosaic_enable ? merge.mosaic_x[0] : 0U);
                            const u32 bg_color = bg.buffer[mx][bg_id];

                            if(bg_color != 0U) {
                                layers[j] = bg_id;
                                colors[j] = bg_color;
                                priorities[j] = static_cast<uint>(bgcnt.priority);
                                break;
                            }
                        }
                    }
                }

                merge.force_alpha_blend = false;

                const auto current_sprite_pixel = enable_obj ? sprite.buffer_rd[x] : Sprite::Pixel{0U};
                if(
                    !current_sprite_pixel.mosaic || !merge.sprite_pixel_latch.mosaic ||
                    current_sprite_pixel.priority < merge.sprite_pixel_latch.priority ||
                    merge.mosaic_x[1] == 0U
                ) {
                    merge.sprite_pixel_latch = current_sprite_pixel;
                }

                if(enable_obj && (!have_windows || win_layer_enable[LAYER_OBJ])) {
                    const auto pixel = merge.sprite_pixel_latch;
                    if(pixel.color != 0U) {
                        if(pixel.priority <= priorities[0]) {
                            layers[1] = layers[0];
                            colors[1] = colors[0];
                            layers[0] = LAYER_OBJ;
                            colors[0] = pixel.color | 256U;
                            merge.force_alpha_blend = pixel.alpha;
                        } else if(pixel.priority <= priorities[1]) {
                            layers[1] = LAYER_OBJ;
                            colors[1] = pixel.color | 256U;
                        }
                    }
                }

                if((colors[0] & 0x8000'0000) == 0) {
                    colors[0] = FetchPRAM(merge.cycle, colors[0] << 1);
                }
            } else {
                colors[0] = 0x7FFFU;
            }
        } else if(phase == 2) {
            if(!merge.forced_blank) {
                const bool have_src = snap_->bld_targets[1][layers[1]];

                if(merge.force_alpha_blend && have_src) {
                    if((colors[1] & 0x8000'0000) == 0) {
                        colors[1] = FetchPRAM(merge.cycle, colors[1] << 1);
                    }
                    colors[0] = Blend(colors[0], colors[1], snap_->eva, snap_->evb);
                } else if(!have_windows || win_layer_enable[LAYER_SFX]) {
                    const bool have_dst = snap_->bld_targets[0][layers[0]];

                    switch(snap_->bldcnt_sfx) {
                        case 1: { // SFX_BLEND
                            if(have_dst && have_src) {
                                if((colors[1] & 0x8000'0000) == 0) {
                                    colors[1] = FetchPRAM(merge.cycle, colors[1] << 1);
                                }
                                colors[0] = Blend(colors[0], colors[1], snap_->eva, snap_->evb);
                            }
                            break;
                        }
                        case 2: { // SFX_BRIGHTEN
                            if(have_dst) colors[0] = Brighten(colors[0], snap_->evy);
                            break;
                        }
                        case 3: { // SFX_DARKEN
                            if(have_dst) colors[0] = Darken(colors[0], snap_->evy);
                            break;
                        }
                    }
                }
            }

            if(x & 1) {
                u16 color_l = merge.color_l;
                u16 color_r = colors[0];

                if(snap_->greenswap & 1) {
                    const u16 mask = 31U << 5;
                    u16 g_l = color_l & mask;
                    u16 g_r = color_r & mask;
                    color_l = (color_l & ~mask) | g_r;
                    color_r = (color_r & ~mask) | g_l;
                }

                u32* out = &output_[vcount_ * 240 + (x & ~1)];
                out[0] = RGB555(color_l);
                out[1] = RGB555(color_r);
            } else {
                merge.color_l = colors[0];
            }

            if(++merge.mosaic_x[0] == static_cast<uint>(snap_->mosaic_bg_size_x)) {
                merge.mosaic_x[0] = 0U;
            }
            if(++merge.mosaic_x[1] == static_cast<uint>(snap_->mosaic_obj_size_x)) {
                merge.mosaic_x[1] = 0U;
            }
        }

        if(++merge.cycle == 1006U) break;
    }
}

void Pipeline::RenderFrame(const nba::PpuFrameSnapshot& snapshot, u32* output) {
    snap_ = &snapshot;
    output_ = output;

    // Per-frame state init
    sprite.buffer_rd = sprite.buffer[0];
    sprite.buffer_wr = sprite.buffer[1];
    mosaic_bg_counter_y_ = 0;
    mosaic_obj_counter_y_ = 0;
    vram_bg_latch_ = 0;

    // Window vertical flags carry across scanlines; reset for new frame.
    window.v_flag[0] = false;
    window.v_flag[1] = false;
    window.h_flag[0] = false;
    window.h_flag[1] = false;

    merge.color_l = 0;
    merge.sprite_pixel_latch.data = 0;

    for(vcount_ = 0; vcount_ < kScreenHeight; ++vcount_) {
        InitSprite();
        DrawSpriteImpl(static_cast<int>(sprite.latch_cycle_limit));
        std::swap(sprite.buffer_rd, sprite.buffer_wr);

        InitBackground();
        InitWindow();
        InitMerge();

        DrawBackground();
        DrawWindow(1232);
        DrawMergeImpl(1006);
    }
}

} // namespace ppu_renderer
