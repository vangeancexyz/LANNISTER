#include <dlfcn.h>
#include <stddef.h>

typedef void (*MsgFn)(const char* pMsg, ...);
MsgFn EngineMsg = NULL;

__attribute__((constructor))
void init(void) {
    void *tier0 = dlopen("libtier0.so", RTLD_NOLOAD | RTLD_NOW);
    if (!tier0) return;

    EngineMsg = (MsgFn)dlsym(tier0, "Msg");
    if (!EngineMsg) return;

    EngineMsg("[LANNISTER] Hello EngineMsg!\n");
}
