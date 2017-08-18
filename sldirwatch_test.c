/*
 */

#include "sldirwatch.h"
#include <stdio.h>
#include <signal.h>
#ifdef __linux__
int usleep(unsigned int usec);
#endif

static void watchcallback(const char *filename, void *ud_ptr, int ud_int) {
	printf("watchcallback(%s, %p, %d)\n", filename, ud_ptr, ud_int);
}

static int quit;

static void sigint_handler(int sig) {
	if(sig == SIGINT) {
		quit = 1;
	}
}

int main(int argc, char **argv) {
#define MAX_WATCHPOINTS 128

	/* memory reserve for watching context */
	char mempad[SLDIRWATCH_SIZE(MAX_WATCHPOINTS)];
	sldirwatch_t *ctx = (void*)mempad;
	int wp;

	(void)argc, (void)argv;

	signal(SIGINT, &sigint_handler);

	sldirwatch_init(ctx, MAX_WATCHPOINTS);

	wp = sldirwatch_add_watchpoint(ctx, ".",
			SLDIRWATCH_SKIP_HIDDEN_BIT | SLDIRWATCH_MERGE_PATHS_BIT);
	if(!wp) {
		printf("couldn't set watchpoint\n");
		return 1;
	}
	sldirwatch_set_callback(ctx, wp, &watchcallback, NULL, 0);

	while(!quit) {
		while(sldirwatch_poll(ctx, NULL)) {}
#ifdef __linux__
		usleep(1000);
#elif defined(_WIN32)
		Sleep(10);
#endif
	}

	sldirwatch_deinit(ctx);

	return 0;
}
