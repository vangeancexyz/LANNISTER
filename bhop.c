#include "bhop.h"
#include "util.h"
#include <stdint.h>

void bhop_probe(void) {
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
}
