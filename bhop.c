#include "bhop.h"
#include "util.h"
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

CreateMoveFn Original_CreateMove = NULL;

bool Hooked_CreateMove(Interface_t* thisptr, float flInputSampleTime, CUserCmd* pCmd) {
	bool result = Original_CreateMove(thisptr, flInputSampleTime, pCmd);

	if (!pCmd || pCmd->command_number == 0) return result;

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
