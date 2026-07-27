#ifndef BHOP_H
#define BHOP_H

#include <stdbool.h>
#include "interfaces.h"

#define OFF_CLIENTMODE 0x1046900

// IClientMode::CreateMove
#define IDX_CREATE_MOVE 22
#define IN_JUMP (1 << 1)

#define IN_MOVELEFT  (1 << 9)
#define IN_MOVERIGHT (1 << 10)

#define DW_LOCAL_PLAYER 0xf946b0
#define M_F_FLAGS 1072
#define FL_ONGROUND (1 << 0)

#define M_VEC_VELOCITY_X 312
#define M_VEC_VELOCITY_Y 316

#define AIR_MAX_WISHSPEED 30.0f
#define AIR_MIN_SPEED 30.0f

#define MOVE_MAGNITUDE 400.0f

typedef struct {
	void* vtable;            // 0x00
	int command_number;      // 0x08
	int tick_count;          // 0x0C
	float viewangles[3];     // 0x10
	float forwardmove;       // 0x1C
	float sidemove;          // 0x20
	float upmove;            // 0x24
	int buttons;             // 0x28
	char impulse;            // 0x2C
	int weaponselect;        // 0x30
	int weaponsubtype;       // 0x34
	int random_seed;         // 0x38
	short mousedx;           // 0x3C
	short mousedy;           // 0x3E
	bool hasbeenpredicted;   // 0x40
} CUserCmd;

typedef bool (*CreateMoveFn)(Interface_t* thisptr, float flInputSampleTime, CUserCmd* pCmd);

extern CreateMoveFn Original_CreateMove;

bool Hooked_CreateMove(Interface_t* thisptr, float flInputSampleTime, CUserCmd* pCmd);

void bhop_init(void);

#endif /* BHOP_H */
