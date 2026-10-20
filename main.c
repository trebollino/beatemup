// castle crasher por trebollino (Etapas 1 y 2)
// Controles (estilo Castle Crashers):
//   Stick izq / D-pad = mover | X = saltar | Cuadrado = ataque suave
//   Triangulo = ataque agresivo | Circulo = usar objeto | L2 = bloquear
//   R2 = magia | L1/R1 = seleccionar objeto | START = pausa | SELECT = reiniciar
// Menu principal con fondos que se deslizan, botones con glow y navegacion con cruceta/stick.
// Usa material de Castle Crashers: caballero armado con sus piezas, fuente, musica y efectos.
// En el menu: cruceta/stick = elegir | X o START = aceptar
// En el juego: START = pausa | en pausa, CIRCULO = volver al menu
// 1 o 2 jugadores (boton de la llave en el menu). Caballeros grises = aliados (IA).

#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <libpad.h>
#include <math.h>
#include "assets.h"
#include "audio_data.h"
#ifdef USE_AUDIO
#include <audsrv.h>
#endif

#define SCREEN_W 640
#define SCREEN_H 448

// Zona caminable (profundidad 2.5D)
#define GROUND_TOP    300
#define GROUND_BOTTOM 420
#define WORLD_LEFT    20
#define WORLD_RIGHT   620

#define GRAVITY 0.8f

static char padBuf[256] __attribute__((aligned(64)));
static char padBuf2[256] __attribute__((aligned(64)));


// =====================================================================
//  AUDIO: musica y efectos (se desactiva con USE_AUDIO = 0 en el Makefile)
// =====================================================================
#ifdef USE_AUDIO
extern unsigned char audsrv_irx[];
extern unsigned int size_audsrv_irx;
extern unsigned char freesd_irx[];
extern unsigned int size_freesd_irx;
static const char *audioStatus = "AUDIO: NO INICIADO";

#define MAX_VOICES 6
#define MIX_MAX 1024
#define AUDIO_TARGET_BYTES 4096

typedef struct { const short *data; int len; int pos; } Voice;
static Voice voices[MAX_VOICES];
static int audioOk = 0, audioCap = 0;
static const short *curMusic = NULL;
static int curMusicLen = 0, musicPos = 0;

static void initAudio(void) {
    int ret = 0;
    int id = SifExecModuleBuffer(freesd_irx, size_freesd_irx, 0, NULL, &ret);
    if (id < 0) { audioStatus = "AUDIO: ERROR FREESD"; return; }
    id = SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, &ret);
    if (id < 0) { audioStatus = "AUDIO: ERROR AUDSRV"; return; }

    // esperar al servidor de audio, pero con limite de tiempo:
    // si nunca aparece, el juego sigue sin sonido en vez de colgarse
    SifRpcClientData_t cd;
    int found = 0;
    for (int i = 0; i < 200 && !found; i++) {
        if (SifBindRpc(&cd, 0x870884, 0) >= 0 && cd.server != 0) found = 1;
        else { for (volatile int d = 0; d < 1000000; d++) { } }
    }
    if (!found) { audioStatus = "AUDIO: SIN SERVIDOR"; return; }

    if (audsrv_init() != 0) { audioStatus = "AUDIO: ERROR INIT"; return; }
    struct audsrv_fmt_t fmt;
    fmt.bits = 16;
    fmt.freq = AUDIO_RATE;
    fmt.channels = 1;
    if (audsrv_set_format(&fmt) != AUDSRV_ERR_NOERROR) { audioStatus = "AUDIO: ERROR FORMATO"; return; }
    audsrv_set_volume(MAX_VOLUME);
    audioCap = audsrv_available();
    audioOk = 1;
    audioStatus = "AUDIO: OK";
}

static void startMenuMusic(void) { curMusic = (const short *)music_menu_data; curMusicLen = MUSIC_MENU_LEN; musicPos = 0; }
static void startGameMusic(void) { curMusic = (const short *)music_game_data; curMusicLen = MUSIC_GAME_LEN; musicPos = 0; }

static void playSfx(int id) {
    if (!audioOk) return;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].data) {
            voices[i].data = (const short *)sfx_data[id];
            voices[i].len = sfx_len[id];
            voices[i].pos = 0;
            return;
        }
    }
}

// mezcla musica + efectos y los manda a la placa de sonido
static void audioUpdate(void) {
    if (!audioOk) return;
    int queued = audioCap - audsrv_available();
    int want = AUDIO_TARGET_BYTES - queued;
    if (want < 256) return;
    int n = want / 2;
    if (n > MIX_MAX) n = MIX_MAX;
    static short mix[MIX_MAX] __attribute__((aligned(16)));
    for (int i = 0; i < n; i++) {
        int sm = 0;
        if (curMusic) {
            sm = (curMusic[musicPos] * 6) / 10;
            if (++musicPos >= curMusicLen) musicPos = 0;
        }
        for (int v = 0; v < MAX_VOICES; v++) {
            if (voices[v].data) {
                sm += voices[v].data[voices[v].pos];
                if (++voices[v].pos >= voices[v].len) voices[v].data = NULL;
            }
        }
        if (sm > 32767) sm = 32767;
        if (sm < -32768) sm = -32768;
        mix[i] = (short)sm;
    }
    audsrv_play_audio((const char *)mix, n * 2);
}
#else
static const char *audioStatus = "SIN AUDIO";
enum { MAX_VOICES_DUMMY = 0 };
static void initAudio(void) {}
static void startMenuMusic(void) {}
static void startGameMusic(void) {}
static void playSfx(int id) { (void)id; }
static void audioUpdate(void) {}
#endif

static u64 COL(int r, int g, int b) {
    return GS_SETREG_RGBAQ(r, g, b, 0x80, 0x00);
}
static u64 COLA(int r, int g, int b, int a) {
    return GS_SETREG_RGBAQ(r, g, b, a, 0x00);
}

// Formato de color para sprites con textura (0x80 = 1.0)
#define TEXCOL(r, g, b, a) GS_SETREG_RGBAQ((r), (g), (b), (a), 0x00)

static GSTEXTURE texBg[3], texLogo, texBtn[3], texGlow[3];
static GSTEXTURE texSprites, texFont;

static int texOk(GSTEXTURE *t) {
    return t->Vram != 0 && t->Vram != GSKIT_ALLOC_ERROR;
}

static int textWidth(const char *str, float sc) {
    float w = 0;
    for (; *str; str++) {
        int c = (unsigned char)*str;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < FONT_FIRST || c > FONT_FIRST + 94) c = '?';
        w += font_adv[c - FONT_FIRST] * sc;
    }
    return (int)w;
}

// texto con la fuente de Castle Crashers (solo mayusculas, sin tildes ni enie)
static void drawText(GSGLOBAL *gs, const char *str, float x, float y, float sc, int r, int g, int b) {
    if (!texOk(&texFont)) return;
    u64 col = TEXCOL(r / 2, g / 2, b / 2, 128);
    for (; *str; str++) {
        int c = (unsigned char)*str;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < FONT_FIRST || c > FONT_FIRST + 94) c = '?';
        int k = c - FONT_FIRST;
        float u = (float)((k % FONT_COLS) * FONT_CELL_W);
        float v = (float)((k / FONT_COLS) * FONT_CELL_H);
        if (c != ' ')
            gsKit_prim_sprite_texture(gs, &texFont, x, y, u, v,
                                      x + FONT_CELL_W * sc, y + FONT_CELL_H * sc,
                                      u + FONT_CELL_W, v + FONT_CELL_H, 8, col);
        x += font_adv[k] * sc;
    }
}

static void drawTextCentered(GSGLOBAL *gs, const char *str, float cx, float y, float sc, int r, int g, int b) {
    drawText(gs, str, cx - textWidth(str, sc) / 2.0f, y, sc, r, g, b);
}

// dibuja un cuadro del atlas de sprites; (px,py) = punto de los pies
static void drawSpriteFrame(GSGLOBAL *gs, const SpriteFrame *f, float px, float py,
                            float s, int flip, u64 col) {
    float x1, x2;
    float u1 = (float)f->x, u2 = (float)(f->x + f->w);
    if (!flip) {
        x1 = px + f->ox * s;
        x2 = x1 + f->w * s;
    } else {
        x2 = px - f->ox * s;
        x1 = x2 - f->w * s;
        float tmp = u1; u1 = u2; u2 = tmp;
    }
    float y1 = py + f->oy * s;
    float y2 = y1 + f->h * s;
    gsKit_prim_sprite_texture(gs, &texSprites, x1, y1, u1, (float)f->y,
                              x2, y2, u2, (float)(f->y + f->h), 3, col);
}

static void drawIcon(GSGLOBAL *gs, int idx, float x, float y) {
    if (!texOk(&texSprites)) return;
    drawSpriteFrame(gs, &iconFrames[idx], x, y, 1.0f, 0, TEXCOL(128, 128, 128, 128));
}

static void initPad(void) {
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    padPortOpen(1, 0, padBuf2);        // control 2 (opcional)
    // esperar a que el control 1 este listo
    int state;
    do {
        state = padGetState(0, 0);
    } while (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1);

    // pedir modo analogico (DualShock) y esperar a que se aplique
    padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
    while (padGetReqState(0, 0) == PAD_RSTAT_BUSY) { }
    do {
        state = padGetState(0, 0);
    } while (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1);

    // control 2: si esta conectado, tambien en modo analogico (sin esperar)
    state = padGetState(1, 0);
    if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1)
        padSetMainMode(1, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
}

// =====================================================================
//  JUEGO: luchadores, IA y oleadas
// =====================================================================

enum { K_PLAYER, K_NPC, K_BARB, K_THIEF };

typedef struct {
    int   active;
    int   kind;
    int   team;          // 0 = aliados (jugadores y caballeros grises), 1 = enemigos
    int   pidx;          // 0 / 1 = jugador 1 / 2, -1 = no es jugador
    float x, y, z, vz;   // x horizontal, y profundidad (en el suelo), z altura (salto)
    int   facing;        // 1 derecha, -1 izquierda
    int   hp, maxhp;
    int   attackTimer, attackType, hitDone;
    int   hurtTimer;
    float knockX;
    int   moving;
    int   cooldown;      // IA: espera entre ataques
    int   deadTimer;     // >0: cayendo (parpadea y desaparece)
    int   blocking, wasBlocking;
    float mana;
    int   magicTimer;
    int   selectedItem;
    int   itemCount[3];  // 0 pocion vida, 1 pocion magia, 2 comida
} Fighter;

#define MAX_F 16
static Fighter F[MAX_F];          // 0 y 1 = jugadores, el resto = caballeros grises y enemigos
static int numPlayers = 1;        // 1 o 2 jugadores (se cambia en el menu)
static int wave = 0, waveTimer = 0, waveBanner = 0, kills = 0, gameOver = 0;
static int paused = 0;
static int walkTick = 0, gameBg = 0, soonTimer = 0;

// ataques: duracion, ventana de golpe (hitLo..hitHi), dano, alcance, empuje
typedef struct { int dur, hitLo, hitHi, dmg; float reach, knock; } AtkDef;
static const AtkDef ATK[6] = {
    {  0,  0,  0,  0,   0,  0 },
    { 20,  9, 15, 10,  90,  8 },   // 1: suave (jugador)
    { 34, 15, 25, 20, 105, 16 },   // 2: agresivo (jugador)
    { 22,  9, 15,  8,  90,  7 },   // 3: caballero gris
    { 36, 14, 22, 14,  95, 10 },   // 4: barbaro
    { 26, 10, 16,  8, 100,  7 },   // 5: ladron
};

static int isAlive(Fighter *f) { return f->active && f->hp > 0 && f->deadTimer == 0; }

static void clampFighter(Fighter *f) {
    if (f->x < WORLD_LEFT)  f->x = WORLD_LEFT;
    if (f->x > WORLD_RIGHT) f->x = WORLD_RIGHT;
    if (f->y < GROUND_TOP)    f->y = GROUND_TOP;
    if (f->y > GROUND_BOTTOM) f->y = GROUND_BOTTOM;
}

static void initPlayer(int idx, float x, float y) {
    Fighter *f = &F[idx];
    memset(f, 0, sizeof(*f));
    f->active = 1; f->kind = K_PLAYER; f->team = 0; f->pidx = idx;
    f->x = x; f->y = y; f->facing = 1;
    f->hp = f->maxhp = 100;
    f->mana = 100;
    f->itemCount[0] = f->itemCount[1] = f->itemCount[2] = 3;
}

static void spawnFighter(int kind, float x, float y) {
    int i;
    for (i = 2; i < MAX_F; i++) if (!F[i].active) break;
    if (i >= MAX_F) return;
    Fighter *f = &F[i];
    memset(f, 0, sizeof(*f));
    f->active = 1; f->kind = kind; f->pidx = -1;
    f->x = x; f->y = y;
    f->team = (kind == K_NPC) ? 0 : 1;
    f->facing = (kind == K_NPC) ? 1 : -1;
    f->hp = f->maxhp = (kind == K_NPC) ? 90 : (kind == K_BARB) ? 70 : 40;
    f->cooldown = 30 + rand() % 40;
}

static void resetGame(void) {
    memset(F, 0, sizeof(F));
    initPlayer(0, 150, 360);
    if (numPlayers == 2) initPlayer(1, 100, 395);
    spawnFighter(K_NPC, 60, 330);      // dos caballeros grises aliados
    spawnFighter(K_NPC, 80, 410);
    wave = 0; waveTimer = 90; waveBanner = 0; kills = 0; gameOver = 0; paused = 0;
}

static int countTeam(int team, int onlyNpc) {
    int n = 0;
    for (int i = 0; i < MAX_F; i++)
        if (isAlive(&F[i]) && F[i].team == team && (!onlyNpc || F[i].kind == K_NPC)) n++;
    return n;
}

// un luchador recibe un golpe de otro
static void hurtFighter(Fighter *t, Fighter *src, int dmg, float knock) {
    if (!isAlive(t)) return;
    int dir = (t->x >= src->x) ? 1 : -1;
    if (t->blocking && t->facing == -dir) {          // bloqueo: mira hacia el atacante
        dmg = dmg / 5;
        t->x += (knock / 3) * dir;
        playSfx(SFX_BLOCK);
    } else {
        t->hurtTimer = 15;
        t->knockX = knock * dir;
        t->attackTimer = 0; t->attackType = 0;       // el golpe interrumpe
        playSfx(dmg >= 14 ? SFX_HIT_HEAVY : SFX_HIT_LIGHT);
    }
    t->hp -= dmg;
    if (t->hp <= 0) {
        t->hp = 0;
        t->deadTimer = 45;
        if (t->team == 1) { kills++; playSfx(SFX_COIN); }
    }
}

// aplica el golpe de 'a' a todos los enemigos que alcance
static void attackHits(Fighter *a) {
    if (a->attackType == 0 || a->hitDone) return;
    const AtkDef *d = &ATK[a->attackType];
    int t = a->attackTimer;
    if (t < d->hitLo || t > d->hitHi) return;
    int hit = 0;
    for (int i = 0; i < MAX_F; i++) {
        Fighter *b = &F[i];
        if (b == a || !isAlive(b) || b->team == a->team) continue;
        float dx = (b->x - a->x) * a->facing;
        float dy = b->y - a->y;
        float dz = a->z - b->z;
        if (dx > -15 && dx < d->reach && dy > -24 && dy < 24 && dz > -40 && dz < 40) {
            hurtFighter(b, a, d->dmg, d->knock);
            hit = 1;
        }
    }
    if (hit) a->hitDone = 1;
}

// ---------------------------------------------------------------------
//  Jugador (con el control correspondiente)
// ---------------------------------------------------------------------
static void updatePlayerF(Fighter *p, u32 pressed, u32 held, int lx, int ly) {
    if (!isAlive(p)) return;

    // gravedad / salto
    if (p->z > 0 || p->vz > 0) {
        p->z += p->vz;
        p->vz -= GRAVITY;
        if (p->z <= 0) { p->z = 0; p->vz = 0; }
    }

    // herido: sin control, solo empuje
    if (p->hurtTimer > 0) {
        p->hurtTimer--;
        p->x += p->knockX;
        p->knockX *= 0.85f;
        p->moving = 0;
        p->blocking = 0;
        clampFighter(p);
        return;
    }

    float speed = 3.0f;
    float dx = 0, dy = 0;

    // bloquear con L2 (en el suelo y sin atacar)
    p->blocking = ((held & PAD_L2) && p->z == 0 && p->attackTimer == 0);
    if (p->blocking && !p->wasBlocking) playSfx(SFX_BLOCK);
    p->wasBlocking = p->blocking;
    p->moving = 0;

    if (held & PAD_LEFT)  dx -= 1;
    if (held & PAD_RIGHT) dx += 1;
    if (held & PAD_UP)    dy -= 1;
    if (held & PAD_DOWN)  dy += 1;

    // stick analogico (centro 128, zona muerta 30)
    if (lx < 98)  dx = -1; else if (lx > 158) dx = 1;
    if (ly < 98)  dy = -1; else if (ly > 158) dy = 1;

    // no moverse mientras golpea o bloquea
    if (p->attackTimer == 0 && !p->blocking) {
        p->moving = (dx != 0 || dy != 0);
        p->x += dx * speed;
        p->y += dy * speed * 0.7f;
        if (dx > 0) p->facing = 1;
        if (dx < 0) p->facing = -1;
    }

    // salto (X)
    if ((pressed & PAD_CROSS) && p->z == 0 && p->attackTimer == 0 && !p->blocking) {
        p->vz = 12.0f;
        playSfx(SFX_JUMP);
    }

    // ataque suave (Cuadrado) y agresivo (Triangulo)
    if (p->attackTimer == 0 && !p->blocking) {
        if (pressed & PAD_SQUARE) {
            p->attackTimer = ATK[1].dur; p->attackType = 1; p->hitDone = 0;
            playSfx(SFX_SWING_LIGHT);
        } else if (pressed & PAD_TRIANGLE) {
            p->attackTimer = ATK[2].dur; p->attackType = 2; p->hitDone = 0;
            playSfx(SFX_SWING_HEAVY);
        }
    }
    if (p->attackTimer > 0) {
        p->attackTimer--;
        attackHits(p);
        if (p->attackTimer == 0) p->attackType = 0;
    }

    // magia (R2): cuesta 50 de mana y pega a todos los enemigos cercanos
    if (p->mana < 100) p->mana += 0.1f;
    if (p->magicTimer > 0) p->magicTimer--;
    if ((pressed & PAD_R2) && p->mana >= 50 && p->magicTimer == 0) {
        p->mana -= 50;
        p->magicTimer = 30;
        playSfx(SFX_MAGIC);
        for (int i = 0; i < MAX_F; i++) {
            Fighter *b = &F[i];
            if (!isAlive(b) || b->team == p->team) continue;
            float ddx = b->x - p->x, ddy = b->y - p->y;
            if (ddx > -240 && ddx < 240 && ddy > -90 && ddy < 90)
                hurtFighter(b, p, 25, 12.0f);
        }
    }

    // seleccionar objeto (L1 / R1) y usar objeto (Circulo)
    if (pressed & PAD_R1) p->selectedItem = (p->selectedItem + 1) % 3;
    if (pressed & PAD_L1) p->selectedItem = (p->selectedItem + 2) % 3;
    if ((pressed & PAD_CIRCLE) && p->itemCount[p->selectedItem] > 0) {
        p->itemCount[p->selectedItem]--;
        playSfx(SFX_ITEM);
        if (p->selectedItem == 0) { p->hp += 40; if (p->hp > p->maxhp) p->hp = p->maxhp; }
        else if (p->selectedItem == 1) { p->mana += 60; if (p->mana > 100) p->mana = 100; }
        else { p->hp += 15; if (p->hp > p->maxhp) p->hp = p->maxhp; }
    }

    clampFighter(p);
}

// ---------------------------------------------------------------------
//  IA: caballeros grises (aliados) y enemigos (barbaros y ladrones)
// ---------------------------------------------------------------------
static Fighter *nearestFoe(Fighter *a) {
    Fighter *best = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_F; i++) {
        Fighter *b = &F[i];
        if (b == a || !isAlive(b) || b->team == a->team) continue;
        float d = fabsf(b->x - a->x) + fabsf(b->y - a->y) * 2.0f;
        if (d < bd) { bd = d; best = b; }
    }
    return best;
}

static void updateAI(Fighter *a, int idx) {
    if (a->deadTimer > 0) {
        if (--a->deadTimer == 0) a->active = 0;
        return;
    }
    if (a->hurtTimer > 0) {
        a->hurtTimer--;
        a->x += a->knockX;
        a->knockX *= 0.85f;
        a->moving = 0;
        clampFighter(a);
        return;
    }
    a->knockX = 0;
    a->moving = 0;

    int atk = (a->kind == K_NPC) ? 3 : (a->kind == K_BARB) ? 4 : 5;
    float speed = (a->kind == K_NPC) ? 1.6f : (a->kind == K_BARB) ? 1.0f : 1.7f;

    if (a->attackTimer > 0) {
        a->attackTimer--;
        attackHits(a);
        if (a->attackTimer == 0) {
            a->attackType = 0;
            a->cooldown = 45 + rand() % 50;
        }
        return;
    }

    Fighter *t = nearestFoe(a);
    if (!t) return;

    float ty = t->y + ((idx % 3) - 1) * 12;    // se reparten un poco en profundidad
    float dx = t->x - a->x;
    float dy = ty - a->y;
    a->facing = (dx >= 0) ? 1 : -1;
    if (a->cooldown > 0) a->cooldown--;

    float range = ATK[atk].reach * 0.75f;
    if (fabsf(dx) < range && fabsf(dy) < 14) {
        if (a->cooldown <= 0) {
            a->attackType = atk;
            a->attackTimer = ATK[atk].dur;
            a->hitDone = 0;
        }
    } else {
        if (fabsf(dx) > range * 0.7f) { a->x += (dx > 0) ? speed : -speed; a->moving = 1; }
        if (fabsf(dy) > 5) { a->y += ((dy > 0) ? speed : -speed) * 0.7f; a->moving = 1; }
    }
    clampFighter(a);
}

// oleadas: cuando no quedan enemigos, llega la siguiente
static void updateWaves(void) {
    if (waveBanner > 0) waveBanner--;
    if (countTeam(1, 0) > 0) { waveTimer = 120; return; }
    if (--waveTimer > 0) return;

    wave++;
    waveBanner = 120;
    waveTimer = 120;
    int n = 2 + wave;
    if (n > 6) n = 6;
    for (int i = 0; i < n; i++) {
        int kind = (i % 2 == 0) ? K_BARB : K_THIEF;
        float x = WORLD_RIGHT - (i % 2) * 40;
        float y = GROUND_TOP + 10 + rand() % (GROUND_BOTTOM - GROUND_TOP - 20);
        spawnFighter(kind, x, y);
    }
    // refuerzos: siempre hay 2 caballeros grises
    while (countTeam(0, 1) < 2)
        spawnFighter(K_NPC, WORLD_LEFT + 20, GROUND_TOP + 20 + rand() % 90);
}

static void updateGame(u32 *pressed, u32 *held, int *lx, int *ly) {
    walkTick++;
    for (int p = 0; p < numPlayers; p++) {
        if (!F[p].active) continue;
        if (F[p].deadTimer > 0) { if (--F[p].deadTimer == 0) F[p].active = 0; continue; }
        updatePlayerF(&F[p], pressed[p], held[p], lx[p], ly[p]);
    }
    for (int i = 2; i < MAX_F; i++)
        if (F[i].active) updateAI(&F[i], i);
    updateWaves();

    int alive = 0;
    for (int p = 0; p < numPlayers; p++) if (isAlive(&F[p])) alive++;
    if (alive == 0 && !F[0].deadTimer && !F[1].deadTimer) gameOver = 1;
}

// ---------------------------------------------------------------------
//  Dibujo de luchadores
// ---------------------------------------------------------------------
static void drawBar(GSGLOBAL *gs, int x, int y, int w, int hp, int maxhp, int r, int g, int b) {
    if (hp < 0) hp = 0;
    gsKit_prim_sprite(gs, x - 2, y - 2, x + w + 2, y + 14, 10, COL(0, 0, 0));
    gsKit_prim_sprite(gs, x, y, x + w, y + 12, 11, COL(80, 0, 0));
    gsKit_prim_sprite(gs, x, y, x + (w * hp) / maxhp, y + 12, 12, COL(r, g, b));
}

static void drawFighter(GSGLOBAL *gs, Fighter *f) {
    // escala por profundidad: mas abajo = mas grande
    float t = (f->y - GROUND_TOP) / (float)(GROUND_BOTTOM - GROUND_TOP);
    float s = 1.35f + 0.3f * t;

    if (f->deadTimer > 0 && (f->deadTimer / 3) % 2) return;   // parpadea al caer

    // sombra en el suelo (semitransparente)
    gsKit_prim_sprite(gs, f->x - 26 * s, f->y - 5 * s, f->x + 26 * s, f->y + 5 * s, 2, COLA(0, 0, 0, 0x40));

    float top = f->y - f->z - 64 * s;   // cabeza (para barras y nombre)

    if (!texOk(&texSprites)) {
        // respaldo con rectangulos si no entra la textura
        int r = f->team ? 200 : 40, g = f->team ? 60 : 120, b = f->team ? 40 : 220;
        gsKit_prim_sprite(gs, f->x - 20 * s, f->y - f->z - 70 * s, f->x + 20 * s, f->y - f->z, 3, COL(r, g, b));
    } else {
        static const int walkSeq[4] = {0, 1, 2, 1};
        int w = walkSeq[(walkTick / 7) % 4];
        int tm = f->attackTimer;
        const SpriteFrame *tab;
        int fi;
        int hurt = (f->hurtTimer > 0 || f->deadTimer > 0);

        if (f->kind == K_PLAYER) {
            tab = (f->pidx == 0) ? playerFrames : player2Frames;
            if (hurt)                    fi = PF_HURT;
            else if (f->blocking)        fi = PF_BLOCK;
            else if (f->attackType == 1) fi = (tm > 15) ? PF_L0 : (tm > 7) ? PF_L1 : PF_L2;
            else if (f->attackType == 2) fi = (tm > 25) ? PF_H0 : (tm > 14) ? PF_H1 : PF_H2;
            else if (f->z > 0)           fi = PF_JUMP;
            else if (f->moving)          fi = PF_WALK0 + w;
            else                         fi = PF_IDLE;
        } else if (f->kind == K_NPC) {
            tab = npcFrames;
            if (hurt)                    fi = NF_HURT;
            else if (f->attackType == 3) fi = (tm > 15) ? NF_L0 : (tm > 7) ? NF_L1 : NF_L2;
            else if (f->moving)          fi = NF_WALK0 + w;
            else                         fi = NF_IDLE;
        } else {
            tab = (f->kind == K_BARB) ? barbFrames : thiefFrames;
            if (hurt)                    fi = EF_HURT;
            else if (f->attackType > 0)  fi = (tm > ATK[f->attackType].hitHi) ? EF_ATKW : EF_ATKS;
            else if (f->moving)          fi = EF_WALK0 + w;
            else                         fi = EF_IDLE;
        }
        drawSpriteFrame(gs, &tab[fi], f->x, f->y - f->z, s, f->facing < 0, TEXCOL(128, 128, 128, 128));
    }

    // nombre del jugador / barra de vida de los demas
    if (f->kind == K_PLAYER) {
        const char *nm = (f->pidx == 0) ? "JUGADOR (1)" : "JUGADOR (2)";
        drawTextCentered(gs, nm, f->x, top - 26, 0.6f, 120, 255, 120);
    } else {
        int bw = 40;
        int bx = (int)(f->x - bw / 2), by = (int)(top - 8);
        gsKit_prim_sprite(gs, bx - 1, by - 1, bx + bw + 1, by + 6, 10, COL(0, 0, 0));
        gsKit_prim_sprite(gs, bx, by, bx + (bw * f->hp) / f->maxhp, by + 5, 11,
                          f->team ? COL(230, 50, 40) : COL(80, 140, 240));
    }
}

// =====================================================================
//  MENU PRINCIPAL
// =====================================================================

typedef enum { STATE_MENU, STATE_GAME } GameState;
static GameState state = STATE_MENU;

// ---- Ajustes del fondo animado (60 cuadros = 1 segundo) ----
#define BG_HOLD_FRAMES  240   // tiempo que se queda quieta cada imagen (4 s)
#define BG_SLIDE_FRAMES 30    // duracion del deslizamiento (0,5 s)
// Para que deslice sin pausa, poner BG_HOLD_FRAMES en 0.

static int bgIndex = 0, bgTimer = 0;

typedef struct {
    float x, y, w, h, cx, cy;        // boton
    float gx, gy, gw, gh;            // glow
} MenuItem;

static MenuItem items[3];
static int menuSel = 0;
static int menuTick = 0;
static int shakeTimer = 0;
static u32 vramBase = 0;             // donde empiezan las texturas en la memoria de video

static void loadTex(GSGLOBAL *gs, GSTEXTURE *t, const unsigned int *data, int w, int h) {
    memset(t, 0, sizeof(GSTEXTURE));
    t->Width = w;
    t->Height = h;
    t->PSM = GS_PSM_CT32;
    t->Mem = (u32 *)data;
    t->Filter = GS_FILTER_LINEAR;
    t->Vram = gsKit_vram_alloc(gs, gsKit_texture_size(w, h, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
    if (t->Vram == GSKIT_ALLOC_ERROR) {
        printf("Sin VRAM para textura %dx%d\n", w, h);
        t->Vram = 0;
        return;
    }
    gsKit_texture_upload(gs, t);
}

static void initMenuItems(void) {
    items[0] = (MenuItem){ BTN0_X, BTN0_Y, BTN0_W, BTN0_H, BTN0_CX, BTN0_CY,
                           BTN0_GLOW_X, BTN0_GLOW_Y, BTN0_GLOW_W * 2, BTN0_GLOW_H * 2 };
    items[1] = (MenuItem){ BTN1_X, BTN1_Y, BTN1_W, BTN1_H, BTN1_CX, BTN1_CY,
                           BTN1_GLOW_X, BTN1_GLOW_Y, BTN1_GLOW_W * 2, BTN1_GLOW_H * 2 };
    items[2] = (MenuItem){ BTN2_X, BTN2_Y, BTN2_W, BTN2_H, BTN2_CX, BTN2_CY,
                           BTN2_GLOW_X, BTN2_GLOW_Y, BTN2_GLOW_W * 2, BTN2_GLOW_H * 2 };
}

// La memoria de video (4 MB) no alcanza para todo junto, asi que se cargan
// las imagenes del menu o las del juego segun donde estemos.
static void loadMenuResources(GSGLOBAL *gs) {
    gs->CurrentPointer = vramBase;
    memset(texBg, 0, sizeof(texBg));
    memset(&texSprites, 0, sizeof(texSprites));
    FlushCache(0);
    loadTex(gs, &texBg[0], bg1_data, BG_TEX_W, BG_TEX_H);
    loadTex(gs, &texBg[1], bg2_data, BG_TEX_W, BG_TEX_H);
    loadTex(gs, &texBg[2], bg3_data, BG_TEX_W, BG_TEX_H);
    loadTex(gs, &texLogo, logo_data, LOGO_W, LOGO_H);
    loadTex(gs, &texBtn[0], btn0_data, BTN0_W, BTN0_H);
    loadTex(gs, &texBtn[1], btn1_data, BTN1_W, BTN1_H);
    loadTex(gs, &texBtn[2], btn2_data, BTN2_W, BTN2_H);
    loadTex(gs, &texGlow[0], btn0_glow_data, BTN0_GLOW_W, BTN0_GLOW_H);
    loadTex(gs, &texGlow[1], btn1_glow_data, BTN1_GLOW_W, BTN1_GLOW_H);
    loadTex(gs, &texGlow[2], btn2_glow_data, BTN2_GLOW_W, BTN2_GLOW_H);
    loadTex(gs, &texFont, font_data, FONT_W, FONT_H);
}

static void loadGameResources(GSGLOBAL *gs, int bg) {
    gs->CurrentPointer = vramBase;
    memset(texBg, 0, sizeof(texBg));
    memset(&texLogo, 0, sizeof(texLogo));
    memset(texBtn, 0, sizeof(texBtn));
    memset(texGlow, 0, sizeof(texGlow));
    FlushCache(0);
    static const unsigned int *bgData[3] = { bg1_data, bg2_data, bg3_data };
    loadTex(gs, &texBg[bg], bgData[bg], BG_TEX_W, BG_TEX_H);
    loadTex(gs, &texSprites, sprites_data, SPRITES_W, SPRITES_H);
    loadTex(gs, &texFont, font_data, FONT_W, FONT_H);
}

// dibuja una textura escalada alrededor de (cx, cy)
static void drawTexScaled(GSGLOBAL *gs, GSTEXTURE *t, float x, float y, float w, float h,
                          float cx, float cy, float s, u64 col) {
    float x1 = cx + (x - cx) * s;
    float y1 = cy + (y - cy) * s;
    float x2 = cx + (x + w - cx) * s;
    float y2 = cy + (y + h - cy) * s;
    gsKit_prim_sprite_texture(gs, t, x1, y1, 0.0f, 0.0f, x2, y2,
                              (float)t->Width, (float)t->Height, 2, col);
}

static void drawBgImage(GSGLOBAL *gs, GSTEXTURE *t, int x) {
    gsKit_prim_sprite_texture(gs, t, (float)x, 0.0f, 0.5f, 0.5f,
                              (float)(x + SCREEN_W), (float)SCREEN_H,
                              BG_TEX_W - 0.5f, BG_TEX_H - 0.5f, 1, TEXCOL(128, 128, 128, 128));
}

static void updateMenu(GSGLOBAL *gs, u32 navPressed, u32 pressed) {
    // fondo: cada imagen se queda quieta y luego se desliza hacia la izquierda
    if (++bgTimer >= BG_HOLD_FRAMES + BG_SLIDE_FRAMES) {
        bgTimer = 0;
        bgIndex = (bgIndex + 1) % 3;
    }
    menuTick++;
    if (shakeTimer > 0) shakeTimer--;
    if (soonTimer > 0) soonTimer--;
    int oldSel = menuSel;

    // navegacion: arriba = Partida local, abajo = botones de abajo
    if ((navPressed & PAD_DOWN)  && menuSel == 0) menuSel = 1;
    if ((navPressed & PAD_UP)    && menuSel != 0) menuSel = 0;
    if ((navPressed & PAD_RIGHT) && menuSel == 1) menuSel = 2;
    if ((navPressed & PAD_LEFT)  && menuSel == 2) menuSel = 1;

    if (menuSel != oldSel) playSfx(SFX_MOVE);

    if (pressed & (PAD_CROSS | PAD_START)) {
        playSfx(SFX_SELECT);
        if (menuSel == 0) {                 // Partida local
            gameBg = bgIndex;
            loadGameResources(gs, gameBg);
            resetGame();
            startGameMusic();
            state = STATE_GAME;
        } else if (menuSel == 1) {          // llave: cambiar 1 / 2 jugadores
            numPlayers = (numPlayers == 1) ? 2 : 1;
        } else {
            shakeTimer = 18;                // todavia no hace nada: el boton "tiembla"
            soonTimer = 90;
        }
    }
}

static void drawMenu(GSGLOBAL *gs) {
    // ---- fondo deslizante ----
    int offset = 0;
    if (bgTimer >= BG_HOLD_FRAMES) {
        float k = (float)(bgTimer - BG_HOLD_FRAMES) / (float)BG_SLIDE_FRAMES;
        float e = k * k * (3.0f - 2.0f * k);   // suave al empezar y al terminar
        offset = (int)(e * SCREEN_W);
    }
    drawBgImage(gs, &texBg[bgIndex], -offset);
    if (offset > 0) drawBgImage(gs, &texBg[(bgIndex + 1) % 3], SCREEN_W - offset);

    // ---- logo ----
    gsKit_prim_sprite_texture(gs, &texLogo, LOGO_X, LOGO_Y, 0, 0,
                              LOGO_X + LOGO_W, LOGO_Y + LOGO_H, LOGO_W, LOGO_H, 2,
                              TEXCOL(128, 128, 128, 128));

    // ---- botones ----
    float pulse = 0.5f + 0.5f * sinf(menuTick * 0.12f);   // 0..1
    for (int i = 0; i < 3; i++) {
        MenuItem *m = &items[i];
        float dx = 0;
        float s = 1.0f;
        if (i == menuSel) {
            s = 1.03f + 0.02f * pulse;
            if (shakeTimer > 0) dx = sinf(shakeTimer * 1.8f) * 4.0f;

            // glow: se dibuja detras del boton, pulsando
            int ga = (int)(128.0f * (0.55f + 0.45f * pulse));
            drawTexScaled(gs, &texGlow[i], m->gx + dx, m->gy, m->gw, m->gh,
                          m->cx + dx, m->cy, s, TEXCOL(128, 128, 128, ga));
        }
        drawTexScaled(gs, &texBtn[i], m->x + dx, m->y, m->w, m->h,
                      m->cx + dx, m->cy, s, TEXCOL(128, 128, 128, 128));
    }

    // ---- textos ----
    char buf[32];
    sprintf(buf, "JUGADORES: %d", numPlayers);
    drawText(gs, buf, 16, 304, 0.9f, 255, 230, 90);
    if (menuSel == 1) drawText(gs, "X: CAMBIAR 1 / 2 JUGADORES", 16, 326, 0.7f, 255, 255, 255);
    drawText(gs, "X: ACEPTAR", 500, 410, 0.9f, 255, 255, 255);
    drawText(gs, audioStatus, 16, 410, 0.8f, 255, 255, 120);
    if (soonTimer > 0) drawTextCentered(gs, "PROXIMAMENTE", 320, 350, 1.4f, 255, 230, 90);
}

// =====================================================================
//  PANTALLA DE JUEGO
// =====================================================================

static void drawPlayerHud(GSGLOBAL *gs, Fighter *p, int right) {
    int hasIcons = texOk(&texSprites);
    const char *name = (p->pidx == 0) ? "JUGADOR (1)" : "JUGADOR (2)";
    int head = (p->pidx == 0) ? I_HEAD_GREEN : I_HEAD_RED;
    int bx = right ? 392 : (hasIcons ? 58 : 20);

    if (!right) {
        drawIcon(gs, head, 14, 10);
        drawText(gs, name, (float)bx, 4, 0.8f, 255, 255, 255);
    } else {
        drawIcon(gs, head, 592, 10);
        drawText(gs, name, (float)(584 - textWidth(name, 0.8f)), 4, 0.8f, 255, 255, 255);
    }
    drawBar(gs, bx, 28, 180, p->hp, p->maxhp, right ? 230 : 50, right ? 70 : 220, right ? 60 : 60);
    drawBar(gs, bx, 46, 180, (int)p->mana, 100, 60, 120, 240);

    // objeto seleccionado: icono + cantidad
    char buf[8];
    sprintf(buf, "X%d", p->itemCount[p->selectedItem]);
    if (hasIcons) {
        int icon[3] = {I_POTION_RED, I_POTION_BLUE, I_FOOD};
        if (!right) {
            drawIcon(gs, icon[p->selectedItem], 16, 66);
            drawText(gs, buf, 58, 76, 1.0f, 255, 255, 255);
        } else {
            drawIcon(gs, icon[p->selectedItem], 594, 66);
            drawText(gs, buf, (float)(586 - textWidth(buf, 1.0f)), 76, 1.0f, 255, 255, 255);
        }
    } else {
        drawText(gs, buf, right ? 560.0f : 20.0f, 70, 1.0f, 255, 255, 255);
    }
}

static void drawGame(GSGLOBAL *gs) {
    gsKit_clear(gs, COL(40, 60, 100));
    if (texOk(&texBg[gameBg])) {
        // fondo: el mismo artwork del menu, un poco oscurecido, con el suelo marcado
        drawBgImage(gs, &texBg[gameBg], 0);
        gsKit_prim_sprite(gs, 0, 0, SCREEN_W, SCREEN_H, 1, COLA(0, 0, 0, 0x28));
        gsKit_prim_sprite(gs, 0, GROUND_TOP - 20, SCREEN_W, SCREEN_H, 1, COLA(70, 55, 40, 0x90));
    } else {
        gsKit_prim_sprite(gs, 0, 0, SCREEN_W, GROUND_TOP - 20, 0, COL(60, 90, 150));
        gsKit_prim_sprite(gs, 0, GROUND_TOP - 20, SCREEN_W, SCREEN_H, 0, COL(90, 80, 70));
    }

    // luchadores ordenados por profundidad (el de mas abajo se dibuja encima)
    int order[MAX_F], n = 0;
    for (int i = 0; i < MAX_F; i++) {
        if (!F[i].active) continue;
        int j = n++;
        while (j > 0 && F[order[j - 1]].y > F[i].y) { order[j] = order[j - 1]; j--; }
        order[j] = i;
    }
    for (int k = 0; k < n; k++) drawFighter(gs, &F[order[k]]);

    // efecto de magia
    for (int p = 0; p < numPlayers; p++) {
        Fighter *f = &F[p];
        if (f->active && f->magicTimer > 0 && (f->magicTimer / 2) % 2 == 0)
            gsKit_prim_sprite(gs, f->x - 240, f->y - 80, f->x + 240, f->y - 50, 6, COL(255, 240, 80));
    }

    // HUD
    drawPlayerHud(gs, &F[0], 0);
    if (numPlayers == 2) drawPlayerHud(gs, &F[1], 1);

    char buf[32];
    sprintf(buf, "OLEADA %d", wave);
    drawTextCentered(gs, buf, 320, 6, 0.8f, 255, 230, 90);
    sprintf(buf, "BAJAS: %d", kills);
    drawTextCentered(gs, buf, 320, 28, 0.7f, 255, 255, 255);
    if (waveBanner > 0 && wave > 0) {
        sprintf(buf, "OLEADA %d", wave);
        drawTextCentered(gs, buf, 320, 130, 2.0f, 255, 230, 90);
    }

    // fin del juego
    if (gameOver) {
        gsKit_prim_sprite(gs, 0, 0, SCREEN_W, SCREEN_H, 19, COLA(0, 0, 0, 0x60));
        drawTextCentered(gs, "FIN DEL JUEGO", 320, 150, 2.0f, 255, 90, 70);
        drawTextCentered(gs, "START: VOLVER AL MENU", 320, 250, 1.0f, 255, 255, 255);
    }

    // pausa
    if (paused) {
        gsKit_prim_sprite(gs, 0, 0, SCREEN_W, SCREEN_H, 19, COLA(0, 0, 0, 0x50));
        if (texOk(&texFont)) {
            drawTextCentered(gs, "PAUSA", 320, 150, 2.2f, 255, 230, 90);
            drawTextCentered(gs, "START: CONTINUAR", 320, 240, 1.0f, 255, 255, 255);
            drawTextCentered(gs, "CIRCULO: VOLVER AL MENU", 320, 270, 1.0f, 255, 255, 255);
            drawTextCentered(gs, "SELECT: REINICIAR", 320, 300, 1.0f, 255, 255, 255);
        } else {
            gsKit_prim_sprite(gs, 295, 190, 315, 250, 20, COL(255, 255, 255));
            gsKit_prim_sprite(gs, 335, 190, 355, 250, 20, COL(255, 255, 255));
        }
    }
}

// cuadrados verdes que avanzan mientras carga (sirve para ver donde se frena)
static void bootStage(GSGLOBAL *gs, int n) {
    for (int k = 0; k < 2; k++) {   // dos veces por el doble buffer
        gsKit_clear(gs, COL(20, 20, 60));
        for (int i = 0; i < 6; i++)
            gsKit_prim_sprite(gs, 200 + i * 40, 210, 224 + i * 40, 234, 1,
                              i < n ? COL(80, 220, 80) : COL(60, 60, 90));
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }
}

static struct padButtonStatus padStat;

// lee un control (port 0 = jugador 1, port 1 = jugador 2)
static void readPad(int port, u32 *old, u32 *held, u32 *pressed, int *lx, int *ly) {
    *held = 0; *pressed = 0; *lx = 128; *ly = 128;
    if (padGetState(port, 0) == PAD_STATE_STABLE) {
        if (padRead(port, 0, &padStat)) {
            *held = 0xffff ^ padStat.btns;
            *pressed = *held & ~(*old);
            *old = *held;
            // usar el stick solo si el control esta en modo analogico
            if (padInfoMode(port, 0, PAD_MODECURID, 0) == PAD_TYPE_DUALSHOCK) {
                *lx = padStat.ljoy_h;
                *ly = padStat.ljoy_v;
            }
        }
    }
}

static void backToMenu(GSGLOBAL *gs) {
    paused = 0;
    state = STATE_MENU;
    menuSel = 0;
    loadMenuResources(gs);
    startMenuMusic();
}

int main(void) {
    GSGLOBAL *gs = gsKit_init_global();
    gs->PSM = GS_PSM_CT16;                 // 16 bits: deja memoria de video para las texturas
    gs->Dithering = GS_SETTING_ON;
    gs->PSMZ = GS_PSMZ_16S;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->ZBuffering = GS_SETTING_OFF;
    gs->PrimAlphaEnable = GS_SETTING_ON;   // transparencias (botones, glow)

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    // mezcla normal: resultado = origen * alfa + destino * (1 - alfa)
    gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    vramBase = gs->CurrentPointer;

    bootStage(gs, 1);          // 1: pantalla lista
    initPad();
    bootStage(gs, 2);          // 2: control listo
    resetGame();
    initMenuItems();
    loadMenuResources(gs);
    bootStage(gs, 3);          // 3: imagenes cargadas

    // dibuja el menu una vez y recien despues inicia el audio
    for (int k = 0; k < 2; k++) {
        drawMenu(gs);
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }
    initAudio();
    startMenuMusic();

    srand(12345);
    u32 oldPad[2] = {0, 0}, oldNav = 0;

    while (1) {
        u32 held[2], pressed[2];
        int lx[2], ly[2];
        readPad(0, &oldPad[0], &held[0], &pressed[0], &lx[0], &ly[0]);
        if (state == STATE_GAME && numPlayers == 2)
            readPad(1, &oldPad[1], &held[1], &pressed[1], &lx[1], &ly[1]);
        else { held[1] = pressed[1] = 0; lx[1] = ly[1] = 128; }

        // direcciones para el menu: cruceta + stick izquierdo del control 1
        u32 navHeld = held[0];
        if (lx[0] < 98)  navHeld |= PAD_LEFT;
        if (lx[0] > 158) navHeld |= PAD_RIGHT;
        if (ly[0] < 98)  navHeld |= PAD_UP;
        if (ly[0] > 158) navHeld |= PAD_DOWN;
        u32 navPressed = navHeld & ~oldNav;
        oldNav = navHeld;

        if (state == STATE_MENU) {
            updateMenu(gs, navPressed, pressed[0]);
            if (state == STATE_MENU) drawMenu(gs);
            else drawGame(gs);                    // acaba de empezar la partida
        } else {
            if (pressed[0] & PAD_SELECT) resetGame();
            if (gameOver) {
                if (pressed[0] & PAD_START) backToMenu(gs);
            } else {
                if ((pressed[0] | pressed[1]) & PAD_START) paused = !paused;
                if (paused && (pressed[0] & PAD_CIRCLE)) backToMenu(gs);
            }
            if (state == STATE_GAME) {
                if (!paused && !gameOver) updateGame(pressed, held, lx, ly);
                drawGame(gs);
            } else {
                drawMenu(gs);                     // volvio al menu
            }
        }

        audioUpdate();

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }

    return 0;
}
