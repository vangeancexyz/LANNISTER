#include "menu.h"
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

Interface_t *g_pSurface = NULL;
Interface_t *g_pEngineVGui = NULL;
PaintFn Original_Paint = NULL;
static HFont_t g_menuFont = 0;
static bool g_fontReady = false;

void menu_probe(void) {
	void *matsurface_lib = dlopen("vguimatsurface.so", RTLD_NOLOAD | RTLD_NOW);
	if (!matsurface_lib) {
		EngineMsg("[LANNISTER] vguimatsurface.so load failed\n");
		return;
	}

	CreateInterfaceFn MatSurfaceFactory = (CreateInterfaceFn)dlsym(matsurface_lib, "CreateInterface");
	if (!MatSurfaceFactory) {
		EngineMsg("[LANNISTER] CreateInterface missing in vguimatsurface.so\n");
		return;
	}

	g_pSurface = (Interface_t*)MatSurfaceFactory(VGUI_SURFACE_VERSION, NULL);
	if (!g_pSurface) {
		EngineMsg("[LANNISTER] %s not found\n", VGUI_SURFACE_VERSION);
		return;
	}

	EngineMsg("[LANNISTER] ISurface: %p\n", (void*)g_pSurface);
}

static void ensure_menu_font(void) {
	if (g_fontReady) return;

	void **surf_vtable = g_pSurface->vtable;
	CreateFontFn CreateFont = (CreateFontFn)surf_vtable[IDX_CREATE_FONT];
	SetFontGlyphSetFn SetFontGlyphSet = (SetFontGlyphSetFn)surf_vtable[IDX_SET_FONT_GLYPH_SET];

	g_menuFont = CreateFont(g_pSurface);
	if (!g_menuFont) {
		EngineMsg("[LANNISTER] CreateFont returned 0\n");
		return;
	}

	// SetFontGlyphSetFn. tall=18, weight=500 (semi-bold), blur=0,
	// scanlines=0, flags=0, rangeMax=0.
	SetFontGlyphSet(g_pSurface, g_menuFont, "Tahoma", 18, 500, 0, 0, 0, 0);

	EngineMsg("[LANNISTER] font created: handle=%lu\n", g_menuFont);
	g_fontReady = true;
}

void Hooked_Paint(Interface_t* thisptr, unsigned int mode) {
	Original_Paint(thisptr, mode);

	if (!g_pSurface) return;

	ensure_menu_font();

	void **surf_vtable = g_pSurface->vtable;
	DrawSetColorFn DrawSetColor = (DrawSetColorFn)surf_vtable[IDX_DRAW_SET_COLOR];
	DrawFilledRectFn DrawFilledRect = (DrawFilledRectFn)surf_vtable[IDX_DRAW_FILLED_RECT];
	DrawOutlinedRectFn DrawOutlinedRect = (DrawOutlinedRectFn)surf_vtable[IDX_DRAW_OUTLINED_RECT];

	// hello world
	int x0 = 100, y0 = 100, x1 = 400, y1 = 300;

	DrawSetColor(g_pSurface, 20, 20, 20, 220); // dark, mostly opaque fill
	DrawFilledRect(g_pSurface, x0, y0, x1, y1);

	DrawSetColor(g_pSurface, 0, 255, 0, 255); // green border
	DrawOutlinedRect(g_pSurface, x0, y0, x1, y1);

	// text: font handle is a placeholder (0) -- see menu.h comment,
	// CreateFont's index isn't confirmed yet. Manually building the
	// string as uint16_t, NOT using wchar_t/L"..." (Linux wchar_t is
	// 32-bit, the engine's own convention from its Windows-originated
	// SDK is 16-bit).
	DrawSetTextFontFn DrawSetTextFont = (DrawSetTextFontFn)surf_vtable[IDX_DRAW_SET_TEXT_FONT];
	DrawSetTextColorFn DrawSetTextColor = (DrawSetTextColorFn)surf_vtable[IDX_DRAW_SET_TEXT_COLOR];
	DrawSetTextPosFn DrawSetTextPos = (DrawSetTextPosFn)surf_vtable[IDX_DRAW_SET_TEXT_POS];
	DrawPrintTextFn DrawPrintText = (DrawPrintTextFn)surf_vtable[IDX_DRAW_PRINT_TEXT];

	static const char *title_ascii = "LANNISTER";
	uint16_t title_wide[16];
	int title_len = 0;
	for (; title_ascii[title_len] && title_len < 15; title_len++) {
		title_wide[title_len] = (uint16_t)(unsigned char)title_ascii[title_len];
	}

	DrawSetTextFont(g_pSurface, g_menuFont);
	DrawSetTextColor(g_pSurface, 255, 255, 255, 255);
	DrawSetTextPos(g_pSurface, x0 + 10, y0 + 10);
	DrawPrintText(g_pSurface, title_wide, title_len, 0);
}

void menu_install_hook(void) {
	// own engine.so handle, resolved independently
	void *engine_lib = dlopen("engine.so", RTLD_NOLOAD | RTLD_NOW);
	if (!engine_lib) {
		EngineMsg("[LANNISTER] menu: engine.so load failed\n");
		return;
	}

	CreateInterfaceFn EngineFactory = (CreateInterfaceFn)dlsym(engine_lib, "CreateInterface");
	if (!EngineFactory) {
		EngineMsg("[LANNISTER] menu: CreateInterface missing in engine.so\n");
		return;
	}

	g_pEngineVGui = (Interface_t*)EngineFactory(VENGINE_VGUI_VERSION, NULL);
	if (!g_pEngineVGui) {
		EngineMsg("[LANNISTER] %s not found\n", VENGINE_VGUI_VERSION);
		return;
	}
	EngineMsg("[LANNISTER] IEngineVGui: %p\n", (void*)g_pEngineVGui);

	void **vtable = g_pEngineVGui->vtable;
	Original_Paint = (PaintFn)vtable[IDX_PAINT];

	size_t page_size = sysconf(_SC_PAGESIZE);
	void *page_start = (void *)((uintptr_t)&vtable[IDX_PAINT] & ~(page_size - 1));

	mprotect(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
	vtable[IDX_PAINT] = (void*)Hooked_Paint;
	mprotect(page_start, page_size, PROT_READ | PROT_EXEC);

	EngineMsg("[LANNISTER] Paint hook installed at idx %d\n", IDX_PAINT);
}
