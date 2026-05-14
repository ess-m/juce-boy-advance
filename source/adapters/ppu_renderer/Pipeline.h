//
// forked from nba's ppu (ppu.hpp state structs + Init/Draw methods).
// runs off the audio thread from a snapshot. no bus timestamps.
//
// Copyright (C) 2025 fleroviux — GPLv3+, see LICENSE.
//

#pragma once

#include <nba/common/compiler.hpp>
#include <nba/common/punning.hpp>
#include <nba/device/frame_renderer.hpp>
#include <nba/integer.hpp>

#include <algorithm>
#include <cstring>

namespace ppu_renderer {

using ::u8;
using ::u16;
using ::u32;
using ::u64;
using ::s8;
using ::s16;
using ::s32;
using ::s64;
using nba::read;

class Pipeline {
public:
    static constexpr int kScreenWidth = 240;
    static constexpr int kScreenHeight = 160;

    // Render a full frame from `snapshot` into `output` (240*160 ARGB samples).
    void RenderFrame(const nba::PpuFrameSnapshot& snapshot, u32* output);

private:
    enum ObjAttribute {
        OBJ_IS_ALPHA  = 1,
        OBJ_IS_WINDOW = 2
    };

    enum ObjectMode {
        OBJ_NORMAL = 0,
        OBJ_SEMI   = 1,
        OBJ_WINDOW = 2,
        OBJ_PROHIBITED = 3
    };

    enum Layer {
        LAYER_BG0 = 0,
        LAYER_BG1 = 1,
        LAYER_BG2 = 2,
        LAYER_BG3 = 3,
        LAYER_OBJ = 4,
        LAYER_SFX = 5,
        LAYER_BD  = 5
    };

    enum Enable {
        ENABLE_BG0 = 0,
        ENABLE_BG1 = 1,
        ENABLE_BG2 = 2,
        ENABLE_BG3 = 3,
        ENABLE_OBJ = 4,
        ENABLE_WIN0 = 5,
        ENABLE_WIN1 = 6,
        ENABLE_OBJWIN = 7
    };

    // ──── State structs (mirror of nba::core::PPU's per-frame mutable state)

    struct Background {
        uint cycle;

        struct Text {
            int fetches;
            struct Tile {
                u32 address;
                uint palette;
                bool flip_x;
            } tile;
            struct PISO {
                u16 data;
                int remaining;
            } piso;
        } text[4];

        struct Affine {
            s32 x;
            s32 y;
            bool out_of_bounds;
            u16 tile_address;
        } affine[2];

        u32 buffer[240][4];
    } bg {};

    struct Sprite {
        uint cycle;
        uint vcount;
        int mosaic_y;

        struct {
            uint index;
            int  step;
            int  wait;
            int  pending_wait;
            bool delay_wait;
            int  initial_local_x;
            int  initial_local_y;
            uint matrix_address;
        } oam_fetch;

        bool drawing;

        struct {
            int width;
            int height;
            int mode;
            bool mosaic;
            bool affine;

            int draw_x;
            int remaining_pixels;

            s16 matrix[4];

            uint tile_number;
            uint priority;
            uint palette;
            bool flip_h;
            bool is_256;

            int texture_x;
            int texture_y;
        } drawer_state[2];

        int state_rd;
        int state_wr;

        union Pixel {
            struct {
                u8 color : 8;
                unsigned priority : 2;
                unsigned alpha  : 1;
                unsigned window : 1;
                unsigned mosaic : 1;
            };
            u16 data;
        };

        Pixel buffer[2][240];
        Pixel* buffer_rd;
        Pixel* buffer_wr;

        uint latch_cycle_limit;
    } sprite {};

    struct Window {
        uint cycle;
        bool v_flag[2];
        bool h_flag[2];
        bool buffer[240][2];
    } window {};

    struct Merge {
        uint cycle;
        uint mosaic_x[2];
        int layers[2];
        bool force_alpha_blend;
        u32 colors[2];
        u16 color_l;
        bool forced_blank;
        Sprite::Pixel sprite_pixel_latch;
    } merge {};

    // Per-frame state that persists across scanlines (affine BG current points).
    s32 bgx_current_[2] {};
    s32 bgy_current_[2] {};
    int mosaic_bg_counter_y_ = 0;
    int mosaic_obj_counter_y_ = 0;

    // Current snapshot + output buffer (set by RenderFrame, read by all stages).
    const nba::PpuFrameSnapshot* snap_ = nullptr;
    u32* output_ = nullptr;
    int vcount_ = 0;            // current scanline being rendered
    u16 vram_bg_latch_ = 0;

    // ──── Accessors (read from snapshot, no timestamp writes)

    bool ALWAYS_INLINE ForcedBlank() const {
        return (snap_->dispcnt_latch_0 | snap_->dispcnt_hword) & 0x80U;
    }

    u32 ALWAYS_INLINE GetSpriteVRAMBoundary() const {
        return snap_->mode >= 3 ? 0x14000 : 0x10000;
    }

    u16 ALWAYS_INLINE FetchPRAM(uint /*cycle*/, uint address) {
        return read<u16>(snap_->pram, address);
    }

    template<typename T>
    T ALWAYS_INLINE FetchVRAM_BG(uint /*cycle*/, uint address) {
        if(ForcedBlank()) {
            return T{0};
        }
        if(likely(address < GetSpriteVRAMBoundary())) {
            vram_bg_latch_ = read<u16>(snap_->vram, address & ~1U);
            return read<T>(snap_->vram, address);
        }
        return read<T>(&vram_bg_latch_, address & 1U);
    }

    template<typename T>
    T ALWAYS_INLINE FetchVRAM_OBJ(uint /*cycle*/, uint address) {
        if(likely(address >= GetSpriteVRAMBoundary())) {
            return read<T>(snap_->vram, address);
        }
        return T{0};
    }

    template<typename T>
    T ALWAYS_INLINE FetchOAM(uint /*cycle*/, uint address) {
        return read<T>(snap_->oam, address);
    }

    // ──── Init / Draw methods

    void InitBackground();
    void DrawBackground();
    template<int mode> void DrawBackgroundImpl(int cycles);

    void InitSprite();
    void DrawSpriteImpl(int cycles);
    void DrawSpriteFetchOAM(uint cycle);
    void DrawSpriteFetchVRAM(uint cycle);

    void InitWindow();
    void DrawWindow(int cycles);

    void InitMerge();
    void DrawMergeImpl(int cycles);

    static u16 Blend(u16 color_a, u16 color_b, int eva, int evb);
    static u16 Brighten(u16 color, int evy);
    static u16 Darken(u16 color, int evy);
    static u32 RGB555(u16 rgb555);

    // RenderMode*BG inlines (included from BackgroundModes.inl)
    void RenderMode0BG(uint id, uint cycle);
    void RenderMode2BG(uint id, uint cycle);
    void RenderMode3BG(uint cycle);
    void RenderMode4BG(uint cycle);
    void RenderMode5BG(uint cycle);
};

} // namespace ppu_renderer
