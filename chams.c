#include "chams.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

Interface_t *g_pModelRender = NULL;
DrawModelExecuteFn Original_DrawModelExecute = NULL;
void *g_pXrayMaterial = NULL;

bool write_xray_vmt(const char *game_dir) {
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

void chams_install_hook(void) {
	void **vtable = g_pModelRender->vtable;
	Original_DrawModelExecute = (DrawModelExecuteFn)vtable[IDX_DRAW_MODEL_EXECUTE];

	size_t page_size = sysconf(_SC_PAGESIZE);
	void *page_start = (void *)((uintptr_t)&vtable[IDX_DRAW_MODEL_EXECUTE] & ~(page_size - 1));

	mprotect(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
	vtable[IDX_DRAW_MODEL_EXECUTE] = (void*)Hooked_DrawModelExecute;
	mprotect(page_start, page_size, PROT_READ | PROT_EXEC);

	EngineMsg("[LANNISTER] hook installed at idx %d\n", IDX_DRAW_MODEL_EXECUTE);
}
