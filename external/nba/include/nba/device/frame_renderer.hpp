#pragma once

#include <nba/integer.hpp>

namespace nba {

struct PpuFrameSnapshot {
  u8 vram[0x18000]; // 96KB
  u8 pram[0x400]; // 1KB
  u8 oam[0x400]; // 1KB

  // DISPCNT
  u16 dispcnt_hword;
  u16 dispcnt_latch_0; // latched DISPCNT
  int mode;
  int frame_select; // dispcnt.frame (mode 4/5)
  int hblank_oam_access;
  int oam_mapping_1d;
  int forced_blank;
  int enable[8]; // BG0, BG1, BG2, BG3, OBJ, WIN0, WIN1, OBJWIN

  u16 greenswap;

  // BGCNT 0..3
  struct BgCtrl {
    int priority;
    int tile_block;
    int mosaic_enable;
    int full_palette;
    int map_block;
    int wraparound;
    int size;
  } bgcnt[4];

  u16 bghofs[4];
  u16 bgvofs[4];

  s32 bgx_initial[2];
  s32 bgy_initial[2];

  s16 bgpa[2];
  s16 bgpb[2];
  s16 bgpc[2];
  s16 bgpd[2];

  struct WinRange {
    int min;
    int max;
  } winh[2], winv[2];

  // win_layer_enable[selector][layer]: selector 0=WIN0, 1=WIN1; layers 0..5
  int winin_enable[2][6];
  // winout_enable[selector][layer]: selector 0=outside, 1=OBJ window
  int winout_enable[2][6];

  // mosaic
  int mosaic_bg_size_x;
  int mosaic_bg_size_y;
  int mosaic_obj_size_x;
  int mosaic_obj_size_y;

  // blend
  int bldcnt_sfx;             // 0=none, 1=alpha, 2=brighten, 3=darken
  int bld_targets[2][6];      // [0]=src, [1]=dst, layers 0..5
  int eva;
  int evb;
  int evy;
};

struct FrameRenderer {
  virtual ~FrameRenderer() = default;
  virtual bool IsActive() const { return false; }
  virtual void OnFrameSnapshot(const PpuFrameSnapshot& snapshot) = 0;
};

struct NullFrameRenderer final : FrameRenderer {
  void OnFrameSnapshot(const PpuFrameSnapshot&) override {}
};

} // namespace nba
