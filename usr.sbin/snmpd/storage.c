/*	$OpenBSD: storage.c,v 1.1 2026/10/30 16:40:40 martijn Exp $	*/

/*
 * Copyright (c) 2026 Martijn van Duren <martijn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "snmpd.h"

void	 storage_init(struct privsep *, struct privsep_proc *, void *);
int	 storage_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 storage_dispatch_snmpe(int, struct privsep_proc *, struct imsg *);
int	 storage_mkdir(const char *);
FILE	*storage_open(const char *, const char *);
void	 storage_store_engine(struct engine *);
void	 storage_load_engines(void);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT, storage_dispatch_parent },
	{ "snmpe",	PROC_SNMPE, storage_dispatch_snmpe }
};

void
storage(struct privsep *ps, struct privsep_proc *p)
{
	proc_run(ps, p, procs, nitems(procs), storage_init, NULL);
}

void
storage_init(struct privsep *ps, struct privsep_proc *s, void *arg)
{
	if (unveil("/", "rwc") == -1)
		fatal("unveil %s", snmpd_env->sc_storagepath);
	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		fatal("pledge");

	storage_load_engines();
	if (proc_compose(&snmpd_env->sc_ps, PROC_SNMPE,
	    IMSG_STORAGE_DONE, NULL, 0) == -1)
		fatal("proc_compose");
}

void
storage_shutdown(void)
{
}

int
storage_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	return -1;
}

int
storage_dispatch_snmpe(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct engine engine;

	switch (imsg_get_type(imsg)) {
	case IMSG_STORAGE_ENGINE:
		if (imsg_get_data(imsg, &engine, sizeof(engine)) == -1)
			fatal("imsg_get_data");
		storage_store_engine(&engine);
		return 0;
	default:
		return -1;
	}
}

int
storage_mkdir(const char *path)
{
	char copy[PATH_MAX], *parent;

	if (mkdir(path, 0755) == -1) {
		if (errno == ENOENT) {
			strlcpy(copy, path, sizeof(copy));
			parent = dirname(copy);
			strlcpy(copy, parent, sizeof(copy));
			if (storage_mkdir(copy) == -1)
				return -1;
			return mkdir(path, 0755);
		}
		return -1;
	}
	return 0;
}

FILE *
storage_open(const char *path, const char *mode)
{
	FILE *f;
	char copy[PATH_MAX], *parent;

	if ((f = fopen(path, mode)) == NULL) {
		if (errno == ENOENT) {
			strlcpy(copy, path, sizeof(copy));
			parent = dirname(copy);
			strlcpy(copy, parent, sizeof(copy));
			if (storage_mkdir(copy) == -1)
				return NULL;
			return fopen(path, mode);
		}
		return NULL;
	}
	return f;
}

void
storage_store_engine(struct engine *engine)
{
	FILE *info;
	char final[PATH_MAX], tmp[PATH_MAX];
	struct timespec mnow, rnow, rmeasure, diff;

	snprintf(tmp, sizeof(tmp), "/engines/%s/.info",
	    tohexstr(engine->id.value, engine->id.length));
	snprintf(final, sizeof(final), "/engines/%s/info",
	    tohexstr(engine->id.value, engine->id.length));
	if ((info = storage_open(tmp, "w")) == NULL)
		fatal("open %s%s", snmpd_env->sc_storagepath, tmp);

	/*
	 * Store the REALTIME.
	 * It can drift harder than MONOTONIC, but it survives reboots.
	 */
	clock_gettime(CLOCK_MONOTONIC, &mnow);
	clock_gettime(CLOCK_REALTIME, &rnow);
	timespecsub(&mnow, &engine->measure, &diff);
	timespecsub(&rnow, &diff, &rmeasure);

	fprintf(info, "%lld.%09ld\n%d\n%d\n%d\n",
	    rmeasure.tv_sec, rmeasure.tv_nsec,
	    engine->boots, engine->time, engine->msg_max_size);
	if (fflush(info) == EOF)
		fatal("write %s%s", snmpd_env->sc_storagepath, tmp);
	if (rename(tmp, final) == -1)
		fatal("rename %s%s -> %s%s", snmpd_env->sc_storagepath, tmp,
		    snmpd_env->sc_storagepath, final);
}

void
storage_load_engines(void)
{
	DIR *engines;
	struct dirent *engineid;
	struct engine engine;
	struct timespec mnow, rnow, rmeasure, diff;
	char path[PATH_MAX];
	FILE *info;

	if ((engines = opendir("/engines")) == NULL) {
		if (errno == ENOENT)
			return;
		fatal("failed to open %s", snmpd_env->sc_storagepath);
	}

	errno = 0;
	clock_gettime(CLOCK_MONOTONIC, &mnow);
	clock_gettime(CLOCK_REALTIME, &rnow);
	while ((engineid = readdir(engines)) != NULL) {
		engine = (struct engine){};
		engine.id.length = strlen(engineid->d_name) / 2;
		if (engine.id.length > sizeof(engine.id.value))
			continue;
		if (fromhexstr(engine.id.value, engineid->d_name,
		    strlen(engineid->d_name)) == NULL)
			continue;
		snprintf(path, sizeof(path), "/engines/%s/info",
		    engineid->d_name);

		if ((info = fopen(path, "r")) == NULL) {
			log_warn("open %s%s", snmpd_env->sc_storagepath, path);
			continue;
		}
		if (fscanf(info, "%lld.%09ld\n%d\n%d\n%d\n",
		    &rmeasure.tv_sec, &rmeasure.tv_nsec, &engine.boots,
		    &engine.time, &engine.msg_max_size) != 5 ||
		    fgetc(info) != EOF || !feof(info)) {
			log_warnx("invalid content %s%s",
			    snmpd_env->sc_storagepath, path);
			fclose(info);
			continue;
		}
		fclose(info);

		timespecsub(&rnow, &rmeasure, &diff);
		timespecsub(&mnow, &diff, &engine.measure);

		if (proc_compose(&snmpd_env->sc_ps, PROC_SNMPE,
		    IMSG_STORAGE_ENGINE, &engine, sizeof(engine)) == -1)
			fatal("proc_compose");
	}
	if (errno != 0)
		fatal("failed to read %s", snmpd_env->sc_storagepath);
}
