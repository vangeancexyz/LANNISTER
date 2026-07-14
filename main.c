#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "mem.h"
#include "game.h"
#include "overlay.h"

#define DEBUG 1   // 0 = off, 1 = on
// https://c-faq.com/cpp/multistmt.html
#if DEBUG
    #define LOG(fmt, ...) do { \
        FILE *_f = fopen("/tmp/lannister.log", "a"); \
        if (_f) { fprintf(_f, "[LANNISTER] " fmt "\n", ##__VA_ARGS__); fclose(_f); } \
    } while(0)
#else
    #define LOG(fmt, ...) do {} while(0)
#endif

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

static volatile bool g_running = false;

static void *cheat_thread(void *arg) {
    (void)arg;

    mem_ctx_t *ctx = mem_create("self");
    if (!ctx) { LOG("[+] mem_create failed"); return NULL; }

    if (mem_attach(ctx) != MEM_OK) {
        LOG("[+] mem_attach failed");
        mem_destroy(ctx);
        return NULL;
    }

    game_ctx_t game;
    game_wait_attach(&game, ctx); // blocks until client.so shows up
    LOG("[-] client base=0x%lx", game.client.base);

    overlay_config_t ov_cfg = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    overlay_t *ov = overlay_create(&ov_cfg);
    if (!ov) { LOG("[+] overlay_create failed"); mem_destroy(ctx); return NULL; }
    LOG("[-] overlay OK");

    while (g_running) {
        overlay_event_batch_t events = overlay_poll_events(ov);
        if (events.quit_requested) break; // bug: game also uses SDL, this fires early

        vec3_t pos;
        int health;
        uintptr_t addr;

        if (game_get_local_player(&game, &pos, &health, &addr)) {
            //LOG("[-] local_player=0x%lx hp=%d pos=(%.1f, %.1f, %.1f)",
                //addr, health, pos.x, pos.y, pos.z);
        }

        overlay_frame_begin(ov);
        SDL_SetRenderDrawColor(overlay_renderer(ov), 255, 0, 0, 255);
        SDL_Rect test_box = { 100, 100, 200, 200 };
        SDL_RenderFillRect(overlay_renderer(ov), &test_box);
        overlay_frame_end(ov);
        usleep(5 * 1000);
    }

    overlay_destroy(ov);
    mem_destroy(ctx);
    return NULL;
}

__attribute__((constructor))
void init() {
    g_running = true;

    pthread_t tid;
    pthread_create(&tid, NULL, cheat_thread, NULL);
    pthread_detach(tid);
}

__attribute__((destructor))
void fini() {
    g_running = false;
}
