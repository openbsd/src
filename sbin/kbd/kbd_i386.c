/*	$OpenBSD: kbd_i386.c,v 1.7 1998/05/29 00:42:53 mickey Exp $	*/

/*
 * Copyright (c) 1996 Juergen Hannken-Illjes
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
 *	This product includes software developed for the NetBSD Project
 *	by Juergen Hannken-Illjes.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <machine/pccons.h>
#include <machine/pcvt_ioctl.h>
#include <paths.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <stdio.h>

#define NUM_NAMES	10

struct {
	char *name[NUM_NAMES];
	pccons_keymap_t map[KB_NUM_KEYS];
} keymaps[] = {

  {
#include "tables/us.english"
  },
  {
#include "tables/german"
  },
  {
#include "tables/koi8"
  },
  {
#include "tables/french"
  },
  {
#include "tables/swedish"
  },
  {
#include "tables/swedish7"
  },
  {
  { NULL }
  }
};

extern char *__progname;

int
ispcvt()
{
	struct pcvtid pcvtid;
		
	return ioctl(0, VGAPCVTID, &pcvtid);
}

void
kbd_list()
{
	int i, j;

	if (ispcvt() < 0) {
		printf("tables available:\n%-16s %s\n\n",
		       "encoding", "nick names");
		for (i = 0; keymaps[i].name[0]; i++) {
			printf("%-16s",keymaps[i].name[0]);
			for (j = 1; j < NUM_NAMES && keymaps[i].name[j]; j++)
				printf(" %s", keymaps[i].name[j]);
			printf("\n");
		}
	} else
		printf("consult 'keycap' database for kbd mapping tables\n");
}

void
kbd_set(name, verbose)
	char *name;
	int verbose;
{

	if (ispcvt() < 0) {
		int i, j, fd;
		pccons_keymap_t *map = NULL;

		for (i = 0; keymaps[i].name[0]; i++)
			for (j = 0; j < NUM_NAMES && keymaps[i].name[j]; j++)
				if (strcmp(keymaps[i].name[j], name) == 0) {
					name = keymaps[i].name[0];
					map = keymaps[i].map;
					break;
				}

		if (map == NULL) {
			fprintf(stderr, "%s: no such keymap: %s\n",
				__progname, name);
			exit(1);
		}

		if ((fd = open(_PATH_CONSOLE, O_RDONLY)) < 0)
			err(1, "%s", _PATH_CONSOLE);

		if (ioctl(fd, CONSOLE_SET_KEYMAP, map) < 0)
			err(1, "CONSOLE_SET_KEYMAP");

		close(fd);

		if (verbose)
			fprintf(stderr, "keyboard mapping set to %s\n", name);
	} else {
		char buf[32];

		snprintf(buf, sizeof(buf), "kcon -m %s", name);

		if (system(buf))
			err(1, name);
	}
}
