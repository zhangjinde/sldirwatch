/** \file sldirwatch.h
 * very simple file change notification library
 */

#ifndef __SL_DIRWATCH_H
#define __SL_DIRWATCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* maximum size of file and directory pathes, in utf8 */
#define SLDIRWATCH_PATH_SIZE 256

#ifdef _WIN32
#if _WIN32_WINNT <= 0x0400
#define _WIN32_WINNT 0x0400
#endif
#include <windows.h>
#endif

/** Callback function, called from sldirwatch_poll_watchpoints when event
 * occurs.
 *  \param filename name of file that triggered an event (file that was
 *    modified)
 *  \param data constant stored in watchpoint. Could be used to determine
 *    which watchpoint triggered a callback
 */
typedef void (sldirwatch_callback_f)(const char *filename, int data);

enum sldirwatch_flags_e {
	SLDIRWATCH_MERGE_PATHS_BIT = 1,
	SLDIRWATCH_SKIP_HIDDEN_BIT = 1 << 1
};

/* None of these structures fields should be accessed directly - it's data
 * formats and usage are implementation-specific. Defined here only to make
 * caller-side know exact size of context structure at compile time
 */
typedef struct {
	char path[SLDIRWATCH_PATH_SIZE];
	int wd;
	int data;
	unsigned int flags;
	sldirwatch_callback_f *cb;
#ifdef _WIN32
#define SLDIRWATCH_WATCHBUFFER_SIZE 1024
	HANDLE dir;
	OVERLAPPED ovp;
	char lpBuffer[SLDIRWATCH_WATCHBUFFER_SIZE];
	DWORD returnedBytes;
#endif
} sldirwatch_watchpoint_t;

typedef struct {
	int locked;
	int fd;
	int max_watchpoints, num_watchpoints;

#ifdef _WIN32
	/* store last triggered notification, because for some reason we getting
	 * filechange notification twice */
	char lastnotify_path[SLDIRWATCH_PATH_SIZE];
	unsigned int lastnotify_callno, callno;
#endif

	sldirwatch_watchpoint_t *watchpoints;
} sldirwatch_t;

/* how much memory context needs */
#define SLDIRWATCH_SIZE(max_watchpoints) (sizeof(sldirwatch_t) + sizeof(sldirwatch_watchpoint_t) * (max_watchpoints))

/** Init context for given max_watchpoints. ctx memory size must be enough
 * (at least SLDIRWATCH_SIZE(max_watchpoints)).
 * \return 0 on success */
int sldirwatch_init(sldirwatch_t *ctx, int max_watchpoints);
/** Deinit context. */
void sldirwatch_deinit(sldirwatch_t *ctx);

/** Add a watchpoint
 *  \param ctx context to add watchpoint into
 *  \param path path to directory to watch
 *  \param callback callback to fire when file notification event occurs
 *  \param data watchpoint-specific constant to pass into a callback
 *  \param flags bitfield of flags (see \c sldirwatch_flags_e)
 *  \return 0 on success
 */
int sldirwatch_add_watchpoint(sldirwatch_t *ctx, const char *path, sldirwatch_callback_f *callback, int data, unsigned int flags);

/** Poll every watchpoint for new events, and fire callbacks when events occur.
 *  \param ctx working context
 *  \return total number of processed events, or -1 on error
 */
int sldirwatch_poll_watchpoints(sldirwatch_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
