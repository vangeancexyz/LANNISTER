#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "mem.h"

typedef struct { float x, y, z; } vec3_t;

typedef struct {
    mem_ctx_t *mem;
    mem_module_t client;
} game_ctx_t;

// blocks using sleep until client.so appears in the process
void game_wait_attach(game_ctx_t *game, mem_ctx_t *mem);

// read the local player. false = could not read
bool game_get_local_player(game_ctx_t *game, vec3_t *out_pos, int *out_health, uintptr_t *out_addr);

#endif /* GAME_H */
