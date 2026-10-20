// Generado automaticamente. Audio 16000 Hz, mono, 16 bits.
#ifndef AUDIO_DATA_H
#define AUDIO_DATA_H

#define AUDIO_RATE 16000

extern const unsigned int sfx_swing_light_data[];
#define SFX_SWING_LIGHT_LEN 4616   /* muestras */
extern const unsigned int sfx_swing_heavy_data[];
#define SFX_SWING_HEAVY_LEN 3780   /* muestras */
extern const unsigned int sfx_hit_light_data[];
#define SFX_HIT_LIGHT_LEN 7540   /* muestras */
extern const unsigned int sfx_hit_heavy_data[];
#define SFX_HIT_HEAVY_LEN 9212   /* muestras */
extern const unsigned int sfx_jump_data[];
#define SFX_JUMP_LEN 11200   /* muestras */
extern const unsigned int sfx_magic_data[];
#define SFX_MAGIC_LEN 20800   /* muestras */
extern const unsigned int sfx_block_data[];
#define SFX_BLOCK_LEN 9212   /* muestras */
extern const unsigned int sfx_item_data[];
#define SFX_ITEM_LEN 9212   /* muestras */
extern const unsigned int sfx_coin_data[];
#define SFX_COIN_LEN 6288   /* muestras */
extern const unsigned int sfx_select_data[];
#define SFX_SELECT_LEN 12800   /* muestras */
extern const unsigned int sfx_move_data[];
#define SFX_MOVE_LEN 1690   /* muestras */
extern const unsigned int music_menu_data[];
#define MUSIC_MENU_LEN 1230490   /* muestras */
extern const unsigned int music_game_data[];
#define MUSIC_GAME_LEN 983894   /* muestras */

enum {
    SFX_SWING_LIGHT = 0,
    SFX_SWING_HEAVY = 1,
    SFX_HIT_LIGHT = 2,
    SFX_HIT_HEAVY = 3,
    SFX_JUMP = 4,
    SFX_MAGIC = 5,
    SFX_BLOCK = 6,
    SFX_ITEM = 7,
    SFX_COIN = 8,
    SFX_SELECT = 9,
    SFX_MOVE = 10,
    SFX_COUNT = 11
};
extern const unsigned int * const sfx_data[SFX_COUNT];
extern const int sfx_len[SFX_COUNT];

#endif
