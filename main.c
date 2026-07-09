#include <stdio.h>
#include "mem.h"

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

__attribute__((constructor))
void init() {
    mem_ctx_t *ctx = mem_create("kcalc");
    if (!ctx) { LOG("mem_create failed"); return; }

    if (mem_attach(ctx) != MEM_OK) {
        LOG("[+] mem_attach failed");
        mem_destroy(ctx);
        return;
    }

    mem_module_t mod;
    if (mem_get_module(ctx, "kcalc", &mod) != MEM_OK) {
        LOG("[+] mem_get_module failed");
        mem_destroy(ctx);
        return;
    }

    LOG("[-] kcalc base=0x%lx size=0x%lx", mod.base, mod.size);

    unsigned char magic[4];
    if (mem_read_raw(ctx, mod.base, magic, sizeof magic) == MEM_OK) {
        LOG("[-] bytes: %02x %02x %02x %02x",
               magic[0], magic[1], magic[2], magic[3]);
    } else {
        LOG("[+] mem_read_raw failed");
    }

    mem_destroy(ctx);
}
