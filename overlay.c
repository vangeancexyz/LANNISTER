#include "overlay.h"

#include <stdint.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

struct overlay {
	Display *dpy;
	Window win;
	int width, height;
	SDL_Window *window;
	SDL_Renderer *renderer;
};

static Window create_transparent_window(Display *dpy, const overlay_config_t *cfg, XVisualInfo *vinfo) {
	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);

	if (!XMatchVisualInfo(dpy, screen, 32, TrueColor, vinfo)) return 0;

	Colormap colormap = XCreateColormap(dpy, root, vinfo->visual, AllocNone);

	XSetWindowAttributes attrs;
	attrs.colormap = colormap;
	attrs.background_pixel = 0;
	attrs.border_pixel = 0;
	attrs.override_redirect = True;

	Window win = XCreateWindow(dpy, root, cfg->x, cfg->y, cfg->width, cfg->height, 0,
							   vinfo->depth, InputOutput, vinfo->visual,
							CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect, &attrs);
	if (win == 0) return 0;

	Atom wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom wm_window_type_tooltip = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
	XChangeProperty(dpy, win, wm_window_type, XA_ATOM, 32, PropModeReplace,
					(unsigned char *)&wm_window_type_tooltip, 1);

	XserverRegion region = XFixesCreateRegion(dpy, NULL, 0);
	XFixesSetWindowShapeRegion(dpy, win, ShapeInput, 0, 0, region);
	XFixesDestroyRegion(dpy, region);

	XMapWindow(dpy, win);
	XSync(dpy, False);

	return win;
}

overlay_t *overlay_create(const overlay_config_t *cfg) {
	setenv("SDL_VIDEODRIVER", "x11", 1);

	overlay_t *ov = calloc(1, sizeof *ov);
	if (!ov) return NULL;

	ov->width  = cfg->width;
	ov->height = cfg->height;

	ov->dpy = XOpenDisplay(NULL);
	if (!ov->dpy) { free(ov); return NULL; }

	XVisualInfo vinfo;
	ov->win = create_transparent_window(ov->dpy, cfg, &vinfo);
	if (ov->win == 0) { XCloseDisplay(ov->dpy); free(ov); return NULL; }

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		XCloseDisplay(ov->dpy);
		free(ov);
		return NULL;
	}

	ov->window = SDL_CreateWindowFrom((void *)(uintptr_t)ov->win);
	if (!ov->window) {
		SDL_Quit();
		XCloseDisplay(ov->dpy);
		free(ov);
		return NULL;
	}

	ov->renderer = SDL_CreateRenderer(ov->window, -1,
									  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!ov->renderer) {
		SDL_DestroyWindow(ov->window);
		SDL_Quit();
		XCloseDisplay(ov->dpy);
		free(ov);
		return NULL;
	}
	SDL_SetRenderDrawBlendMode(ov->renderer, SDL_BLENDMODE_BLEND);

	return ov;
}

void overlay_destroy(overlay_t *ov) {
	if (!ov) return;
	if (ov->renderer) SDL_DestroyRenderer(ov->renderer);
	if (ov->window)   SDL_DestroyWindow(ov->window);
	SDL_Quit();
	if (ov->dpy) XCloseDisplay(ov->dpy);
	free(ov);
}

SDL_Renderer *overlay_renderer(overlay_t *ov) {
	return ov->renderer;
}

// drains the SDL event queue once per frame -- SDL_PollEvent empties the
// queue, so this must be the only place in the program reading it
// overlay_event_batch_t overlay_poll_events(overlay_t *ov) {
// 	(void)ov;
// 	overlay_event_batch_t batch;
// 	batch.count          = 0;
// 	batch.quit_requested = false;
//
// 	SDL_Event event;
// 	while (batch.count < OVERLAY_MAX_EVENTS && SDL_PollEvent(&event)) {
// 		if (event.type == SDL_QUIT) batch.quit_requested = true;
// 		batch.items[batch.count++] = event;
// 	}
// 	return batch;
// }
overlay_event_batch_t overlay_poll_events(overlay_t *ov) {
	(void)ov;
	overlay_event_batch_t batch;
	batch.count          = 0;
	batch.quit_requested = false;

	return batch;
}

void overlay_frame_begin(overlay_t *ov) {
	SDL_SetRenderDrawColor(ov->renderer, 0, 0, 0, 0);
	SDL_RenderClear(ov->renderer);
}

void overlay_frame_end(overlay_t *ov) {
	SDL_RenderPresent(ov->renderer);
}
