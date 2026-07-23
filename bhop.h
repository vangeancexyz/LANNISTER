#ifndef BHOP_H
#define BHOP_H

#include <stdbool.h>
#include "interfaces.h"

#define OFF_CLIENTMODE 0x1046900

// IClientMode::CreateMove
#define IDX_CREATE_MOVE 22
#define IN_JUMP (1 << 1)

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
