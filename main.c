#include <dlfcn.h>

#include "interfaces.h"
#include "chams.h"
#include "bhop.h"

MsgFn EngineMsg = NULL;
Interface_t *g_pEngineClient = NULL;

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

    g_pModelRender = (Interface_t*)EngineFactory("VEngineModel016", NULL);
    if (!g_pModelRender) {
        EngineMsg("[LANNISTER] VEngineModel016 not found\n");
        return;
    }

    Interface_t *g_pMaterialSystem = (Interface_t*)MatSysFactory("VMaterialSystem080", NULL);
    if (!g_pMaterialSystem) {
        EngineMsg("[LANNISTER] VMaterialSystem080 not found\n");
        return;
    }

    g_pEngineClient = (Interface_t*)EngineFactory("VEngineClient014", NULL);
    if (!g_pEngineClient) {
        EngineMsg("[LANNISTER] VEngineClient014 not found\n");
        return;
    }

    EngineMsg("[LANNISTER] ModelRender: %p | MaterialSystem: %p | EngineClient: %p\n",
              g_pModelRender, g_pMaterialSystem, g_pEngineClient);

    GetGameDirectoryFn GetGameDirectory = (GetGameDirectoryFn)g_pEngineClient->vtable[IDX_GET_GAME_DIRECTORY];
    const char *game_dir = GetGameDirectory(g_pEngineClient);
    EngineMsg("[LANNISTER] game dir: %s\n", game_dir);

    if (!write_xray_vmt(game_dir)) {
        EngineMsg("[LANNISTER] aborting -- vmt write failed, FindMaterial would return the error material\n");
        return;
    }

    FindMaterialFn FindMaterial = (FindMaterialFn)g_pMaterialSystem->vtable[IDX_FIND_MATERIAL];
    g_pXrayMaterial = FindMaterial(g_pMaterialSystem, XRAY_MATERIAL_NAME, "Model textures", true, NULL);
    if (!g_pXrayMaterial) {
        EngineMsg("[LANNISTER] FindMaterial failed for %s\n", XRAY_MATERIAL_NAME);
        return;
    }
    EngineMsg("[LANNISTER] material loaded: %p\n", g_pXrayMaterial);

    // refuses to bind the material at all
    void **mat_vtable = *(void***)g_pXrayMaterial;
    IncrementRefCountFn IncRef = (IncrementRefCountFn)mat_vtable[IDX_INCREMENT_REF_COUNT];
    IncRef(g_pXrayMaterial);

    chams_install_hook();

    bhop_init();
}
