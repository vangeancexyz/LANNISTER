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

    void *matsys = dlopen("materialsystem.so", RTLD_NOLOAD | RTLD_NOW);
    if (!matsys) {
        EngineMsg("[LANNISTER] materialsystem.so load failed\n");
        return;
    }

    CreateInterfaceFn EngineFactory = (CreateInterfaceFn)dlsym(engine, "CreateInterface");
    if (!EngineFactory) {
        EngineMsg("[LANNISTER] CreateInterface missing in engine.so\n");
        return;
    }

    CreateInterfaceFn MatSysFactory = (CreateInterfaceFn)dlsym(matsys, "CreateInterface");
    if (!MatSysFactory) {
        EngineMsg("[LANNISTER] CreateInterface missing in materialsystem.so\n");
        return;
    }

    // NULL = version string wrong for this binary
    void *g_pModelRender    = EngineFactory("VEngineModel016",    NULL);
    void *g_pMaterialSystem = MatSysFactory("VMaterialSystem080", NULL);

    EngineMsg("[LANNISTER] ModelRender: %p | MaterialSystem: %p\n", g_pModelRender, g_pMaterialSystem);
}
