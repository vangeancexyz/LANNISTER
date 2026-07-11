#ifndef MEM_H
#define MEM_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct mem_ctx mem_ctx_t;

typedef struct {
    uintptr_t base;
    size_t size;
} mem_module_t;

// POSIX/ISO: 0 = sucess, < 0 = error
enum {
    MEM_OK            =  0,
    MEM_ERR_NOT_FOUND = -1,
    MEM_ERR_IO        = -2
};

mem_ctx_t *mem_create(const char *process_name) __attribute__((warn_unused_result));
void       mem_destroy(mem_ctx_t *ctx);

int   mem_attach(mem_ctx_t *ctx) __attribute__((warn_unused_result));
pid_t mem_pid(const mem_ctx_t *ctx);

int mem_get_module(mem_ctx_t *ctx, const char *module_name, mem_module_t *out)
    __attribute__((warn_unused_result));

int mem_read_raw(mem_ctx_t *ctx, uintptr_t address, void *buf, size_t size)
    __attribute__((warn_unused_result));

#define MEM_READ(ctx, addr, type)                                             \
    ({                                                                        \
        type _mem_buf;                                                        \
        int _mem_rc = mem_read_raw((ctx), (addr), &_mem_buf, sizeof(type));   \
        (void) _mem_rc;                                                       \
        _mem_buf;                                                             \
    })

#endif /* MEM_H */
