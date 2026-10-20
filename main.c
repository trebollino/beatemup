// Beat 'em up 2.5D para PS2 - Base (Etapas 1 y 2)
// Controles: D-pad o stick izq = mover | X = golpe | O = salto | START = reiniciar enemigo
// Todo se dibuja con rectangulos de colores. Los sprites se agregan despues.

#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <libpad.h>

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
    int   hurtTimer;     // parpadeo al recibir dano
    float knockX;
} Fighter;

static Fighter player, enemy;
static int enemyAlive = 1;
static int respawnTimer = 0;

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
}

static void resetGame(void) {
    memset(&player, 0, sizeof(player));
    player.x = 150; player.y = 360; player.facing = 1; player.hp = 100;
    memset(&enemy, 0, sizeof(enemy));
    enemy.x = 450; enemy.y = 360; enemy.facing = -1; enemy.hp = 60;
    enemyAlive = 1;
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
    if (f->attackTimer > 0 && f->attackTimer < 14) {
        float ax = f->x + f->facing * (w / 2);
        float ax2 = ax + f->facing * 35 * s;
        float x1 = ax < ax2 ? ax : ax2;
        float x2 = ax < ax2 ? ax2 : ax;
        gsKit_prim_sprite(gs, x1, by + 15 * s, x2, by + 30 * s, 4, COL(255, 220, 120));
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

    if (held & PAD_LEFT)  dx -= 1;
    if (held & PAD_RIGHT) dx += 1;
    if (held & PAD_UP)    dy -= 1;
    if (held & PAD_DOWN)  dy += 1;

    // stick analogico (centro 128, zona muerta 30)
    if (lx < 98)  dx = -1; else if (lx > 158) dx = 1;
    if (ly < 98)  dy = -1; else if (ly > 158) dy = 1;

    // no moverse mientras golpea
    if (player.attackTimer == 0) {
        player.x += dx * speed;
        player.y += dy * speed * 0.7f;
        if (dx > 0) player.facing = 1;
        if (dx < 0) player.facing = -1;
    }

    // salto
    if ((pressed & PAD_CIRCLE) && player.z == 0 && player.attackTimer == 0) {
        player.vz = 12.0f;
    }
    if (player.z > 0 || player.vz > 0) {
        player.z += player.vz;
        player.vz -= GRAVITY;
        if (player.z <= 0) { player.z = 0; player.vz = 0; }
    }

    // golpe
    if ((pressed & PAD_CROSS) && player.attackTimer == 0) {
        player.attackTimer = 20;
        player.hitDone = 0;
    }
    if (player.attackTimer > 0) player.attackTimer--;
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
    if (enemyAlive && player.attackTimer > 8 && player.attackTimer < 16 && !player.hitDone) {
        float dx = (enemy.x - player.x) * player.facing;
        float dy = enemy.y - player.y;
        if (dx > 0 && dx < 75 && dy > -22 && dy < 22) {
            enemy.hp -= 10;
            enemy.hurtTimer = 15;
            enemy.knockX = 8.0f * player.facing;
            player.hitDone = 1;
            if (enemy.hp <= 0) { enemyAlive = 0; respawnTimer = 120; }
        }
    }
}

int main(void) {
    GSGLOBAL *gs = gsKit_init_global();
    gs->PSM = GS_PSM_CT24;
    gs->PSMZ = GS_PSMZ_16S;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->ZBuffering = GS_SETTING_OFF;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);

    initPad();
    resetGame();

    struct padButtonStatus buttons;
    u32 oldPad = 0;

    while (1) {
        u32 held = 0, pressed = 0;
        int lx = 128, ly = 128;

        if (padGetState(0, 0) == PAD_STATE_STABLE) {
            if (padRead(0, 0, &buttons)) {
                held = 0xffff ^ buttons.btns;
                pressed = held & ~oldPad;
                oldPad = held;
                lx = buttons.ljoy_h;
                ly = buttons.ljoy_v;
            }
        }

        if (pressed & PAD_START) resetGame();

        updatePlayer(pressed, held, lx, ly);
        updateEnemy();
        checkHits();

        // ---- dibujo ----
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
        if (enemyAlive) drawBar(gs, 420, 20, 200, enemy.hp, 60, 230, 160, 40);

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }

    return 0;
}
