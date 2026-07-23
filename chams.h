#ifndef CHAMS_H
#define CHAMS_H

#include <stdbool.h>
#include "interfaces.h"

// resolved via GetGameDirectory
#define IDX_DRAW_MODEL_EXECUTE   19
#define IDX_FORCED_MAT_OVERRIDE  1
#define IDX_FIND_MATERIAL        71
#define IDX_COLOR_MODULATE       28
#define IDX_INCREMENT_REF_COUNT  12

#define XRAY_MATERIAL_NAME "models/lannister_xray"

extern Interface_t *g_pModelRender;
extern DrawModelExecuteFn Original_DrawModelExecute;
extern void *g_pXrayMaterial;

bool write_xray_vmt(const char *game_dir);

void Hooked_DrawModelExecute(Interface_t* thisptr, const void* state, const void* pInfo, void* pCustomBoneToWorld);

void chams_install_hook(void);

#endif /* CHAMS_H */
