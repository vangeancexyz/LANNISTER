#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "interfaces.h"

// decompiled sub_FDD70 in vguimatsurface.so: sub_118490(&unk_3044F0, sub_103980, "VGUI_Surface030")
#define VGUI_SURFACE_VERSION "VGUI_Surface030"

// decompiled sub_3884F0 in engine.so: sub_663B60(..., "VEngineVGui002") / "VEngineVGui001", both resolve to the same object.
#define VENGINE_VGUI_VERSION "VEngineVGui002"

// IEngineVGui::Paint -- idx 15, confirmed two independent ways: (1)
// vtable position counted from off_8E3D28 in engine.so, (2) the
// function's own body embeds its qualified name via a VPROF telemetry
// call ("(%s)%s", "VGUI", "CEngineVGui::Paint") -- not just position,
// the binary names itself. Highest-confidence index in this project.
#define IDX_PAINT 15

// ISurface (CMatSystemSurface) draw methods -- confirmed via RTTI
// (`vtable for'CMatSystemSurface) + decompilation against this exact binary
#define IDX_DRAW_SET_COLOR     10
#define IDX_DRAW_FILLED_RECT   12
#define IDX_DRAW_OUTLINED_RECT 14
#define IDX_DRAW_LINE          15
#define IDX_DRAW_SET_TEXT_FONT  17
#define IDX_DRAW_SET_TEXT_COLOR 18 // (int,int,int,int) overload
#define IDX_DRAW_SET_TEXT_POS   20
#define IDX_DRAW_PRINT_TEXT     22

// CreateFont() = idx 71, SetFontGlyphSet = idx 72.
#define IDX_CREATE_FONT         71
#define IDX_SET_FONT_GLYPH_SET  72

typedef void (*PaintFn)(Interface_t* thisptr, unsigned int mode);
typedef void (*DrawSetColorFn)(Interface_t* thisptr, int r, int g, int b, int a);
typedef void (*DrawFilledRectFn)(Interface_t* thisptr, int x0, int y0, int x1, int y1);
typedef void (*DrawOutlinedRectFn)(Interface_t* thisptr, int x0, int y0, int x1, int y1);
typedef void (*DrawLineFn)(Interface_t* thisptr, int x0, int y0, int x1, int y1);

typedef unsigned long HFont_t;
typedef HFont_t (*CreateFontFn)(Interface_t* thisptr);

typedef void (*SetFontGlyphSetFn)(Interface_t* thisptr, HFont_t font, const char *fontName,
								  int tall, int weight, int blur, int scanlines,
								  int flags, int rangeMax);

typedef void (*DrawSetTextFontFn)(Interface_t* thisptr, HFont_t font);
typedef void (*DrawSetTextColorFn)(Interface_t* thisptr, int r, int g, int b, int a);
typedef void (*DrawSetTextPosFn)(Interface_t* thisptr, int x, int y);

// text is 16-bit units (Valve's own wchar_t convention from the
// Windows-originated SDK), NOT the platform's native wchar_t (32-bit
// on Linux). Build the buffer as uint16_t manually, never use L"..."
// literals or <wchar.h> here.
typedef void (*DrawPrintTextFn)(Interface_t* thisptr, const uint16_t *text, int textLen, int drawType);

extern Interface_t *g_pSurface;
extern Interface_t *g_pEngineVGui;
extern PaintFn Original_Paint;

// finds vguimatsurface.so, resolves VGUI_Surface030, logs the result.
void menu_probe(void);

void Hooked_Paint(Interface_t* thisptr, unsigned int mode);

// finds engine.so's IEngineVGui, installs the dumb Paint hook (idx 15). call after menu_probe().
void menu_install_hook(void);

#endif /* MENU_H */
