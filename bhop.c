#include "bhop.h"
#include "util.h"
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

CreateMoveFn Original_CreateMove = NULL;
static uintptr_t g_client_base = 0;

static float apply_auto_strafe(uintptr_t local_player, CUserCmd *pCmd) {
	float vel_x = *(float*)(local_player + M_VEC_VELOCITY_X);
	float vel_y = *(float*)(local_player + M_VEC_VELOCITY_Y);

	if (!isfinite(vel_x) || !isfinite(vel_y)) return -1.0f;
	if (fabsf(vel_x) > 5000.0f || fabsf(vel_y) > 5000.0f) return -1.0f;

	float speed2d = sqrtf(vel_x * vel_x + vel_y * vel_y);
	if (speed2d < AIR_MIN_SPEED) return speed2d;

	float view_yaw = pCmd->viewangles[1]; // read-only, never written back

	float vel_yaw = atan2f(vel_y, vel_x) * (180.0f / (float)M_PI);

	// where does the view currently lead/lag the velocity?
	float yaw_delta = view_yaw - vel_yaw;
	while (yaw_delta > 180.0f)  yaw_delta -= 360.0f;
	while (yaw_delta < -180.0f) yaw_delta += 360.0f;
	float auto_strafe_dir = (yaw_delta >= 0.0f) ? 1.0f : -1.0f;

	bool holding_left = (pCmd->buttons & IN_MOVELEFT) != 0;
	bool holding_right = (pCmd->buttons & IN_MOVERIGHT) != 0;
	float strafe_dir = auto_strafe_dir;
	if (holding_left && !holding_right) {
		strafe_dir = -1.0f;
	} else if (holding_right && !holding_left) {
		strafe_dir = 1.0f;
	}

	float ratio = AIR_MAX_WISHSPEED / speed2d;
	if (ratio > 1.0f) ratio = 1.0f;
	if (ratio < -1.0f) ratio = -1.0f;
	float optimal_angle_deg = asinf(ratio) * (180.0f / (float)M_PI);

	float wish_yaw_rel_deg = vel_yaw + strafe_dir * (90.0f - optimal_angle_deg) - view_yaw;
	float wish_yaw_rel_rad = wish_yaw_rel_deg * ((float)M_PI / 180.0f);

	if (!isfinite(wish_yaw_rel_rad)) return -1.0f;

	pCmd->forwardmove = MOVE_MAGNITUDE * cosf(wish_yaw_rel_rad);
	pCmd->sidemove = -MOVE_MAGNITUDE * sinf(wish_yaw_rel_rad);

	return speed2d;
}

bool Hooked_CreateMove(Interface_t* thisptr, float flInputSampleTime, CUserCmd* pCmd) {
	bool result = Original_CreateMove(thisptr, flInputSampleTime, pCmd);

	if (!pCmd || pCmd->command_number == 0) return result;

	static int tick_since_log = 0;
	bool should_log = (++tick_since_log >= 16); // ~4x/sec at 64 ticks
	if (should_log) tick_since_log = 0;

	static bool was_grounded = true;

	if (g_client_base) {
		uintptr_t local_player = *(uintptr_t*)(g_client_base + DW_LOCAL_PLAYER);
		if (local_player) {
			int flags = *(int*)(local_player + M_F_FLAGS);
			bool grounded = (flags & FL_ONGROUND) != 0;

			if (grounded) {
				was_grounded = true;
			} else {
				was_grounded = false;
				apply_auto_strafe(local_player, pCmd);
			}
		}
	}

	if (pCmd->buttons & IN_JUMP) {
		if (pCmd->tick_count % 2 == 0) {
			pCmd->buttons &= ~IN_JUMP;
		}
	}

	return result;
}

void bhop_init(void) {
	uintptr_t client_base = get_module_base("client.so");
	if (!client_base) {
		EngineMsg("[LANNISTER] client.so base not found\n");
		return;
	}
	EngineMsg("[LANNISTER] client.so base: %p\n", (void*)client_base);
	g_client_base = client_base;

	Interface_t **ppClientMode = (Interface_t **)(client_base + OFF_CLIENTMODE);
	Interface_t *g_pClientMode = *ppClientMode;
	if (!g_pClientMode) {
		EngineMsg("[LANNISTER] IClientMode ptr is NULL at offset 0x%X\n", OFF_CLIENTMODE);
		return;
	}
	EngineMsg("[LANNISTER] IClientMode: %p\n", (void*)g_pClientMode);

	void **clientmode_vtable = g_pClientMode->vtable;
	Original_CreateMove = (CreateMoveFn)clientmode_vtable[IDX_CREATE_MOVE];

	size_t page_size = sysconf(_SC_PAGESIZE);
	void *page_start = (void *)((uintptr_t)&clientmode_vtable[IDX_CREATE_MOVE] & ~(page_size - 1));

	mprotect(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
	clientmode_vtable[IDX_CREATE_MOVE] = (void*)Hooked_CreateMove;
	mprotect(page_start, page_size, PROT_READ | PROT_EXEC);

	EngineMsg("[LANNISTER] CreateMove hook (bhop) installed at idx %d\n", IDX_CREATE_MOVE);
}
