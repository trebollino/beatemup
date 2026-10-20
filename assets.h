#ifndef ASSETS_H
#define ASSETS_H

extern const unsigned int bg1_data[];
extern const unsigned int bg2_data[];
extern const unsigned int bg3_data[];
#define BG_TEX_W 320
#define BG_TEX_H 224

extern const unsigned int logo_data[];
#define LOGO_W 344
#define LOGO_H 80
#define LOGO_X 12
#define LOGO_Y 9
#define LOGO_CX 182.0f
#define LOGO_CY 46.0f

extern const unsigned int btn0_data[];
#define BTN0_W 312
#define BTN0_H 72
#define BTN0_X 10
#define BTN0_Y 154
#define BTN0_CX 163.5f
#define BTN0_CY 188.5f
extern const unsigned int btn0_glow_data[];
#define BTN0_GLOW_W 176
#define BTN0_GLOW_H 56
#define BTN0_GLOW_X -6
#define BTN0_GLOW_Y 138

extern const unsigned int btn1_data[];
#define BTN1_W 184
#define BTN1_H 72
#define BTN1_X 10
#define BTN1_Y 229
#define BTN1_CX 98.5f
#define BTN1_CY 261.5f
extern const unsigned int btn1_glow_data[];
#define BTN1_GLOW_W 112
#define BTN1_GLOW_H 56
#define BTN1_GLOW_X -6
#define BTN1_GLOW_Y 213

extern const unsigned int btn2_data[];
#define BTN2_W 96
#define BTN2_H 72
#define BTN2_X 183
#define BTN2_Y 229
#define BTN2_CX 228.0f
#define BTN2_CY 261.5f
extern const unsigned int btn2_glow_data[];
#define BTN2_GLOW_W 64
#define BTN2_GLOW_H 56
#define BTN2_GLOW_X 167
#define BTN2_GLOW_Y 213

typedef struct { short x, y, w, h, ox, oy; } SpriteFrame;
extern const unsigned int sprites_data[];
#define SPRITES_W 512
#define SPRITES_H 656
extern const SpriteFrame playerFrames[13];
extern const SpriteFrame player2Frames[13];
extern const SpriteFrame npcFrames[8];
extern const SpriteFrame barbFrames[7];
extern const SpriteFrame thiefFrames[7];
extern const SpriteFrame iconFrames[5];
#define PF_IDLE 0
#define PF_WALK0 1
#define PF_WALK1 2
#define PF_WALK2 3
#define PF_JUMP 4
#define PF_L0 5
#define PF_L1 6
#define PF_L2 7
#define PF_H0 8
#define PF_H1 9
#define PF_H2 10
#define PF_BLOCK 11
#define PF_HURT 12
#define NF_IDLE 0
#define NF_WALK0 1
#define NF_WALK1 2
#define NF_WALK2 3
#define NF_L0 4
#define NF_L1 5
#define NF_L2 6
#define NF_HURT 7
#define EF_IDLE 0
#define EF_WALK0 1
#define EF_WALK1 2
#define EF_WALK2 3
#define EF_ATKW 4
#define EF_ATKS 5
#define EF_HURT 6
#define I_HEAD_GREEN 0
#define I_HEAD_RED 1
#define I_POTION_RED 2
#define I_POTION_BLUE 3
#define I_FOOD 4

extern const unsigned int font_data[];
extern const unsigned char font_adv[95];
#define FONT_W 352
#define FONT_H 160
#define FONT_CELL_W 22
#define FONT_CELL_H 26
#define FONT_COLS 16
#define FONT_FIRST 32

#endif
