#include <dlfcn.h>

#include "interfaces.h"
#include "chams.h"
#include "bhop.h"
#include "menu.h"

MsgFn EngineMsg = NULL;

__attribute__((constructor))
void init(void) {
    void *tier0 = dlopen("libtier0.so", RTLD_NOLOAD | RTLD_NOW);
    if (!tier0) return;

    EngineMsg = (MsgFn)dlsym(tier0, "Msg");
    if (!EngineMsg) return;

    EngineMsg("[LANNISTER] Hello EngineMsg!\n");

    void *engine = dlopen("engine.so", RTLD_NOLOAD | RTLD_NOW);
    if (!engine)
        EngineMsg("[LANNISTER] engine.so load failed\n");

    void *matsys = dlopen("materialsystem.so", RTLD_NOLOAD | RTLD_NOW);
    if (!matsys)
        EngineMsg("[LANNISTER] materialsystem.so load failed\n");

    CreateInterfaceFn EngineFactory = engine
        ? (CreateInterfaceFn)dlsym(engine, "CreateInterface")
        : NULL;
    if (!EngineFactory)
        EngineMsg("[LANNISTER] CreateInterface missing in engine.so\n");

    CreateInterfaceFn MatSysFactory = matsys
        ? (CreateInterfaceFn)dlsym(matsys, "CreateInterface")
        : NULL;
    if (!MatSysFactory)
        EngineMsg("[LANNISTER] CreateInterface missing in materialsystem.so\n");

    Interface_t *model_render = NULL;
    Interface_t *material_system = NULL;
    Interface_t *render_view = NULL;

    if (EngineFactory) {
        model_render = (Interface_t*)EngineFactory("VEngineModel016", NULL);
        render_view = (Interface_t*)EngineFactory("VEngineRenderView014", NULL);
    }
    if (MatSysFactory)
        material_system = (Interface_t*)MatSysFactory("VMaterialSystem080", NULL);

    EngineMsg("[LANNISTER] ModelRender=%p | MaterialSystem=%p | RenderView=%p\n",
              model_render, material_system, render_view);

    if (!chams_initialize(model_render, material_system, render_view))
        EngineMsg("[LANNISTER] Chams unavailable; independent features continue\n");

    bhop_init();

    menu_probe();
    menu_install_hook();
}
