/*	$OpenBSD: kbd_sparc.c,v 1.3 1999/07/20 21:02:25 maja Exp $ */

/*
 * Copyright (c) 1999 Mats O Jansson.  All rights reserved.
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
 *	This product includes software developed by Mats O Jansson.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sparc/kbio.h>
#include <sparc/kbd.h>
#include <sys/ioctl.h>

#define	NUM_KEYS	128	/* Number of scan codes */
#define	NUM_NAMES	10	/* Number of names for a map */

#define ALL(s)	(s), (s), (s), (s),
#define BB(s)	(FUNNY+(s))
#define LF(s)	(0x600+(s)-1)
#define RF(s)	(0x610+(s)-1)
#define TF(s)	(0x620+(s)-1)
#define BF(s)	(0x630+(s)-1)
#define SK(s)	(SHIFTKEYS+(s))
#define ST(s)	(0x500+(s))
#define GR(s)	(0x400+(s))
#define	C8(s)	((u_char)(s))

typedef struct {
        u_short unshift;
        u_short shift;
	u_short altgr;
	u_short control;
} keymap_t;

struct {
	char *name[NUM_NAMES];
	keymap_t map[NUM_KEYS];
} keymaps[] = {

  {
#include "tables/sparc/us"
  },
  {
#include "tables/sparc/be_fr"
  },
  {
#include "tables/sparc/ca"
  },
  {
#include "tables/sparc/dk"
  },
  {
#include "tables/sparc/de"
  },
  {
#include "tables/sparc/it"
  },
  {
#include "tables/sparc/nl"
  },
  {
#include "tables/sparc/no"
  },
  {
#include "tables/sparc/pt"
  },
  {
#include "tables/sparc/es"
  },
  {
#include "tables/sparc/se_fi"
  },
  {
#include "tables/sparc/ch_fr"
  },
  {
#include "tables/sparc/ch_de"
  },
  {
#include "tables/sparc/uk"
  },
  {
#include "tables/sparc/us_5"
  },
  {
#include "tables/sparc/fr_5"
  },
  {
#include "tables/sparc/dk_5"
  },
  {
#include "tables/sparc/de_5"
  },
  {
#include "tables/sparc/it_5"
  },
  {
#include "tables/sparc/nl_5"
  },
  {
#include "tables/sparc/no_5"
  },
  {
#include "tables/sparc/pt_5"
  },
  {
#include "tables/sparc/es_5"
  },
  {
#include "tables/sparc/se_5"
  },
  {
#include "tables/sparc/ch_fr_5"
  },
  {
#include "tables/sparc/ch_de_5"
  },
  {
#include "tables/sparc/uk_5"
  },
  {
    { NULL }
  }
};

extern char *__progname;

void
kbd_list()
{
	int i, j;

	printf("tables available:\n%-16s %s\n\n",
		"encoding", "nick names");
	for (i = 0; keymaps[i].name[0]; i++) {
		printf("%-16s",keymaps[i].name[0]);
		for (j = 1; j < NUM_NAMES && keymaps[i].name[j]; j++)
			printf(" %s", keymaps[i].name[j]);
		printf("\n");
	}	  
}

void
kbd_set(name, verbose)
	char *name;
	int verbose;
{
	int i, j, fd, t, l, r;
	keymap_t *map = NULL;
	int x[] = { KIOC_NOMASK, KIOC_SHIFTMASK,
		    KIOC_ALTGMASK, KIOC_CTRLMASK };
	struct kiockey k;
	
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

	fd = open("/dev/kbd", O_RDWR);
	if (fd == -1) {
		perror("/dev/kbd");
		exit(1);
	}		

	r = ioctl(fd, KIOCTYPE, &t);
	r = ioctl(fd, KIOCLAYOUT, &l);
	for (i = 0; i < 128; i++) {
		for (j = 0; j < 4; j++) {
			k.kio_tablemask = x[j];
			k.kio_station = i;
			switch(j) {
			case 0:
				k.kio_entry = map[i].unshift;
				break;
			case 1:
				k.kio_entry = map[i].shift;
				break;
			case 2:
				k.kio_entry = map[i].altgr;
				break;
			case 3:
				k.kio_entry = map[i].control;
				break;
			}
			r = ioctl(fd, KIOCSKEY, &k);
		}
	}
	close(fd);

	if (r == -1) {
		printf("failure to set keyboard mapping\n");
		return;
	}

	if (verbose)
		fprintf(stderr, "keyboard mapping set to %s\n", name);
}
