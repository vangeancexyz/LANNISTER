#include "util.h"
#include <stdio.h>
#include <string.h>

uintptr_t get_module_base(const char *module_name) {
	FILE *f = fopen("/proc/self/maps", "r");
	if (!f) return 0;

	char line[1024];
	uintptr_t base = 0;
	while (fgets(line, sizeof(line), f)) {
		if (strstr(line, module_name)) {
			sscanf(line, "%lx", &base);
			break;
		}
	}
	fclose(f);
	return base;
}
