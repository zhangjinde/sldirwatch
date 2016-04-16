#include "sldirwatch.h"
#include <string.h>
#include <limits.h>

#ifdef __linux__
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>

/* inotify's event buffer size */
#define EVENT_BUF_SIZE (8 * (sizeof(struct inotify_event) + 16))

#elif defined(_WIN32)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <wchar.h>

#else
#error "unsupported OS"
#endif

#ifdef __linux__
/* support function - find watchpoint by it's descriptor (got descriptor from
 * inotify
 * returns watchpoint's index
 */
static int search_wd(const sldirwatch_t *ctx, int wd) {
	int i;
	for(i = 0; i != ctx->num_watchpoints; ++i) {
		if(ctx->watchpoints[i].wd == wd) {
			return i;
		}
	}

	return -1;
}
#endif

/* = snprintf(out, out_size, "%s/%s", p0, p1) */
static void join_paths(char *out, int out_size, const char *p0, const char *p1) {
	while(*p0) {
		if(out_size > 1) {
			*out++ = *p0++;
			out_size--;
		} else {
			break;
		}
	}

	if(out_size > 1) {
		*out++ = '/';
		out_size--;
	}

	while(*p1) {
		if(out_size > 1) {
			*out++ = *p1++;
			out_size--;
		} else {
			break;
		}
	}

	*out = '\0';
}

#ifdef _WIN32
static void join_paths_wchar(wchar_t *out, int out_cnt, const wchar_t *p0, const wchar_t *p1) {
	while(*p0) {
		if(out_cnt > 1) {
			*out++ = *p0++;
			out_cnt--;
		} else {
			break;
		}
	}

	if(out_cnt > 1) {
		*out++ = '/';
		out_cnt--;
	}

	while(*p1) {
		if(out_cnt > 1) {
			*out++ = *p1++;
			out_cnt--;
		} else {
			break;
		}
	}

	*out = '\0';
}
#endif


int sldirwatch_init(sldirwatch_t *ctx, int max_watchpoints) {
	memset(ctx, 0, SLDIRWATCH_SIZE(max_watchpoints));

	ctx->max_watchpoints = max_watchpoints;

	/* watchpoints stored just after base sizeof(*ctx) */
	ctx->watchpoints = (sldirwatch_watchpoint_t*)((char*)ctx + sizeof(*ctx));

#ifdef __linux__
	ctx->fd = inotify_init();
#else
	ctx->fd = 1;
#endif

	return !(ctx->fd > 0);
}

void sldirwatch_deinit(sldirwatch_t *ctx) {
	if(ctx->fd <= 0) { return; }

#ifdef __linux__
	/* closing inotify descriptor deinitializes everything inotify-related in
	 * kernel-side
	 */
	close(ctx->fd);
#elif defined(_WIN32)
	{
		int i;
		for(i = 0; i != ctx->num_watchpoints; ++i) {
			CloseHandle(ctx->watchpoints[i].ovp.hEvent);
			CloseHandle(ctx->watchpoints[i].dir);
		}
	}
#endif

	ctx->fd = 0;
}

int sldirwatch_add_watchpoint(sldirwatch_t *ctx, const char *path, sldirwatch_callback_f *callback, int data, unsigned int flags) {
	/* pointer to newly placed watchpoint (stored inside context) */
	sldirwatch_watchpoint_t *watch = &ctx->watchpoints[ctx->num_watchpoints];

	/* check if it's too many watchpoints already */
	if(ctx->num_watchpoints == ctx->max_watchpoints) { return 0; }

#ifdef __linux__
	/* add watch to fd - will recieve only 'closed, while was opened for writing' */
	watch->wd = inotify_add_watch(ctx->fd, path, IN_CLOSE_WRITE);
	if(watch->wd < 0) {
		/* error */
		watch->wd = 0;
		return 1;
	}
#elif defined(_WIN32)
	{
		/* convert path to WCHAR* */
		WCHAR pWidePath[1024];
		MultiByteToWideChar(CP_UTF8, 0, path, -1, pWidePath, sizeof(pWidePath) * sizeof(WCHAR));

		watch->wd = ctx->num_watchpoints;	/* using as watchpoint id */

		watch->dir = CreateFileW(pWidePath, GENERIC_READ | FILE_LIST_DIRECTORY,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
				OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
		if(!watch->dir || watch->dir == (HANDLE)-1) { return 0; }

		/* create OVERLAPPED */
		watch->ovp.hEvent = CreateEvent(0, FALSE, FALSE, 0);
		if(!watch->ovp.hEvent) {
			CloseHandle(watch->dir);
			return 1;
		}

		ReadDirectoryChangesW(watch->dir, watch->lpBuffer, sizeof(watch->lpBuffer), FALSE,
				FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
				&watch->returnedBytes, &watch->ovp, 0);
	}
#endif

	/* copy watchpoint parameters */

	watch->cb = callback;
	watch->data = data;
	watch->flags = flags;

	strncpy(watch->path, path, SLDIRWATCH_PATH_SIZE - 1);
	watch->path[SLDIRWATCH_PATH_SIZE - 1] = '\0';	/* ensure null-termination */

	ctx->num_watchpoints++;

	return 0;
}

int sldirwatch_poll_watchpoints(sldirwatch_t *ctx) {
	int ret = 0;	/* return status */

#ifdef __linux__
	char buf[EVENT_BUF_SIZE];	/* event buffer */

	/* start polling inotify */
	struct pollfd pfd;
	pfd.fd = ctx->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	while(poll(&pfd, 1, 0) == 1) {
		int i = 0;	/* current offset in event buffer */
		int length = read(ctx->fd, buf, sizeof(buf));
		if(length < 0) {
			/* read returned an error
			 * it's very unlikely to fix by itself, so stop reading */
			return -1;
		}

		while(i < length) {
			/* iterate through events */
			struct inotify_event *event = (struct inotify_event*)&buf[i];
			if(event->len) {
				/* get watchpoint index by descriptor - need it for callback */
				int watch_idx = search_wd(ctx, event->wd);
				if(watch_idx != -1) {
					sldirwatch_watchpoint_t *watch = &ctx->watchpoints[watch_idx];

					if(!(watch->flags & SLDIRWATCH_SKIP_HIDDEN_BIT && event->name[0] == '.')) {
						if(watch->cb) {
							char fullpath[SLDIRWATCH_PATH_SIZE];
							const char *path = event->name;

							if(watch->flags & SLDIRWATCH_MERGE_PATHS_BIT) {
								join_paths(fullpath, sizeof(fullpath), watch->path, event->name);
								path = fullpath;
							}

							/* everything ready - fire callback */
							watch->cb(path, watch->data);
						}
						++ret;
					}
				} else {
					/* something gone very bad - we've got event for
					 * nonexistent watchpoint. should never happen
					 */
					return -1;
				}
			}

			i += sizeof(struct inotify_event) + event->len;
		}
	}
#elif defined(_WIN32)
	int i;

	ctx->callno++;	/* double-notify hack */

	for(i = 0; i != ctx->num_watchpoints; ++i) {
		/* ask for changes on every watchpoint */
		DWORD dwWaitStatus = 0;
		sldirwatch_watchpoint_t *watch = &ctx->watchpoints[i];

		dwWaitStatus = WaitForSingleObject(watch->ovp.hEvent, 0);
		if(dwWaitStatus == WAIT_OBJECT_0) {
			DWORD seek = 0;
			while(seek < sizeof(watch->lpBuffer)) {
				WCHAR szwFileName[MAX_PATH];			/* in UTF16 */
				char filepath[SLDIRWATCH_PATH_SIZE];	/* in UTF8 */
				int count = 0;

				PFILE_NOTIFY_INFORMATION pNotify = (PFILE_NOTIFY_INFORMATION)(watch->lpBuffer + seek);

				seek += pNotify->NextEntryOffset;

				/* get filename and convert to UTF8 */
				count = min(pNotify->FileNameLength/2, MAX_PATH-1);
				wcsncpy(szwFileName, pNotify->FileName, count);
				szwFileName[count] = L'\0';
				WideCharToMultiByte(CP_UTF8, 0, szwFileName, -1, filepath, SLDIRWATCH_PATH_SIZE, NULL, NULL);

				/* On windows FILE_NOTIFY_CHANGE_LAST_WRITE can occur
				 * multiple times during writing (each time buffer flushes?).
				 * To avoid triggering callbacks multiple times or triggering
				 * them too early (e.g. data is still being written to file)
				 * we're using heuristic approach:
				 *
				 * if event == last_event && last_event isn't too long ago:
				 *   // (using call counter, not actual time
				 *   if changed file could be opened for read:
				 *     trigger event
				 *     save filename and call number as 'last triggered'
				 */
				if(!(watch->flags & SLDIRWATCH_SKIP_HIDDEN_BIT && szwFileName[0] == L'.') &&
						!(strcmp(filepath, ctx->lastnotify_path) == 0 &&
						(ctx->callno - ctx->lastnotify_callno <= 1 ||
						ctx->lastnotify_callno - ctx->callno == UINT_MAX))) {
					/* attempt to open file (may be still locked) */
					WCHAR p[SLDIRWATCH_PATH_SIZE];
					WCHAR fullp[SLDIRWATCH_PATH_SIZE];
					HANDLE file;
					int l = MultiByteToWideChar(CP_UTF8, 0, watch->path, -1, p, sizeof(p) / sizeof(p[0]));
					p[l] = L'\0';
					join_paths_wchar(fullp, sizeof(fullp) / sizeof(fullp[0]), p, szwFileName);

					file = CreateFileW(fullp, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
					if(file != INVALID_HANDLE_VALUE) {
						CloseHandle(file);

						strcpy(ctx->lastnotify_path, filepath);
						ctx->lastnotify_callno = ctx->callno;

						if(watch->cb) {
							char fullpath[SLDIRWATCH_PATH_SIZE];
							join_paths(fullpath, sizeof(fullpath), watch->path, filepath);

							watch->cb(fullpath, watch->data);
						}

						++ret;
					}
				}

				if(pNotify->NextEntryOffset == 0) { break; }
			}
	
			/* setup reading for next events */
			ReadDirectoryChangesW(watch->dir, watch->lpBuffer, sizeof(watch->lpBuffer), FALSE,
					FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
					&watch->returnedBytes, &watch->ovp, 0);
		}
	}
#endif

	return ret;
}
