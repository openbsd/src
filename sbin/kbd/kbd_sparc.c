/*	$OpenBSD: kbd_sparc.c,v 1.5 1999/08/21 20:27:43 maja Exp $ */

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

#define	PATH_KBD	"/dev/kbd"

#define ALL(s,n)	(s), (s), (s), (s), (n), (s),
#define BB(s)	(FUNNY+(s))
#define SK(s)	(SHIFTKEYS+(s))
#define ST(s)	(0x500+(s))
#define GR(s)	(0x400+(s))
#define	C8(s)	((u_char)(s))

typedef struct {
        u_short unshift;
        u_short shift;
	u_short caps;
	u_short altgr;
	u_short numl;
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

char *
kbd_find_default()
{
	int i, j, fd, r, ok;
	int t = KB_SUN4;
	int l = 0;
	char defaultmap[30];
	char *ret;

	/* Generate the default map name */
	
	fd = open(PATH_KBD, O_RDONLY);
	if (fd != -1) {
		r = ioctl(fd, KIOCTYPE, &t);
		r = ioctl(fd, KIOCLAYOUT, &l);
		close(fd);
	}
	snprintf(defaultmap,sizeof(defaultmap),"type_%d_layout_%02x\0",t,l);

	/* Check if it exist, if not use "type_4_layout_00" */ 
	
	ret = keymaps[0].name[0];
	
	for (i = 0; keymaps[i].name[0]; i++) {
		ok = 0;
		for (j = 1; j < NUM_NAMES && keymaps[i].name[j]; j++)
			ok |= (strcmp(keymaps[i].name[j],defaultmap) == 0);
		if (ok) ret = keymaps[i].name[0]; 
	}	  

	return(ret);

}

void
kbd_list()
{
	int i, j;
	char *defmap;

	defmap = kbd_find_default();

	printf("tables available:\n%-16s %s\n\n",
		"encoding", "nick names");
	for (i = 0; keymaps[i].name[0]; i++) {
		printf("%-16s",keymaps[i].name[0]);
		for (j = 1; j < NUM_NAMES && keymaps[i].name[j]; j++)
			printf(" %s", keymaps[i].name[j]);
		if (keymaps[i].name[0] == defmap) printf(" default");
		printf("\n");
	}	  
}

void
kbd_set(name, verbose)
	char *name;
	int verbose;
{
	int i, j, fd, r;
	keymap_t *map = NULL;
	int x[] = { KIOC_NOMASK, KIOC_SHIFTMASK, KIOC_CAPSMASK,
		    KIOC_ALTGMASK, KIOC_NUMLMASK, KIOC_CTRLMASK };
	struct kiockey k;

	if(strcmp(name,"default") == 0) {
		name = kbd_find_default();
	}
	
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

	fd = open(PATH_KBD, O_RDWR);
	if (fd == -1) {
		perror(PATH_KBD);
		exit(1);
	}		

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 6; j++) {
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
				k.kio_entry = map[i].caps;
				break;
			case 3:
				k.kio_entry = map[i].altgr;
				break;
			case 4:
				k.kio_entry = map[i].numl;
				break;
			case 5:
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
