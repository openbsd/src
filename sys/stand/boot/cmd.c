/*	$OpenBSD: cmd.c,v 1.48 2002/07/14 09:19:17 mdw Exp $	*/

/*
 * Copyright (c) 1997-1999 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <libsa.h>
#include <sys/reboot.h>
#include "cmd.h"

#define CTRL(c)	((c)&0x1f)

static int Xboot(void);
static int Xecho(void);
static int Xhelp(void);
static int Xls(void);
static int Xnop(void);
static int Xreboot(void);
static int Xstty(void);
static int Xtime(void);
#ifdef MACHINE_CMD
static int Xmachine(void);
extern const struct cmd_table MACHINE_CMD[];
#endif
extern int Xset(void);
extern int Xenv(void);

extern const struct cmd_table cmd_set[];
const struct cmd_table cmd_table[] = {
	{"#",      CMDT_CMD, Xnop},  /* XXX must be first */
	{"boot",   CMDT_CMD, Xboot},
	{"echo",   CMDT_CMD, Xecho},
	{"env",    CMDT_CMD, Xenv},
	{"help",   CMDT_CMD, Xhelp},
	{"ls",     CMDT_CMD, Xls},
#ifdef MACHINE_CMD
	{"machine",CMDT_MDC, Xmachine},
#endif
	{"reboot", CMDT_CMD, Xreboot},
	{"set",    CMDT_SET, Xset},
	{"stty",   CMDT_CMD, Xstty},
	{"time",   CMDT_CMD, Xtime},
	{NULL, 0},
};

static void ls(char *, register struct stat *);
static int readline(register char *, size_t, int);
char *nextword(register char *);
static char *whatcmd(register const struct cmd_table **ct, register char *);
static int docmd(void);
static char *qualify(char *);

char cmd_buf[133];

int
getcmd()
{
	cmd.cmd = NULL;

	if (!readline(cmd_buf, sizeof(cmd_buf), cmd.timeout))
		cmd.cmd = cmd_table;

	return docmd();
}

int
read_conf()
{
#ifndef INSECURE
	struct stat sb;
#endif
	int fd, eof = 0;

	if ((fd = open(qualify(cmd.conf), 0)) < 0) {
		if (errno != ENOENT && errno != ENXIO) {
			printf("open(%s): %s\n", cmd.path, strerror(errno));
			return 0;
		}
		return -1;
	}

#ifndef INSECURE
	(void) fstat(fd, &sb);
	if (sb.st_uid || (sb.st_mode & 2)) {
		printf("non-secure %s, will not proceed\n", cmd.path);
		close(fd);
		return -1;
	}
#endif

	do {
		register char *p = cmd_buf;

		cmd.cmd = NULL;

		do
			eof = read(fd, p, 1);
		while (eof > 0 && *p++ != '\n');

		if (eof < 0)
			printf("%s: %s\n", cmd.path, strerror(errno));
		else
			*--p = '\0';

	} while (eof > 0 && !(eof = docmd()));

	close(fd);
	return eof;
}

static int
docmd()
{
	register char *p = NULL;
	const struct cmd_table *ct = cmd_table, *cs;

	cmd.argc = 1;
	if (cmd.cmd == NULL) {

		/* command */
		for (p = cmd_buf; *p && (*p == ' ' || *p == '\t'); p++)
			;
		if (*p == '#' || *p == '\0') { /* comment or empty string */
#ifdef DEBUG
			printf("rem\n");
#endif
			return 0;
		}
		ct = cmd_table;
		cs = NULL;
		cmd.argv[cmd.argc] = p; /* in case it's shortcut boot */
		p = whatcmd(&ct, p);
		if (ct == NULL) {
			cmd.argc++;
			ct = cmd_table;
		} else if (ct->cmd_type == CMDT_SET && p != NULL) {
			cs = cmd_set;
#ifdef MACHINE_CMD
		} else if (ct->cmd_type == CMDT_MDC && p != NULL) {
			cs = MACHINE_CMD;
#endif
		}

		if (cs != NULL) {
			p = whatcmd(&cs, p);
			if (cs == NULL) {
				printf("%s: syntax error\n", ct->cmd_name);
				return 0;
			}
			ct = cs;
		}
		cmd.cmd = ct;
	}

	cmd.argv[0] = ct->cmd_name;
	while (p && cmd.argc+1 < sizeof(cmd.argv) / sizeof(cmd.argv[0])) {
		cmd.argv[cmd.argc++] = p;
		p = nextword(p);
	}
	cmd.argv[cmd.argc] = NULL;

	return (*cmd.cmd->cmd_exec)();
}

static char *
whatcmd(ct, p)
	register const struct cmd_table **ct;
	register char *p;
{
	register char *q;
	register int l;

	q = nextword(p);

	for (l = 0; p[l]; l++)
		;

	while ((*ct)->cmd_name != NULL && strncmp(p, (*ct)->cmd_name, l))
		(*ct)++;

	if ((*ct)->cmd_name == NULL)
		*ct = NULL;

	return q;
}

static int
readline(buf, n, to)
	register char *buf;
	size_t n;
	int	to;
{
#ifdef DEBUG
	extern int debug;
#endif
	register char *p = buf, ch;

	/* Only do timeout if greater than 0 */
	if (to > 0) {
		u_long i = 0;
		time_t tt = getsecs() + to;
#ifdef DEBUG
		if (debug > 2)
			printf ("readline: timeout(%d) at %u\n", to, tt);
#endif
		/* check for timeout expiration less often
		   (for some very constrained archs) */
		while (!cnischar())
			if (!(i++ % 1000) && (getsecs() >= tt))
				break;

		if (!cnischar()) {
			strncpy(buf, "boot", 5);
			putchar('\n');
			return strlen(buf);
		}
	} else
		while (!cnischar()) ;

	while (1) {
		switch ((ch = getchar())) {
		case CTRL('u'):
			while (p > buf) {
				putchar('\177');
				p--;
			}
			continue;
		case '\n':
		case '\r':
			p[1] = *p = '\0';
			break;
		case '\b':
		case '\177':
			if (p > buf) {
				putchar('\177');
				p--;
			}
			continue;
		default:
			if (p - buf < n-1)
				*p++ = ch;
			else {
				putchar('\007');
				putchar('\177');
			}
			continue;
		}
		break;
	}

	return p - buf;
}

/*
 * Search for spaces/tabs after the current word. If found, \0 the
 * first one.  Then pass a pointer to the first character of the
 * next word, or NULL if there is no next word. 
 */
char *
nextword(p)
	register char *p;
{
	/* skip blanks */
	while (*p && *p != '\t' && *p != ' ')
		p++;
	if (*p) {
		*p++ = '\0';
		while (*p == '\t' || *p == ' ')
			p++;
	}
	if (*p == '\0')
		p = NULL;
	return p;
}

static void
print_help(ct)
	register const struct cmd_table *ct;
{
	for (; ct->cmd_name != NULL; ct++)
		printf(" %s", ct->cmd_name);
	putchar('\n');
}

static int
Xhelp()
{
	printf("commands:");
	print_help(cmd_table);
#ifdef MACHINE_CMD
	return Xmachine();
#else
	return 0;
#endif
}

#ifdef MACHINE_CMD
static int
Xmachine()
{
	printf("machine:");
	print_help(MACHINE_CMD);
	return 0;
}
#endif

static int
Xecho()
{
	register int i;
	for (i = 1; i < cmd.argc; i++)
		printf("%s ", cmd.argv[i]);
	putchar('\n');
	return 0;
}

static int
Xstty()
{
	register int sp;
	register char *cp;
	dev_t dev;

	if (cmd.argc == 1)
		printf("%s speed is %d\n", ttyname(0), cnspeed(0, -1));
	else {
		dev = ttydev(cmd.argv[1]);
		if (dev == NODEV)
			printf("%s not a console device\n", cmd.argv[1]);
		else {
			if (cmd.argc == 2)
				printf("%s speed is %d\n", cmd.argv[1],
				       cnspeed(dev, -1));
			else {
				sp = 0;
				for (cp = cmd.argv[2]; *cp && isdigit(*cp); cp++)
					sp = sp * 10 + (*cp - '0');
				cnspeed(dev, sp);
			}
		}
	}

	return 0;
}

static int
Xtime()
{
	time_t tt = getsecs();

	if (cmd.argc == 1)
		printf(ctime(&tt));
	else {
	}

	return 0;
}

static int
Xls()
{
	struct stat sb;
	register char *p;
	int fd;

	if (stat(qualify((cmd.argv[1]? cmd.argv[1]: "/.")), &sb) < 0) {
		printf("stat(%s): %s\n", cmd.path, strerror(errno));
		return 0;
	}

	if ((sb.st_mode & S_IFMT) != S_IFDIR)
		ls(cmd.path, &sb);
	else {
		if ((fd = opendir(cmd.path)) < 0) {
			printf ("opendir(%s): %s\n", cmd.path,
				strerror(errno));
			return 0;
		}

		/* no strlen in lib !!! */
		for (p = cmd.path; *p; p++);
		*p++ = '/';
		*p = '\0';

		while(readdir(fd, p) >= 0) {
			if (stat(cmd.path, &sb) < 0)
				printf("stat(%s): %s\n", cmd.path,
				       strerror(errno));
			else
				ls(p, &sb);
		}
		closedir (fd);
	}
	return 0;
}

#define lsrwx(mode,s) \
	putchar ((mode) & S_IROTH? 'r' : '-'); \
	putchar ((mode) & S_IWOTH? 'w' : '-'); \
	putchar ((mode) & S_IXOTH? *(s): (s)[1]);

static void
ls(name, sb)
	register char *name;
	register struct stat *sb;
{
	putchar("-fc-d-b---l-s-w-"[(sb->st_mode & S_IFMT) >> 12]);
	lsrwx(sb->st_mode >> 6, (sb->st_mode & S_ISUID? "sS" : "x-"));
	lsrwx(sb->st_mode >> 3, (sb->st_mode & S_ISGID? "sS" : "x-"));
	lsrwx(sb->st_mode     , (sb->st_mode & S_ISTXT? "tT" : "x-"));

	printf (" %u,%u\t%lu\t%s\n", sb->st_uid, sb->st_gid,
		(u_long)sb->st_size, name);
}
#undef lsrwx

int doboot = 1;

static int
Xnop()
{
	if (doboot) {
		doboot = 0;
		return (Xboot());
	}

	return 0;
}

static int
Xboot()
{
	if (cmd.argc > 1 && cmd.argv[1][0] != '-') {
		qualify((cmd.argv[1]? cmd.argv[1]: cmd.image));
		if (bootparse(2))
			return 0;
	} else {
		if (bootparse(1))
			return 0;
		sprintf(cmd.path, "%s:%s", cmd.bootdev, cmd.image);
	}

	return 1;
}

/*
 * Qualifies the path adding neccessary dev
 */

static char *
qualify(name)
	char *name;
{
	register char *p;

	for (p = name; *p; p++)
		if (*p == ':')
			break;
	if (*p == ':')
		strncpy(cmd.path, name, sizeof(cmd.path));
	else
		sprintf(cmd.path, "%s:%s", cmd.bootdev, name);
	return cmd.path;
}

static int
Xreboot()
{
	printf("Rebooting...\n");
	exit();
	return 0; /* just in case */
}

