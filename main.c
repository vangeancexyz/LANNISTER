#include <dlfcn.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#include "interfaces.h"

#define IDX_DRAW_MODEL_EXECUTE 19

MsgFn EngineMsg = NULL;
DrawModelExecuteFn Original_DrawModelExecute = NULL;

void Hooked_DrawModelExecute(Interface_t* thisptr, const void* state, const void* pInfo, void* pCustomBoneToWorld) {
    static int log_count = 0;
    if (log_count < 10) {
        EngineMsg("[LANNISTER] DrawModelExecute hit\n");
        log_count++;
    }
    Original_DrawModelExecute(thisptr, state, pInfo, pCustomBoneToWorld);
}

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
    Interface_t *g_pModelRender    = (Interface_t*)EngineFactory("VEngineModel016", NULL);
    Interface_t *g_pMaterialSystem = (Interface_t*)MatSysFactory("VMaterialSystem080", NULL);

    if (!g_pModelRender) {
        EngineMsg("[LANNISTER] VEngineModel016 not found\n");
        return;
    }

    if (!g_pMaterialSystem) {
        EngineMsg("[LANNISTER] VMaterialSystem080 not found\n");
        return;
    }

    EngineMsg("[LANNISTER] ModelRender: %p | MaterialSystem: %p\n", g_pModelRender, g_pMaterialSystem);

    // swap vtable[19]: save original, unlock page, overwrite, relock
    void **vtable = g_pModelRender->vtable;
    Original_DrawModelExecute = (DrawModelExecuteFn)vtable[IDX_DRAW_MODEL_EXECUTE];

    size_t page_size = sysconf(_SC_PAGESIZE);
    void *page_start = (void *)((uintptr_t)&vtable[IDX_DRAW_MODEL_EXECUTE] & ~(page_size - 1));

    mprotect(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    vtable[IDX_DRAW_MODEL_EXECUTE] = (void*)Hooked_DrawModelExecute;
    mprotect(page_start, page_size, PROT_READ | PROT_EXEC);

    EngineMsg("[LANNISTER] hook installed at idx %d\n", IDX_DRAW_MODEL_EXECUTE);
}
