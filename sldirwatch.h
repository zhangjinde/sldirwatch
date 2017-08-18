/** \file sldirwatch.h
 * very simple file change notification library
 */

#ifndef __SL_DIRWATCH_H
#define __SL_DIRWATCH_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SLDIRWATCH_ASSERT
	#include <assert.h>
	#define SLDIRWATCH_ASSERT(x) assert(x)
#endif

/* build sldirwatch as static functions */
#ifdef SLDIRWATCH_STATIC
	#ifdef __GNUC__
		#define SLDIRWATCH_UNUSED_FUNC __attribute__ ((unused))
	#else
		#define SLDIRWATCH_UNUSED_FUNC
	#endif

	#define SLDIRWATCH_FUNC static SLDIRWATCH_UNUSED_FUNC
#else
	#define SLDIRWATCH_FUNC
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
 *  \param ud_ptr constant stored in watchpoint. Could be used to determine
 *    which watchpoint triggered a callback
 *  \param ud_int constant stored in watchpoint.
 */
typedef void (sldirwatch_callback_f)(const char *filename, void *ud_ptr, int ud_int);

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
	void *ud_ptr;
	int ud_int;
	unsigned int flags;
	sldirwatch_callback_f *cb;
#ifdef _WIN32
	HANDLE dir;
	OVERLAPPED ovp;
	char lpBuffer[1024];
	DWORD returnedBytes;
#endif
} sldirwatch_watchpoint_t;

typedef struct {
	int watchpoint_id;
	/* watchpoint directory + relative filename (fopen()'able)*/
	char filename[SLDIRWATCH_PATH_SIZE];
	/* filename relative to watchpoint directory */
	char relative_filename[SLDIRWATCH_PATH_SIZE];
} sldirwatch_event_t;

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

	/* max amount of queued events is tied to buffer sizes; don't reduce */
	int num_queued_events;
	sldirwatch_event_t queued_events[32];
} sldirwatch_t;

/* how much memory context needs */
#define SLDIRWATCH_SIZE(max_watchpoints) (sizeof(sldirwatch_t) + sizeof(sldirwatch_watchpoint_t) * (max_watchpoints))

/** Init context for given max_watchpoints. ctx memory size must be enough
 * (at least SLDIRWATCH_SIZE(max_watchpoints)).
 * \return 1 on success */
SLDIRWATCH_FUNC int sldirwatch_init(sldirwatch_t *ctx, int max_watchpoints);
/** Deinit context. */
SLDIRWATCH_FUNC void sldirwatch_deinit(sldirwatch_t *ctx);

/** Add a watchpoint with callback
 *  \param ctx context to add watchpoint into
 *  \param path path to directory to watch
 *  \param callback callback to fire when file notification event occurs
 *  \param ud_ptr watchpoint-specific constant to be passed into a callback
 *  \param ud_int watchpoint-specific constant to be passed into a callback
 *  \param flags bitfield of flags (see \c sldirwatch_flags_e)
 *  \return watchpoint id
 */
SLDIRWATCH_FUNC int sldirwatch_add_watchpoint(sldirwatch_t *ctx, const char *path, unsigned int flags);

/** Set callback to be called each time file modification is detected in
 * specified watchpoint */
SLDIRWATCH_FUNC void sldirwatch_set_callback(sldirwatch_t *ctx, int watchpoint_id, sldirwatch_callback_f *callback, void *ud_ptr, int ud_int);

/** Poll watchpoints and write one event into \c ev. If watchpoint have a
 * callback - it will be called.
 * \return 1 if there was an event
 */
SLDIRWATCH_FUNC int sldirwatch_poll(sldirwatch_t *ctx, sldirwatch_event_t *ev);

#ifdef SLDIRWATCH_STATIC
	#include "sldirwatch.c"
#endif

#ifdef __cplusplus
}
#endif

#endif
