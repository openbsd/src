/*
 * Copyright (c) 2009 Todd C. Miller <millert@openbsd.org>
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

struct fuser {
	TAILQ_ENTRY(fuser) tq;
	uid_t uid;
	pid_t pid;
	int flags;
#define F_ROOT 0x01	/* is procs root directory */
#define F_CWD  0x02	/* is procs cwd */
#define F_OPEN 0x04	/* just has it open */
#define F_TEXT 0x08	/* is procs executable text */
};

struct filearg {
	SLIST_ENTRY(filearg) next;
	dev_t dev;
	ino_t ino;
	char *name;
	TAILQ_HEAD(fuserhead, fuser) fusers;
};

SLIST_HEAD(fileargs, filearg);

extern int uflg;
extern int cflg;
extern int fsflg;
extern int sflg;
extern int signo;
extern int error;
extern struct fileargs fileargs;

extern char *__progname;

void fuser_check(struct kinfo_file *);
void fuser_run(void);
void usage(void);
