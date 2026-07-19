#include <dlfcn.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include "interfaces.h"

#define IDX_DRAW_MODEL_EXECUTE   19
#define IDX_FORCED_MAT_OVERRIDE  1
#define IDX_FIND_MATERIAL        71
#define IDX_COLOR_MODULATE       29
#define IDX_INCREMENT_REF_COUNT  12
#define IDX_GET_GAME_DIRECTORY   35

#define XRAY_MATERIAL_NAME "models/lannister_xray"

MsgFn EngineMsg = NULL;
Interface_t *g_pModelRender = NULL;
DrawModelExecuteFn Original_DrawModelExecute = NULL;
void *g_pXrayMaterial = NULL;

static bool write_xray_vmt(const char *game_dir) {
    char path[1024];
    snprintf(path, sizeof path, "%s/materials/models", game_dir);

    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        EngineMsg("[LANNISTER] mkdir failed: %s (errno=%d: %s)\n", path, errno, strerror(errno));
        return false;
    }

    char vmt_path[1024];
    snprintf(vmt_path, sizeof vmt_path, "%s/lannister_xray.vmt", path);

    FILE *f = fopen(vmt_path, "w");
    if (!f) {
        EngineMsg("[LANNISTER] failed to write vmt: %s\n", vmt_path);
        return false;
    }

    fprintf(f,
            "\"VertexLitGeneric\"\n"
            "{\n"
            "\t\"$basetexture\" \"vgui/white\"\n"
            "\t\"$model\" \"1\"\n"
            "\t\"$ignorez\" \"1\"\n"
            "}\n"
    );
    fclose(f);

    EngineMsg("[LANNISTER] vmt written: %s\n", vmt_path);
    return true;
}

void Hooked_DrawModelExecute(Interface_t* thisptr, const void* state, const void* pInfo, void* pCustomBoneToWorld) {
    if (g_pXrayMaterial && pInfo) {
        int entity_index = *(int*)((uintptr_t)pInfo + 68); // ModelRenderInfo_t::entity_index

        if (entity_index >= 1 && entity_index <= 64) {
            void **mat_vtable = *(void***)g_pXrayMaterial;
            ColorModulateFn ColorModulate = (ColorModulateFn)mat_vtable[IDX_COLOR_MODULATE];
            ColorModulate(g_pXrayMaterial, 1.0f, 0.0f, 1.0f);

            ForcedMaterialOverrideFn Override = (ForcedMaterialOverrideFn)g_pModelRender->vtable[IDX_FORCED_MAT_OVERRIDE];
            Override(g_pModelRender, g_pXrayMaterial, 0);
            Original_DrawModelExecute(thisptr, state, pInfo, pCustomBoneToWorld);
            Override(g_pModelRender, NULL, 0);
            return;
        }
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

    Interface_t *g_pEngineClient = (Interface_t*)EngineFactory("VEngineClient014", NULL);
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
