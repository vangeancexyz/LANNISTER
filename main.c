#include <dlfcn.h>
#include <stddef.h>

#include "interfaces.h"

MsgFn EngineMsg = NULL;

__attribute__((constructor))
void init(void) {
    void *tier0 = dlopen("libtier0.so", RTLD_NOLOAD | RTLD_NOW);
    if (!tier0) return;

    EngineMsg = (MsgFn)dlsym(tier0, "Msg");
    if (!EngineMsg) return;

    EngineMsg("[LANNISTER] Hello EngineMsg!\n");

    void *engine = dlopen("engine.so", RTLD_NOLOAD | RTLD_NOW);
    if (!engine) {
        EngineMsg("[LANNISTER] engine.so load failed\n");
        return;
    }

    CreateInterfaceFn CreateInterface = (CreateInterfaceFn)dlsym(engine, "CreateInterface");
    if (!CreateInterface) {
        EngineMsg("[LANNISTER] CreateInterface symbol missing\n");
        return;
    }

    // NULL = version string wrong for this binary
    void *g_pModelRender = CreateInterface("VEngineModel016", NULL);
    EngineMsg("[LANNISTER] VEngineModel016 -> %p\n", g_pModelRender);
}
