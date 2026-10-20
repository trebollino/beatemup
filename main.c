// Beat 'em up 2.5D para PS2 - Base (Etapas 1 y 2)
// Controles (estilo Castle Crashers):
//   Stick izq / D-pad = mover | X = saltar | Cuadrado = ataque suave
//   Triangulo = ataque agresivo | Circulo = usar objeto | L2 = bloquear
//   R2 = magia | L1/R1 = seleccionar objeto | START = pausa | SELECT = reiniciar
// Menu principal con fondos que se deslizan, botones con glow y navegacion con cruceta/stick.
// En el menu: cruceta/stick = elegir | X o START = aceptar
// En el juego: START = pausa | en pausa, CIRCULO = volver al menu

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

#define SCREEN_W 640
#define SCREEN_H 448

// Zona caminable (profundidad 2.5D)
#define GROUND_TOP    300
#define GROUND_BOTTOM 420
#define WORLD_LEFT    20
#define WORLD_RIGHT   620

#define GRAVITY 0.8f

static char padBuf[256] __attribute__((aligned(64)));

typedef struct {
    float x, y;      // x horizontal, y profundidad (en el suelo)
    float z;         // altura (salto)
    float vz;
    int   facing;    // 1 derecha, -1 izquierda
    int   hp;
    int   attackTimer;   // >0 mientras golpea
    int   hitDone;       // ya pego en este golpe
    int   attackType;    // 0 nada, 1 suave, 2 agresivo
    int   hurtTimer;     // parpadeo al recibir dano
    float knockX;
} Fighter;

static Fighter player, enemy;
static int enemyAlive = 1;
static int respawnTimer = 0;

static float mana = 100;
static int magicTimer = 0;
static int blocking = 0;
static int paused = 0;
static int selectedItem = 0;
static int itemCount[3] = {3, 3, 3};   // 0 pocion vida, 1 pocion magia, 2 comida

static void damageEnemy(int dmg, float knock) {
    enemy.hp -= dmg;
    enemy.hurtTimer = 15;
    enemy.knockX = knock;
    if (enemy.hp <= 0) { enemyAlive = 0; respawnTimer = 120; }
}

static u64 COL(int r, int g, int b) {
    return GS_SETREG_RGBAQ(r, g, b, 0x80, 0x00);
}

static void initPad(void) {
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    // esperar a que el control este listo
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
}

static void resetGame(void) {
    memset(&player, 0, sizeof(player));
    player.x = 150; player.y = 360; player.facing = 1; player.hp = 100;
    memset(&enemy, 0, sizeof(enemy));
    enemy.x = 450; enemy.y = 360; enemy.facing = -1; enemy.hp = 60;
    enemyAlive = 1;
    mana = 100; magicTimer = 0; blocking = 0; selectedItem = 0;
    itemCount[0] = itemCount[1] = itemCount[2] = 3;
}

static void drawFighter(GSGLOBAL *gs, Fighter *f, int r, int g, int b) {
    // escala por profundidad: mas abajo = mas grande
    float t = (f->y - GROUND_TOP) / (float)(GROUND_BOTTOM - GROUND_TOP);
    float s = 0.8f + 0.4f * t;
    float w = 40 * s, h = 80 * s;

    // sombra en el suelo
    gsKit_prim_sprite(gs, f->x - w / 2, f->y - 4, f->x + w / 2, f->y + 4, 2, COL(20, 20, 20));

    // parpadeo si esta herido
    if (f->hurtTimer > 0 && (f->hurtTimer / 3) % 2 == 0) { r = 255; g = 255; b = 255; }

    float bx = f->x - w / 2;
    float by = f->y - f->z - h;
    gsKit_prim_sprite(gs, bx, by, bx + w, by + h, 3, COL(r, g, b));

    // cabeza
    gsKit_prim_sprite(gs, f->x - 10 * s, by - 18 * s, f->x + 10 * s, by, 3, COL(240, 200, 160));

    // brazo de golpe
    int armOn = (f->attackType == 2) ? (f->attackTimer > 14 && f->attackTimer < 26)
                                     : (f->attackTimer > 0 && f->attackTimer < 14 && f->attackType == 1);
    if (armOn) {
        float ax = f->x + f->facing * (w / 2);
        float ax2 = ax + f->facing * 35 * s;
        float x1 = ax < ax2 ? ax : ax2;
        float x2 = ax < ax2 ? ax2 : ax;
        gsKit_prim_sprite(gs, x1, by + 15 * s, x2, by + 30 * s, 4, COL(255, 220, 120));
    }

    // escudo al bloquear (solo jugador)
    if (f == &player && blocking) {
        float sx = f->x + f->facing * (w / 2 + 4);
        gsKit_prim_sprite(gs, sx - 6, by + 10 * s, sx + 6, by + h - 10 * s, 5, COL(180, 180, 200));
    }
}

static void drawBar(GSGLOBAL *gs, int x, int y, int w, int hp, int maxhp, int r, int g, int b) {
    if (hp < 0) hp = 0;
    gsKit_prim_sprite(gs, x - 2, y - 2, x + w + 2, y + 14, 10, COL(0, 0, 0));
    gsKit_prim_sprite(gs, x, y, x + w, y + 12, 11, COL(80, 0, 0));
    gsKit_prim_sprite(gs, x, y, x + (w * hp) / maxhp, y + 12, 12, COL(r, g, b));
}

static void clampFighter(Fighter *f) {
    if (f->x < WORLD_LEFT)  f->x = WORLD_LEFT;
    if (f->x > WORLD_RIGHT) f->x = WORLD_RIGHT;
    if (f->y < GROUND_TOP)    f->y = GROUND_TOP;
    if (f->y > GROUND_BOTTOM) f->y = GROUND_BOTTOM;
}

static void updatePlayer(u32 pressed, u32 held, int lx, int ly) {
    float speed = 3.0f;
    float dx = 0, dy = 0;

    // bloquear con L2 (en el suelo y sin atacar)
    blocking = ((held & PAD_L2) && player.z == 0 && player.attackTimer == 0);

    if (held & PAD_LEFT)  dx -= 1;
    if (held & PAD_RIGHT) dx += 1;
    if (held & PAD_UP)    dy -= 1;
    if (held & PAD_DOWN)  dy += 1;

    // stick analogico (centro 128, zona muerta 30)
    if (lx < 98)  dx = -1; else if (lx > 158) dx = 1;
    if (ly < 98)  dy = -1; else if (ly > 158) dy = 1;

    // no moverse mientras golpea
    if (player.attackTimer == 0 && !blocking) {
        player.x += dx * speed;
        player.y += dy * speed * 0.7f;
        if (dx > 0) player.facing = 1;
        if (dx < 0) player.facing = -1;
    }

    // salto
    if ((pressed & PAD_CROSS) && player.z == 0 && player.attackTimer == 0 && !blocking) {
        player.vz = 12.0f;
    }
    if (player.z > 0 || player.vz > 0) {
        player.z += player.vz;
        player.vz -= GRAVITY;
        if (player.z <= 0) { player.z = 0; player.vz = 0; }
    }

    // ataque suave (Cuadrado) y agresivo (Triangulo)
    if (player.attackTimer == 0 && !blocking) {
        if (pressed & PAD_SQUARE) {
            player.attackTimer = 20; player.attackType = 1; player.hitDone = 0;
        } else if (pressed & PAD_TRIANGLE) {
            player.attackTimer = 34; player.attackType = 2; player.hitDone = 0;
        }
    }
    if (player.attackTimer > 0) {
        player.attackTimer--;
        if (player.attackTimer == 0) player.attackType = 0;
    }

    // magia (R2): cuesta 50 de mana, pega a distancia
    if (mana < 100) mana += 0.1f;
    if (magicTimer > 0) magicTimer--;
    if ((pressed & PAD_R2) && mana >= 50 && magicTimer == 0) {
        mana -= 50;
        magicTimer = 30;
        if (enemyAlive) damageEnemy(25, 12.0f * (enemy.x >= player.x ? 1 : -1));
    }

    // seleccionar objeto (L1 / R1) y usar objeto (Circulo)
    if (pressed & PAD_R1) selectedItem = (selectedItem + 1) % 3;
    if (pressed & PAD_L1) selectedItem = (selectedItem + 2) % 3;
    if ((pressed & PAD_CIRCLE) && itemCount[selectedItem] > 0) {
        itemCount[selectedItem]--;
        if (selectedItem == 0) { player.hp += 40; if (player.hp > 100) player.hp = 100; }
        else if (selectedItem == 1) { mana += 60; if (mana > 100) mana = 100; }
        else { player.hp += 15; if (player.hp > 100) player.hp = 100; }
    }
    if (player.hurtTimer > 0) player.hurtTimer--;

    clampFighter(&player);
}

static void updateEnemy(void) {
    if (!enemyAlive) {
        if (--respawnTimer <= 0) {
            memset(&enemy, 0, sizeof(enemy));
            enemy.x = (player.x < 320) ? 560 : 80;
            enemy.y = GROUND_TOP + rand() % (GROUND_BOTTOM - GROUND_TOP);
            enemy.hp = 60; enemy.facing = -1;
            enemyAlive = 1;
        }
        return;
    }

    if (enemy.hurtTimer > 0) enemy.hurtTimer--;

    // knockback
    if (enemy.knockX != 0) {
        enemy.x += enemy.knockX;
        enemy.knockX *= 0.85f;
        if (enemy.knockX > -0.3f && enemy.knockX < 0.3f) enemy.knockX = 0;
    } else {
        // IA simple: acercarse al jugador
        float ex = player.x - enemy.x;
        float ey = player.y - enemy.y;
        if (ex > 55) { enemy.x += 1.5f; enemy.facing = 1; }
        else if (ex < -55) { enemy.x -= 1.5f; enemy.facing = -1; }
        else enemy.facing = (ex >= 0) ? 1 : -1;
        if (ey > 4) enemy.y += 1.0f;
        else if (ey < -4) enemy.y -= 1.0f;
    }
    clampFighter(&enemy);
}

static void checkHits(void) {
    // golpe del jugador al enemigo
    int t = player.attackTimer;
    int inWindow = (player.attackType == 1) ? (t > 8 && t < 16)
                 : (player.attackType == 2) ? (t > 14 && t < 26) : 0;
    if (enemyAlive && inWindow && !player.hitDone) {
        float dx = (enemy.x - player.x) * player.facing;
        float dy = enemy.y - player.y;
        float reach = (player.attackType == 2) ? 90 : 75;
        if (dx > 0 && dx < reach && dy > -22 && dy < 22) {
            int dmg = (player.attackType == 2) ? 20 : 10;
            float kb = (player.attackType == 2) ? 16.0f : 8.0f;
            damageEnemy(dmg, kb * player.facing);
            player.hitDone = 1;
        }
    }
}

// =====================================================================
//  MENU PRINCIPAL
// =====================================================================

typedef enum { STATE_MENU, STATE_GAME } GameState;
static GameState state = STATE_MENU;

// Formato de color para sprites con textura (0x80 = 1.0)
#define TEXCOL(r, g, b, a) GS_SETREG_RGBAQ((r), (g), (b), (a), 0x00)

// ---- Ajustes del fondo animado (60 cuadros = 1 segundo) ----
#define BG_HOLD_FRAMES  240   // tiempo que se queda quieta cada imagen (4 s)
#define BG_SLIDE_FRAMES 30    // duracion del deslizamiento (0,5 s)
// Para que deslice sin pausa, poner BG_HOLD_FRAMES en 0.

static GSTEXTURE texBg[3], texLogo, texBtn[3], texGlow[3];
static int bgIndex = 0, bgTimer = 0;

typedef struct {
    float x, y, w, h, cx, cy;        // boton
    float gx, gy, gw, gh;            // glow
} MenuItem;

static MenuItem items[3];
static int menuSel = 0;
static int menuTick = 0;
static int shakeTimer = 0;

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
        return;
    }
    gsKit_texture_upload(gs, t);
}

static void initMenu(GSGLOBAL *gs) {
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

    items[0] = (MenuItem){ BTN0_X, BTN0_Y, BTN0_W, BTN0_H, BTN0_CX, BTN0_CY,
                           BTN0_GLOW_X, BTN0_GLOW_Y, BTN0_GLOW_W * 2, BTN0_GLOW_H * 2 };
    items[1] = (MenuItem){ BTN1_X, BTN1_Y, BTN1_W, BTN1_H, BTN1_CX, BTN1_CY,
                           BTN1_GLOW_X, BTN1_GLOW_Y, BTN1_GLOW_W * 2, BTN1_GLOW_H * 2 };
    items[2] = (MenuItem){ BTN2_X, BTN2_Y, BTN2_W, BTN2_H, BTN2_CX, BTN2_CY,
                           BTN2_GLOW_X, BTN2_GLOW_Y, BTN2_GLOW_W * 2, BTN2_GLOW_H * 2 };
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

static void updateMenu(u32 navPressed, u32 pressed) {
    // fondo: cada imagen se queda quieta y luego se desliza hacia la izquierda
    if (++bgTimer >= BG_HOLD_FRAMES + BG_SLIDE_FRAMES) {
        bgTimer = 0;
        bgIndex = (bgIndex + 1) % 3;
    }
    menuTick++;
    if (shakeTimer > 0) shakeTimer--;

    // navegacion: arriba = Partida local, abajo = botones de abajo
    if ((navPressed & PAD_DOWN)  && menuSel == 0) menuSel = 1;
    if ((navPressed & PAD_UP)    && menuSel != 0) menuSel = 0;
    if ((navPressed & PAD_RIGHT) && menuSel == 1) menuSel = 2;
    if ((navPressed & PAD_LEFT)  && menuSel == 2) menuSel = 1;

    if (pressed & (PAD_CROSS | PAD_START)) {
        if (menuSel == 0) {
            resetGame();
            paused = 0;
            state = STATE_GAME;
        } else {
            shakeTimer = 18;   // todavia no hace nada: el boton "tiembla"
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
}

// =====================================================================
//  JUEGO
// =====================================================================

static void drawGame(GSGLOBAL *gs) {
    gsKit_clear(gs, COL(40, 60, 100));
    // cielo / fondo
    gsKit_prim_sprite(gs, 0, 0, SCREEN_W, GROUND_TOP - 20, 0, COL(60, 90, 150));
    // suelo
    gsKit_prim_sprite(gs, 0, GROUND_TOP - 20, SCREEN_W, SCREEN_H, 0, COL(90, 80, 70));

    // ordenar por profundidad (el de mas abajo se dibuja encima)
    if (!enemyAlive || player.y >= enemy.y) {
        if (enemyAlive) drawFighter(gs, &enemy, 200, 40, 40);
        drawFighter(gs, &player, 40, 120, 220);
    } else {
        drawFighter(gs, &player, 40, 120, 220);
        drawFighter(gs, &enemy, 200, 40, 40);
    }

    // HUD
    drawBar(gs, 20, 20, 200, player.hp, 100, 50, 220, 60);
    drawBar(gs, 20, 40, 200, (int)mana, 100, 60, 120, 240);

    // objeto seleccionado: cuadro de color + un punto por cada unidad
    {
        int cr[3] = {220, 60, 60}, cg[3] = {40, 120, 200}, cb[3] = {40, 240, 60};
        gsKit_prim_sprite(gs, 20, 62, 44, 86, 11, COL(cr[selectedItem], cg[selectedItem], cb[selectedItem]));
        for (int i = 0; i < itemCount[selectedItem]; i++)
            gsKit_prim_sprite(gs, 52 + i * 10, 70, 58 + i * 10, 78, 11, COL(255, 255, 255));
    }

    // efecto de magia
    if (magicTimer > 0 && (magicTimer / 2) % 2 == 0)
        gsKit_prim_sprite(gs, player.x - 220, player.y - 70, player.x + 220, player.y - 40, 6, COL(255, 240, 80));

    // pausa: dos barras en el centro
    if (paused) {
        gsKit_prim_sprite(gs, 295, 190, 315, 250, 20, COL(255, 255, 255));
        gsKit_prim_sprite(gs, 335, 190, 355, 250, 20, COL(255, 255, 255));
    }
    if (enemyAlive) drawBar(gs, 420, 20, 200, enemy.hp, 60, 230, 160, 40);
}

int main(void) {
    GSGLOBAL *gs = gsKit_init_global();
    gs->PSM = GS_PSM_CT24;
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

    initPad();
    resetGame();
    initMenu(gs);

    struct padButtonStatus buttons;
    u32 oldPad = 0, oldNav = 0;

    while (1) {
        u32 held = 0, pressed = 0;
        int lx = 128, ly = 128;

        if (padGetState(0, 0) == PAD_STATE_STABLE) {
            if (padRead(0, 0, &buttons)) {
                held = 0xffff ^ buttons.btns;
                pressed = held & ~oldPad;
                oldPad = held;
                // usar el stick solo si el control esta en modo analogico
                if (padInfoMode(0, 0, PAD_MODECURID, 0) == PAD_TYPE_DUALSHOCK) {
                    lx = buttons.ljoy_h;
                    ly = buttons.ljoy_v;
                }
            }
        }

        // direcciones para el menu: cruceta + stick izquierdo
        u32 navHeld = held;
        if (lx < 98)  navHeld |= PAD_LEFT;
        if (lx > 158) navHeld |= PAD_RIGHT;
        if (ly < 98)  navHeld |= PAD_UP;
        if (ly > 158) navHeld |= PAD_DOWN;
        u32 navPressed = navHeld & ~oldNav;
        oldNav = navHeld;

        if (state == STATE_MENU) {
            updateMenu(navPressed, pressed);
            drawMenu(gs);
        } else {
            if (pressed & PAD_SELECT) resetGame();
            if (pressed & PAD_START) paused = !paused;
            if (paused && (pressed & PAD_CIRCLE)) {   // volver al menu desde la pausa
                paused = 0;
                state = STATE_MENU;
                menuSel = 0;
            }
            if (!paused && state == STATE_GAME) {
                updatePlayer(pressed, held, lx, ly);
                updateEnemy();
                checkHits();
            }
            drawGame(gs);
        }

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }

    return 0;
}
