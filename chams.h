#ifndef CHAMS_H
#define CHAMS_H

#include <stdbool.h>
#include "interfaces.h"

#define IDX_DRAW_MODEL_EXECUTE   19
#define IDX_FORCED_MAT_OVERRIDE  1
#define IDX_FIND_MATERIAL        71
#define IDX_INCREMENT_REF_COUNT  12
#define IDX_DECREMENT_REF_COUNT  13
#define IDX_IS_ERROR_MATERIAL    42
#define IDX_RENDER_SET_BLEND     4
#define IDX_RENDER_SET_COLOR     6

#define HIDDEN_MATERIAL_NAME     "cssourcex64_chams_lannister/invisible"
#define VISIBLE_MATERIAL_NAME    "cssourcex64_chams_lannister/visible"
#define MATERIAL_DIRECTORY_NAME  "cssourcex64_chams_lannister"

extern Interface_t *g_pModelRender;
extern Interface_t *g_pRenderView;
extern DrawModelExecuteFn Original_DrawModelExecute;
extern void *g_pHiddenMaterial;
extern void *g_pVisibleMaterial;
extern _Atomic int g_chams_stage;

bool chams_initialize(Interface_t *model_render,
                      Interface_t *material_system,
                      Interface_t *render_view);

void Hooked_DrawModelExecute(Interface_t* thisptr, const void* state, const void* pInfo, void* pCustomBoneToWorld);

#endif /* CHAMS_H */
