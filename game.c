#include "game.h"

#include <unistd.h>

// client.so
static const uintptr_t DW_LOCAL_PLAYER = 0xf946b0;
static const uintptr_t M_I_HEALTH      = 0xc0;
static const uintptr_t M_VEC_ORIGIN    = 0x2f8;

void game_wait_attach(game_ctx_t *game, mem_ctx_t *mem) {
    game->mem = mem;

    while (mem_get_module(mem, "client.so", &game->client) != MEM_OK) sleep(1);
}

bool game_get_local_player(game_ctx_t *game, vec3_t *out_pos, int *out_health, uintptr_t *out_addr) {
    uintptr_t local_player = MEM_READ(game->mem, game->client.base + DW_LOCAL_PLAYER, uintptr_t);
    if (local_player == 0) return false;

    *out_addr   = local_player;
    *out_health = MEM_READ(game->mem, local_player + M_I_HEALTH, int);
    *out_pos    = MEM_READ(game->mem, local_player + M_VEC_ORIGIN, vec3_t);
    return true;
}
