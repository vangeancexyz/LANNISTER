#include "mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/uio.h>

struct mem_ctx {
    pid_t pid;
    char  name[256];
};

/* reads a line from /proc/maps and extracts start/end */
static int parse_maps_range(const char *line, uintptr_t *start, uintptr_t *end) {
    const char *dash  = strchr(line, '-');
    const char *space = strchr(line, ' ');
    if (!dash || !space || space <= dash) return MEM_ERR_IO;

    char start_buf[32], end_buf[32];
    size_t start_len = (size_t)(dash - line);
    size_t end_len   = (size_t)(space - (dash + 1));
    if (start_len >= sizeof(start_buf) || end_len >= sizeof(end_buf)) return MEM_ERR_IO;

    memcpy(start_buf, line, start_len);       start_buf[start_len] = '\0';
    memcpy(end_buf,   dash + 1, end_len);     end_buf[end_len]     = '\0';

    char *e1, *e2;
    errno = 0;
    uintptr_t s = (uintptr_t)strtoull(start_buf, &e1, 16);
    uintptr_t e = (uintptr_t)strtoull(end_buf,   &e2, 16);
    if (errno != 0 || e1 == start_buf || e2 == end_buf) return MEM_ERR_IO;

    *start = s;
    *end   = e;
    return MEM_OK;
}

mem_ctx_t *mem_create(const char *process_name) {
    mem_ctx_t *ctx = malloc(sizeof *ctx);
    if (!ctx) return NULL;
    ctx->pid = getpid();
    snprintf(ctx->name, sizeof ctx->name, "%s", process_name);
    return ctx;
}

void mem_destroy(mem_ctx_t *ctx) { free(ctx); }

int mem_attach(mem_ctx_t *ctx) {
    ctx->pid = getpid();
    return MEM_OK;
}

pid_t mem_pid(const mem_ctx_t *ctx) { return ctx->pid; }

int mem_get_module(mem_ctx_t *ctx, const char *module_name, mem_module_t *out) {
    (void)ctx;

    out->base = 0;
    out->size = 0;

    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return MEM_ERR_IO;

    char line[1024];
    uintptr_t min_addr = (uintptr_t)-1;
    uintptr_t max_addr = 0;
    int found = 0;

    while (fgets(line, sizeof line, f) != NULL) {
        if (strstr(line, module_name) == NULL) continue;

        uintptr_t start, end;
        if (parse_maps_range(line, &start, &end) != MEM_OK) continue;

        if (start < min_addr) min_addr = start;
        if (end   > max_addr) max_addr = end;
        found = 1;
    }
    fclose(f);

    if (!found) return MEM_ERR_NOT_FOUND;
    out->base = min_addr;
    out->size = max_addr - min_addr;
    return MEM_OK;
}

int mem_read_raw(mem_ctx_t *ctx, uintptr_t address, void *buf, size_t size) {
    (void)ctx;
    if (address == 0 || size == 0) return MEM_ERR_IO;

    struct iovec local  = { buf,             size };
    struct iovec remote = { (void *)address, size };

    ssize_t r = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
    return (r == (ssize_t)size) ? MEM_OK : MEM_ERR_IO;
}
