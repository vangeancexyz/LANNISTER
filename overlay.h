#ifndef OVERLAY_H
#define OVERLAY_H

#include <stdbool.h>
#include <SDL2/SDL.h>

// opaque type, same pattern as mem_ctx_t
typedef struct overlay overlay_t;

typedef struct {
	int x, y;          // monitor position (top-left corner)
	int width, height; // must match the game's actual resolution
} overlay_config_t;

#define OVERLAY_MAX_EVENTS 64

typedef struct {
	SDL_Event items[OVERLAY_MAX_EVENTS];
	int count;
	bool quit_requested;
} overlay_event_batch_t;

overlay_t *overlay_create(const overlay_config_t *cfg) __attribute__((warn_unused_result));
void overlay_destroy(overlay_t *ov);

SDL_Renderer *overlay_renderer(overlay_t *ov);

overlay_event_batch_t overlay_poll_events(overlay_t *ov);

void overlay_frame_begin(overlay_t *ov); // clears the frame
void overlay_frame_end(overlay_t *ov);   // presents the frame

#endif /* OVERLAY_H */
