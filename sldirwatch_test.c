/*
 */

#include "sldirwatch.h"
#include <stdio.h>
#ifdef __linux__
int usleep(unsigned int usec);
#endif

static void watchcallback(const char *filename, int data) {
	printf("watchcallback(%s, %d)\n", filename, data);
}

int main(int argc, char **argv) {
#define MAX_WATCHPOINTS 128

	/* memory reserve for watching context */
	char mempad[SLDIRWATCH_SIZE(MAX_WATCHPOINTS)];
	sldirwatch_t *ctx = (void*)mempad;

	(void)argc, (void)argv;

	sldirwatch_init(ctx, MAX_WATCHPOINTS);

	if(sldirwatch_add_watchpoint(ctx, ".", &watchcallback, 0,
				SLDIRWATCH_SKIP_HIDDEN_BIT | SLDIRWATCH_MERGE_PATHS_BIT) != 0) {
		printf("couldn't set watchpoint\n");
		return 1;
	}

	while(1) {
		sldirwatch_poll_watchpoints(ctx);
#ifdef __linux__
		usleep(1000);
#elif defined(_WIN32)
		Sleep(10);
#endif
	}

	sldirwatch_deinit(ctx);

	return 0;
}
