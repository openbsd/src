/*	$OpenBSD: vars.c,v 1.2 1998/05/30 01:51:39 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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

extern const char version[];
extern int debug;

static int Xaddr __P((void));
static int Xdevice __P((void));
#ifdef DEBUG
static int Xdebug __P((void));
#endif
static int Ximage __P((void));
static int Xhowto __P((void));
static int Xtty __P((void));
int Xset __P((void));
int Xenv __P((void));

const struct cmd_table cmd_set[] = {
	{"addr",   CMDT_VAR, Xaddr},
	{"howto",  CMDT_VAR, Xhowto},
#ifdef DEBUG	
	{"debug",  CMDT_VAR, Xdebug},
#endif
	{"device", CMDT_VAR, Xdevice},
	{"tty",    CMDT_VAR, Xtty},
	{"image",  CMDT_VAR, Ximage},
	{NULL,0}
};

#ifdef DEBUG
static int
Xdebug()
{
	if (cmd.argc !=2)
		printf(debug? "on": "off");
	else
		debug = (cmd.argv[1][0] == '0' ||
			 (cmd.argv[1][0] == 'o' && cmd.argv[1][1] == 'f'))?
			 0: 1;
	return 0;
}
#endif

/* called only w/ no arguments */
int
Xset()
{
	register const struct cmd_table *ct;

	printf(">> OpenBSD/" MACHINE_ARCH " BOOT %s\n", version);
	for (ct = cmd_set; ct->cmd_name != NULL; ct++) {
		printf("%s\t ", ct->cmd_name);
		(*ct->cmd_exec)();
		putchar('\n');
	}
	return 0;
}

static int
Xdevice()
{
	if (cmd.argc != 2)
		printf(cmd.bootdev);
	else
		strncpy(cmd.bootdev, cmd.argv[1], sizeof(cmd.bootdev));
	return 0;
}

static int
Ximage()
{
	if (cmd.argc != 2)
		printf(cmd.image);
	else
		strncpy(cmd.image, cmd.argv[1], sizeof(cmd.image));
	return 0;
}

static int
Xaddr()
{
	if (cmd.argc != 2)
		printf("%p", cmd.addr);
	else
		cmd.addr = (void *)strtol(cmd.argv[1], NULL, 0);
	return 0;
}

static int
Xtty()
{
	dev_t dev;

	if (cmd.argc != 2)
		printf(ttyname(0));
	else {
		dev = ttydev(cmd.argv[1]);
		if (dev == NODEV)
			printf("%s not a console device\n", cmd.argv[1]);
		else {
			printf("switching console to %s\n", cmd.argv[1]);
			if (cnset(dev))
				printf("%s console not present\n",
				       cmd.argv[1]);
		}
	}
	return 0;
}

static int
Xhowto()
{
	if (cmd.argc == 1) {
		if (cmd.boothowto) {
			putchar('-');
			if (cmd.boothowto & RB_ASKNAME)
				putchar('a');
			if (cmd.boothowto & RB_HALT)
				putchar('b');
			if (cmd.boothowto & RB_CONFIG)
				putchar('c');
			if (cmd.boothowto & RB_SINGLE)
				putchar('s');
			if (cmd.boothowto & RB_KDB)
				putchar('d');
		}
	} else
		bootparse(1);
	return 0;
}

int
bootparse(i)
	int i;
{
	register char *cp;
	int howto = cmd.boothowto;

	for (; i < cmd.argc; i++) {
		cp = cmd.argv[i];
		if (*cp == '-') {
			while (*++cp) {
				switch (*cp) {
				case 'a':
					howto |= RB_ASKNAME;
					break;
				case 'b':
					howto |= RB_HALT;
					break;
				case 'c':
					howto |= RB_CONFIG;
					break;
				case 's':
					howto |= RB_SINGLE;
					break;
				case 'd':
					howto |= RB_KDB;
					break;
				default:
					printf("howto: bad option: %c\n", *cp);
					return 1;
				}
			}
		} else {
			printf("boot: illegal argument %s\n", cmd.argv[i]);
			return 1;
		}
	}
	cmd.boothowto = howto;
	return 0;
}

/*
 * maintain environmanet as a sequence of '\n' separated
 * variable definitions in the form <name>=[<value>]
 * terminated by the usual '\0'
 */
char *environ;
int
Xenv()
{
	if (cmd.argc == 1) {
		if (environ)
			printf(environ);
		else
			printf("empty\n");
	} else {
		register char *p, *q;
		int l;
		for (p = environ; p && *p; p = q) {
			l = strlen(cmd.argv[1]);
			for (q = p; *q != '='; q++);
			l = max(l, q - p) + 1;
			for (q = p; *q != '\n'; q++);
			if (*q)
				q++;
			if (!strncmp(p, cmd.argv[1], l)) {
				while((*p++ = *q++));
				p--;
			}
		}
		if (!p)
			p = environ = alloc(NBPG);
		sprintf(p, "%s=%s\n",
			cmd.argv[1], (cmd.argc==3?cmd.argv[2]:""));
	}

	return 0;
}
