/* $Id: cmds.c,v 1.3 2001/07/02 20:15:06 rees Exp $ */

/*
 * Smartcard commander.
 * Written by Jim Rees and others at University of Michigan.
 */

/*
copyright 2001
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sectok.h>
#include <sc7816.h>

#include "sc.h"

#define CARDIOSIZE 200

struct {
    char *cmd;
    int (*action) (int ac, char *av[]);
} dispatch_table[] = {
    /* Non-card commands */
    { "help", help },
    { "?", help },
    { "reset", reset },
    { "open", reset },
    { "close", dclose },
    { "quit", quit },

    /* 7816-4 commands */
    { "apdu", apdu },
    { "fid", selfid },
    { "isearch", isearch },
    { "class", class },
    { "read", dread },
    { "write", dwrite },

    /* Cyberflex commands */
    { "ls", ls },
    { "create", jcreate },
    { "delete", jdelete },
    { "jdefault", jdefault },
    { "jatr", jatr },
    { "jdata", jdata },
    { "jaut", jaut },
    { "jload", jload },
    { "junload", junload },
    { "jselect", jselect },
    { "jdeselect", jdeselect },
    { NULL, NULL }
};
/*
    { "",  },
*/

int dispatch(int ac, char *av[])
{
    int i;

    if (ac < 1)
	return 0;

    for (i = 0; dispatch_table[i].cmd; i++) {
	if (!strncmp(av[0], dispatch_table[i].cmd, strlen(av[0]))) {
	    (dispatch_table[i].action) (ac, av);
	    break;
	}
    }
    if (!dispatch_table[i].cmd) {
	printf("unknown command \"%s\"\n", av[0]);
	return -1;
    }
    return 0;
}

int help(int ac, char *av[])
{
    int i;

    for (i = 0; dispatch_table[i].cmd; i++) {
	if (strlen(dispatch_table[i].cmd) > 1)
	    printf("%s\n", dispatch_table[i].cmd);
    }

    return 0;
}

int reset(int ac, char *av[])
{
    int i, n, oflags = 0, rflags = 0, vflag = 0, sw;
    unsigned char atr[34];
    struct scparam param;

    optind = optreset = 1;

    while ((i = getopt(ac, av, "1234ivf")) != -1) {
	switch (i) {
	case '1':
	case '2':
	case '3':
	case '4':
	    port = i - '1';
	    break;
	case 'i':
	    oflags |= STONOWAIT;
	    break;
	case 'v':
	    vflag = 1;
	    break;
	case 'f':
	    rflags |= SCRFORCE;
	    break;
	}
    }

    if (fd < 0) {
	fd = sectok_open(port, oflags, &sw);
	if (fd < 0) {
	    sectok_print_sw(sw);
	    return -1;
	}
    }

    n = scxreset(fd, rflags, atr, &sw);
    if (n && !vflag) {
	printf("atr ");
	dump_reply(atr, n, 0, 0);
    }
    if (vflag)
	parse_atr(fd, SCRV, atr, n, &param);
    if (sw != SCEOK) {
	printf("%s\n", scerrtab[sw]);
	return -1;
    }

    return 0;
}

int dclose(int ac, char *av[])
{
    if (fd >= 0) {
	scclose(fd);
	fd = -1;
    }
    return 0;
}

int quit(int ac, char *av[])
{
    exit(0);
}

int apdu(int ac, char *av[])
{
    int i, n, ins, xcl = cla, p1, p2, p3, r1, r2;
    unsigned char buf[256], obuf[256], *bp;

    optind = optreset = 1;

    while ((i = getopt(ac, av, "c:")) != -1) {
	switch (i) {
	case 'c':
	    sscanf(optarg, "%x", &xcl);
	    break;
	}
    }

    if (ac - optind < 4) {
	printf("usage: apdu [ -c cla ] ins p1 p2 p3 data ...\n");
	return -1;
    }

    sscanf(av[optind++], "%x", &ins);
    sscanf(av[optind++], "%x", &p1);
    sscanf(av[optind++], "%x", &p2);
    sscanf(av[optind++], "%x", &p3);

#if 0
    for (bp = buf, i = optind; i < ac; i++)
	bp += parse_input(av[i], bp, (int) (sizeof buf - (bp - buf)));
#else
    for (bp = buf, i = optind; i < ac; i++) {
	sscanf(av[i], "%x", &n);
	*bp++ = n;
    }
#endif

    if (fd < 0)
	reset(0, NULL);

    n = scrw(fd, xcl, ins, p1, p2, p3, buf, sizeof obuf, obuf, &r1, &r2);

    if (n < 0) {
	printf("scrw failed\n");
	return -1;
    }

    dump_reply(obuf, n, r1, r2);

    return 0;
}

int selfid(int ac, char *av[])
{
    unsigned char fid[2];
    int sw;

    if (ac != 2) {
	printf("usage: f fid\n");
	return -1;
    }

    if (fd < 0)
	reset(0, NULL);

    sectok_parse_fname(av[1], fid);
    if (sectok_selectfile(fd, cla, fid, &sw) < 0) {
	printf("selectfile: %s\n", sectok_get_sw(sw));
	return -1;
    }

    return 0;
}

int isearch(int ac, char *av[])
{
    int i, r1, r2;
    unsigned char buf[256];

    if (fd < 0)
	reset(0, NULL);

    /* find instructions */
    for (i = 0; i < 0xff; i += 2)
	if (scread(fd, cla, i, 0, 0, 0, buf, &r1, &r2) == 0
	    && r1 != 0x6d && r1 != 0x6e)
	    printf("%02x %s %s\n", i, lookup_cmdname(i), get_r1r2s(r1, r2));
    return 0;
}

int class(int ac, char *av[])
{
    if (ac > 1)
	sscanf(av[1], "%x", &cla);
    else
	printf("Class %02x\n", cla);
    return 0;
}

int dread(int ac, char *av[])
{
    int n, p3, fsize, r1, r2;
    unsigned char buf[CARDIOSIZE];

    if (ac != 2) {
	printf("usage: read filesize\n");
	return -1;
    }

    sscanf(av[1], "%d", &fsize);

    if (fd < 0)
	reset(0, NULL);

    for (p3 = 0; fsize && p3 < 100000; p3 += n) {
	n = (fsize < CARDIOSIZE) ? fsize : CARDIOSIZE;
	if (scread(fd, cla, 0xb0, p3 >> 8, p3 & 0xff, n, buf, &r1, &r2) < 0) {
	    printf("scread failed\n");
	    break;
	}
	if (r1 != 0x90 && r1 != 0x61) {
	    print_r1r2(r1, r2);
	    break;
	}
	fwrite(buf, 1, n, stdout);
	fsize -= n;
    }

    return 0;
}

int dwrite(int ac, char *av[])
{
    int n, p3, r1, r2;
    FILE *f;
    unsigned char buf[CARDIOSIZE];

    if (ac != 2) {
	printf("usage: write input-filename\n");
	return -1;
    }

    if (fd < 0)
	reset(0, NULL);

    f = fopen(av[1], "r");
    if (!f) {
	printf("can't open %s\n", av[1]);
	return -1;
    }

    n = 0;
    while ((p3 = fread(buf, 1, CARDIOSIZE, f)) > 0) {
	if (scwrite(fd, cla, 0xd6, n >> 8, n & 0xff, p3, buf, &r1, &r2) < 0) {
	    printf("scwrite failed\n");
	    break;
	}
	if (r1 != 0x90 && r1 != 0x61) {
	    print_r1r2(r1, r2);
	    break;
	}
	n += p3;
    }
    fclose(f);

    return (n ? 0 : -1);
}
