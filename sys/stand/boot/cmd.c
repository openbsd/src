/*	$OpenBSD: cmd.c,v 1.1 1997/03/31 03:12:03 weingart Exp $	*/

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
#include "cmd.h"

static struct cmd_table {
	char *cmd_name;
	int cmd_id;
} cmd_table[] = {
	{"addr",   CMD_ADDR},
	{"boot",   CMD_BOOT},
	{"cd",     CMD_CD},
	{"device", CMD_DEVICE},
	{"help",   CMD_HELP},
	{"image",  CMD_IMAGE},
	{"ls",     CMD_LS},
	{"nope",   CMD_NOPE},
	{"reboot", CMD_REBOOT},
	{"set",    CMD_SET},
	{NULL, 0},
};

extern char version[];
void ls __P((char *, register struct stat *));
char skipblnk __P((void));

char cmd_buf[133];

int
getcmd(cmd)
	register struct cmd_state *cmd;
{
	register struct cmd_table *ct = cmd_table;
	register char *p = cmd_buf; /* input */
	register char ch;
	int len;

	cmd->rc = 0;
	cmd->argc = 1;

	for (len = cmd->timeout; len-- && !ischar(); );

	if (len < 0) {
		cmd->cmd = CMD_BOOT;
		cmd->argv[0] = cmd_table[CMD_BOOT].cmd_name;
		cmd->argv[1] = NULL;
		return 0;
	}

	ch = skipblnk();

	for (len = 0; ch != '\n' &&
		     ch != ' ' && ch != '\t'; len++, ch = getchar())
		*p++ = ch;
	*p = '\0';

	if (len == 0 && ch == '\n') {
		cmd->cmd = CMD_NOPE;
		return 0;
	}

	while (ct->cmd_name != NULL &&
	       strncmp(cmd_buf, ct->cmd_name, len))
		ct++;

	if (ct->cmd_name == NULL) {
		cmd->cmd = CMD_ERROR;
		cmd->argv[0] = ct->cmd_name;
		return 0;
	}

	cmd->cmd = ct->cmd_id;
	cmd->argv[0] = ct->cmd_name;
	if (ct->cmd_name != NULL) {
		while (ch != '\n') {

			ch = skipblnk();

			if (ch != '\n') {
				cmd->argv[cmd->argc] = p;
				*p++ = ch;
				for (len = 0; (ch = getchar()) != '\n' &&
					     ch != ' ' && ch != '\t'; len++)
					*p++ = ch;
				*p++ = '\0';
				if (len != 0)
					cmd->argc++;
			}
		}
		cmd->argv[cmd->argc] = NULL;
	}

	return cmd->rc;
}

char
skipblnk()
{
	register char ch;

	/* skip blanks */
	while ((ch = getchar()) != '\n' &&
	       (ch == ' ' || ch == '\t'));

	return ch;
}

int
execmd(cmd)
	register struct cmd_state *cmd;
{
	struct stat sb;
	int fd;
	register char *p, *q;
	register struct cmd_table *ct;

	cmd->rc = 0;

	switch (cmd->cmd) {

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
			q = cmd->argv[1] == NULL? "." : cmd->argv[1];
			sprintf(cmd->path, "%s%s%s",
				cmd->bootdev, cmd->cwd, q);

			if (stat(cmd->path, &sb) < 0) {
				printf("stat(%s): %d\n", cmd->path, errno);
				break;
			}

			if ((sb.st_mode & S_IFMT) != S_IFDIR)
				ls(q, &sb);
			else {
				if ((fd = opendir(cmd->path)) < 0) {
					printf ("opendir(%s): %d\n",
						cmd->path, errno);
					break;
				}

				p = cmd->path + strlen(cmd->path);
				*p++ = '/';
				*p = '\0';

				while(readdir(fd, p) >= 0 && *p != '\0') {

					if (stat(cmd->path, &sb) < 0) {
						printf("stat(%s): %d\n",
						       cmd->path, errno);
						break;
					}
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
		printf("OpenBSD boot version %s\n"
		       "device:\t%s\n"
		       "cwd:\t%s\n"
		       "image:\t%s\n"
		       "load at:\t%p\n"
		       "timeout:\t%d\n",
		       version, cmd->bootdev, cmd->cwd, cmd->image,
		       cmd->addr, cmd->timeout);
		break;

	case CMD_REBOOT:
		exit(1);
		break;

	case CMD_BOOT:
		return 1;
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

	printf (" %s\tuid=%u\tgid=%u\t%lu\n", name, sb->st_uid, sb->st_gid,
		(u_long)sb->st_size);
}

