/*	$OpenBSD: cmd.c,v 1.3 1997/04/01 04:50:33 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
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
#include <string.h>
#include <libsa.h>
#include <debug.h>
#include "cmd.h"
#ifndef _TEST
#include <biosdev.h>
#endif

extern int debug;

#define CTRL(c)	((c)&0x1f)

const struct cmd_table {
	char *cmd_name;
	int cmd_id;
} cmd_table[] = {
	{"addr",   CMD_ADDR},
	{"boot",   CMD_BOOT},
	{"cd",     CMD_CD},
	{"device", CMD_DEVICE},
#ifdef DEBUG	
	{"debug",  CMD_DEBUG},
#endif
	{"help",   CMD_HELP},
	{"image",  CMD_IMAGE},
	{"ls",     CMD_LS},
	{"nope",   CMD_NOPE},
	{"reboot", CMD_REBOOT},
	{"regs",   CMD_REGS},
	{"set",    CMD_SET},
	{NULL, 0},
};

extern const char version[];
static void ls __P((char *, register struct stat *));
static char *skipblnk __P((register char *));
static int readline __P((register char *, int));

char *cmd_buf = NULL;

int
getcmd(cmd)
	struct cmd_state *cmd;
{
	register const struct cmd_table *ct = cmd_table;
	register char *p = cmd_buf, *q; /* input */

	if (cmd_buf == NULL)
		p = cmd_buf = alloc(133);

	cmd->rc = 0;
	cmd->argc = 1;

	if (!readline(cmd_buf, cmd->timeout)) {
		cmd->cmd = CMD_BOOT;
		cmd->argv[0] = cmd_table[CMD_BOOT].cmd_name;
		cmd->argv[1] = NULL;
		return 0;
	}

	p = skipblnk(cmd_buf);

	/* command */
	for ( q = p; *p != '\0' && *p != ' ' && *p != '\t'; p++);
	*p = '\0';

	while (ct->cmd_name != NULL && strncmp(q, ct->cmd_name, (p - q)))
		ct++;

	if (ct->cmd_name == NULL) {
		cmd->cmd = CMD_BOOT;
		cmd->argv[0] = cmd_table[CMD_BOOT].cmd_name;
		cmd->argv[1] = skipblnk(cmd_buf);
		cmd->argv[2] = NULL;
		cmd->argc++;
		return 0;
	}

	cmd->cmd = ct->cmd_id;
	cmd->argv[0] = ct->cmd_name;
	for (p++; *(p = skipblnk(p)) != '\0'; *p++ = '\0') {
		cmd->argv[cmd->argc++] = q = p;
		for (; *p && *p != '\t' && *p != ' '; p++);
	}
	cmd->argv[cmd->argc] = NULL;

	return cmd->rc;
}

static int
readline(buf, to)
	register char *buf;
	int	to;
{
	char *p = buf, *pe = buf, ch;
	int i;

	for (i = to; i-- && !ischar(); )
#ifndef _TEST
		usleep(100000);
#else
		;
#endif
	if (i < 0)
		return 0;

	while (1) {
		switch (ch = getchar()) {
		case CTRL('u'):
			while (pe > buf) {
				pe--;
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			p = pe = buf;
			continue;
		case '\n':
			pe[1] = *pe = '\0';
			break;
		case '\b':
			if (p > buf) {
				putchar('\b');
				putchar(' ');
				putchar('\b');
				p--;
				pe--;
			}
			continue;
		default:
			pe++;
			*p++ = ch;
			continue;
		}
		break;
	}

	return pe - buf;
}

char *
skipblnk(p)
	register char *p;
{
	/* skip blanks */
	while (*p == '\t' || *p == ' ')
		p++;

	return p;
}

int
execmd(cmd)
	struct cmd_state *cmd;
{
	struct stat sb;
	int fd;
	register char *p, *q;
	register const struct cmd_table *ct;

	cmd->rc = 0;
	switch (cmd->cmd) {

#ifdef DEBUG
	case CMD_DEBUG:
		debug = !debug;
		printf("debug is %s\n", debug? "on": "off");
		break;
#endif

	case CMD_HELP:
		printf("commands: ");
		for (ct = cmd_table; ct->cmd_name != NULL; ct++)
			printf(" %s", ct->cmd_name);
		putchar('\n');
		break;

	case CMD_DEVICE:
		if (cmd->argc != 2)
			printf("device: device name required\n");
		else {
			strncpy(cmd->bootdev, cmd->argv[1],
				sizeof(cmd->bootdev));
		}
		break;

	case CMD_IMAGE:
		if (cmd->argc != 2)
			printf("image: pathname required\n");
		else {
			strncpy(cmd->image, cmd->argv[1],
				sizeof(cmd->image));
		}
		break;

	case CMD_ADDR:
		if (cmd->argc != 2)
			printf("addr: address required\n");
		else {
			register u_long a;

			p = cmd->argv[1];
			if (p[0] == '0' && p[1] == 'x')
				p += 2;
			for (a = 0; *p != '\0'; p++) {
				a <<= 4;
				a |= (isdigit(*p)? *p - '0':
				      10 + tolower(*p) - 'a') & 0xf;
			}

			cmd->addr = (void *)a;
		}
		break;

	case CMD_LS:
		{
			if (cmd->argv[1] != NULL)
				strncpy (cmd->path, cmd->argv[1],
					 sizeof(cmd->path));
			else
				sprintf(cmd->path, "%s%s/.",
					cmd->bootdev, cmd->cwd);

			if (stat(cmd->path, &sb) < 0) {
				printf("stat(%s): %d\n", cmd->path, errno);
				break;
			}

			if ((sb.st_mode & S_IFMT) != S_IFDIR)
				ls(cmd->path, &sb);
			else {
				if ((fd = opendir(cmd->path)) < 0) {
					printf ("opendir(%s): %d\n",
						cmd->path, errno);
					break;
				}

				/* no strlen in lib !!! */
				for (p = cmd->path; *p; p++);
				*p++ = '/';
				*p = '\0';

				while(readdir(fd, p) >= 0) {

					if (stat(cmd->path, &sb) < 0)
						printf("stat(%s): %d\n",
						       cmd->path, errno);
					else
						ls(p, &sb);
				}

				closedir (fd);
			}
		}
		break;

	case CMD_CD:
		if (cmd->argc == 1) {
			cmd->cwd[0] = '/';
			cmd->cwd[1] = '\0';
			break;
		}

		if (cmd->argv[1][0] == '.' && cmd->argv[1][1] == '\0')
			break;

		if (cmd->argv[1][0] == '.' && cmd->argv[1][1] == '.'
		    && cmd->argv[1][2] == '\0') {
			/* strrchr(cmd->cwd, '/'); */
			for (p = cmd->cwd; *++p;);
			for (p--; *--p != '/';);
			p[1] = '\0';
			break;
		}

		sprintf(cmd->path, "%s%s%s",
			cmd->bootdev, cmd->cwd, cmd->argv[1]);
		if (stat(cmd->path, &sb) < 0) {
			printf("stat(%s): %d\n", cmd->argv[1], errno);
			break;
		}

		if (!S_ISDIR(sb.st_mode)) {
			printf("boot: %s: not a dir\n", cmd->argv[1]);
			break;
		}

		/* change dir */
		for (p = cmd->cwd; *p; p++);
		for (q = cmd->argv[1]; (*p++ = *q++) != '\0';);
		if (p[-2] != '/') {
			p[-1] = '/';
			p[0] = '\0';
		}
		break;

	case CMD_SET:
		printf("OpenBSD/i386 boot version %s(debug is %s)\n"
		       "device:\t%s\n"
		       "cwd:\t%s\n"
		       "image:\t%s\n"
		       "load at:\t%p\n"
		       "timeout:\t%d\n",
		       version,
#ifdef DEBUG
		       (debug? "on": "off"),
#endif
		       cmd->bootdev, cmd->cwd, cmd->image,
		       cmd->addr, cmd->timeout);
		break;

	case CMD_REBOOT:
		cmd->rc = -1;
		break;

	case CMD_REGS:
		DUMP_REGS;
		break;

	case CMD_BOOT:
		if (cmd->argc > 1)
			strncpy(cmd->path, cmd->argv[1], sizeof(cmd->path));
		else
			sprintf(cmd->path, "%s%s%s", cmd->bootdev,
				cmd->cwd, cmd->image);
		cmd->rc = 1;
		break;

	case CMD_ERROR:
	default:
		printf ("%s: invalid command\n", cmd->argv[0]);
	case CMD_NOPE:
		break;
	}

	return cmd->rc;
}

#define lsrwx(mode,s) \
	putchar ((mode) & S_IROTH? 'r' : '-'); \
	putchar ((mode) & S_IWOTH? 'w' : '-'); \
	putchar ((mode) & S_IXOTH? *(s): (s)[1]);

void
ls(name, sb)
	char *name;
	register struct stat *sb;
{
	putchar("-fc-d-b---l-s-w-"[(sb->st_mode & S_IFMT) >> 12]);
	lsrwx(sb->st_mode >> 6, (sb->st_mode & S_ISUID? "sS" : "x-"));
	lsrwx(sb->st_mode >> 3, (sb->st_mode & S_ISUID? "sS" : "x-"));
	lsrwx(sb->st_mode     , (sb->st_mode & S_ISTXT? "tT" : "x-"));

	printf (" %u,%u\t%lu\t%s\n", sb->st_uid, sb->st_gid,
		(u_long)sb->st_size, name);
}

