/* $Id: cmds.c,v 1.17 2001/10/02 16:22:40 rees Exp $ */

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

#ifdef __palmos__
#pragma pack(2)
#include <Common.h>
#include <System/SysAll.h>
#include <UI/UIAll.h>
#include <System/Unix/sys_types.h>
#include <System/Unix/unix_stdio.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/unix_string.h>
#include <string.h>
#include "getopt.h"
#include "sectok.h"
#include "field.h"
#else
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sectok.h>
#endif

#include "sc.h"

#define MAXFILELEN 0xffff
#define CARDIOSIZE 200

struct dispatchtable dispatch_table[] = {
    /* Non-card commands */
    { "help", "[command]", help },
    { "?", "[command]", help },
    { "reset", "[ -1234ivf ]", reset },
    { "open", "[ -1234ivf ]", reset },
    { "close", "", dclose },
    { "quit", "", quit },

    /* 7816-4 commands */
    { "apdu", "[ -c class ] ins p1 p2 p3 data ...", apdu },
    { "fid", "[ -v ] fid/aid", selfid },
    { "isearch", "", isearch },
    { "csearch", "", csearch },
    { "class", "[ class ]", class },
    { "read", "[ -x ] filesize", dread },
    { "write", "input-filename", dwrite },
    { "challenge", "[ size ]", challenge },

    /* Cyberflex commands */
    { "ls", "[ -l ]", ls },
    { "acl", "[ -x ] fid [ principal: r1 r2 ... ]", acl },
    { "create", "fid size", jcreate },
    { "delete", "fid", jdelete },
    { "jdefault", "[ -d ]", jdefault },
    { "jatr", "", jatr },
    { "jdata", "", jdata },
    { "login", "[ -d ] [ -k keyno ] [ -v ] [ -x hex-aut0 ]", jlogin },
#ifndef __palmos__
    { "jaut", "", jaut },
    { "jload", "[ -p progID ] [ -c contID ] [ -s cont_size ] [ -i inst_size ] [ -a aid ] [ -v ] filename", jload },
#endif
    { "junload", "[ -p progID ] [ -c contID ]", junload },
#ifndef __palmos__
    { "setpass", "[ -d ] [ -x hex-aut0 ]", jsetpass },
#endif
    { NULL, NULL, NULL }
};

int curlen;

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
    int i, j;

    if (ac < 2) {
	for (i = 0; dispatch_table[i].cmd; i++)
	    printf("%s\n", dispatch_table[i].cmd);
    } else {
	for (j = 1; j < ac; j++) {
	    for (i = 0; dispatch_table[i].cmd; i++)
		if (!strncmp(av[j], dispatch_table[i].cmd, strlen(av[j])))
		    break;
	    if (dispatch_table[i].help)
		printf("%s %s\n", dispatch_table[i].cmd, dispatch_table[i].help);
	    else
		printf("no help on \"%s\"\n", av[j]);
	}
    }

    return 0;
}

int reset(int ac, char *av[])
{
    int i, n, oflags = 0, rflags = 0, vflag = 0, sw;
    unsigned char atr[34];
    struct scparam param;

    optind = optreset = 1;

    while ((i = getopt(ac, av, "0123ivf")) != -1) {
	switch (i) {
	case '0':
	case '1':
	case '2':
	case '3':
	    port = i - '0';
	    break;
	case 'i':
	    oflags |= STONOWAIT;
	    break;
	case 'v':
	    vflag = 1;
	    break;
	case 'f':
	    rflags |= STRFORCE;
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

    aut0_vfyd = 0;

    n = sectok_reset(fd, rflags, atr, &sw);
    if (vflag) {
#ifdef __palmos__
	hidefield(printfield->id);
	sectok_parse_atr(fd, STRV, atr, n, &param);
	showfield(printfield->id);
#else
	sectok_parse_atr(fd, STRV, atr, n, &param);
#endif
    }
    if (!sectok_swOK(sw)) {
	printf("sectok_reset: %s\n", sectok_get_sw(sw));
	dclose(0, NULL);
	return -1;
    }

    return 0;
}

int dclose(int ac, char *av[])
{
    if (fd >= 0) {
	sectok_close(fd);
	fd = -1;
    }
    return 0;
}

int quit(int ac, char *av[])
{
    dclose(0, NULL);
#ifndef __palmos__
    exit(0);
#else
    return -1;
#endif
}

int apdu(int ac, char *av[])
{
    int i, ilen, olen, n, ins, xcl = cla, p1, p2, p3, sw;
    unsigned char ibuf[256], obuf[256], *bp;

    optind = optreset = 1;

    while ((i = getopt(ac, av, "c:")) != -1) {
	switch (i) {
	case 'c':
	    sscanf(optarg, "%x", &xcl);
	    break;
	}
    }

    if (ac - optind < 4) {
	printf("usage: apdu [ -c class ] ins p1 p2 p3 data ...\n");
	return -1;
    }

    sscanf(av[optind++], "%x", &ins);
    sscanf(av[optind++], "%x", &p1);
    sscanf(av[optind++], "%x", &p2);
    sscanf(av[optind++], "%x", &p3);

    for (bp = ibuf, i = optind, ilen = 0; i < ac; i++) {
	sscanf(av[i], "%x", &n);
	*bp++ = n;
	ilen++;
    }

    if (fd < 0 && reset(0, NULL) < 0)
	return -1;

    olen = (p3 && !ilen) ? p3 : sizeof obuf;

    n = sectok_apdu(fd, xcl, ins, p1, p2, ilen, ibuf, olen, obuf, &sw);

    sectok_dump_reply(obuf, n, sw);

    return 0;
}

int selfid(int ac, char *av[])
{
    unsigned char fid[16], obuf[256];
    char *fname;
    int i, n, sel, fidlen, vflag = 0, sw;

    optind = optreset = 1;

    while ((i = getopt(ac, av, "v")) != -1) {
	switch (i) {
	case 'v':
	    vflag = 1;
	    break;
	}
    }

    if (ac - optind == 0) {
	/* No fid/aid given; select null aid (default loader for Cyberflex) */
	sel = 4;
	fidlen = 0;
    } else {
	fname = av[optind++];
	if (!strcmp(fname, "..")) {
	    /* Special case ".." means parent */
	    sel = 3;
	    fidlen = 0;
	} else if (strlen(fname) < 5) {
	    /* fid */
	    sel = 0;
	    fidlen = 2;
	    sectok_parse_fname(fname, fid);
	} else {
	    /* aid */
	    sel = 4;
	    fidlen = sectok_parse_input(fname, fid, sizeof fid);
	    if (fname[0] == '#') {
		/* Prepend 0xfc to the aid to make it a "proprietary aid". */
		fid[0] = 0xfc;
	    }
	}
    }

    if (fd < 0 && reset(0, NULL) < 0)
	return -1;

    n = sectok_apdu(fd, cla, 0xa4, sel, 0, fidlen, fid, 256, obuf, &sw);
    if (!sectok_swOK(sw)) {
	printf("Select %02x%02x: %s\n", fid[0], fid[1], sectok_get_sw(sw));
	return -1;
    }

    if (vflag && !n && sectok_r1(sw) == 0x61 && sectok_r2(sw)) {
	/* The card has out data but we must explicitly ask for it */
	n = sectok_apdu(fd, cla, 0xc0, 0, 0, 0, NULL, sectok_r2(sw), obuf, &sw);
    }

    if (n >= 4) {
	/* Some cards put the file length here. No guarantees. */
	curlen = (obuf[2] << 8) | obuf[3];
    }

    if (vflag)
	sectok_dump_reply(obuf, n, sw);

    return 0;
}

int isearch(int ac, char *av[])
{
    int i, r1, sw;

    if (fd < 0 && reset(0, NULL) < 0)
	return -1;

    /* find instructions */
    for (i = 0; i < 0xff; i += 2) {
	sectok_apdu(fd, cla, i, 0, 0, 0, NULL, 0, NULL, &sw);
	r1 = sectok_r1(sw);
	if (r1 != 0x06 && r1 != 0x6d && r1 != 0x6e)
	    printf("%02x %s %s\n", i, sectok_get_ins(i), sectok_get_sw(sw));
    }
    return 0;
}

int csearch(int ac, char *av[])
{
    int i, r1, sw;

    if (fd < 0 && reset(0, NULL) < 0)
	return -1;

    /* find app classes */
    for (i = 0; i <= 0xff; i++) {
	sectok_apdu(fd, i, 0xa4, 0, 0, 2, root_fid, 0, NULL, &sw);
	r1 = sectok_r1(sw);
	if (r1 != 0x06 && r1 != 0x6d && r1 != 0x6e)
	    printf("%02x %s\n", i, sectok_get_sw(sw));
    }
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
    int i, n, col = 0, fsize, xflag = 0, sw;
    unsigned int p3;
    unsigned char buf[CARDIOSIZE+1];

    optind = optreset = 1;

    while ((i = getopt(ac, av, "x")) != -1) {
	switch (i) {
	case 'x':
	    xflag = 1;
	    break;
	}
    }

    if (ac - optind < 1)
	fsize = curlen;
    else
	sscanf(av[optind++], "%d", &fsize);

    if (!fsize) {
	printf("please specify filesize\n");
	return -1;
    }

    if (fd < 0 && reset(0, NULL) < 0)
	return -1;

    for (p3 = 0; fsize && p3 < MAXFILELEN; p3 += n) {
	n = (fsize < CARDIOSIZE) ? fsize : CARDIOSIZE;
	n = sectok_apdu(fd, cla, 0xb0, p3 >> 8, p3 & 0xff, 0, NULL, n, buf, &sw);
	if (!sectok_swOK(sw)) {
	    printf("ReadBinary: %s\n", sectok_get_sw(sw));
	    break;
	}
#ifdef __palmos__
	if (xflag) {
	    hidefield(printfield->id);
	    for (i = 0; i < n; i++) {
		printf("%02x ", buf[i]);
		if (col++ % 12 == 11)
		    printf("\n");
	    }
	    showfield(printfield->id);
	} else {
	    buf[n] = '\0';
	    printf("%s", buf);
	}
#else
	if (xflag) {
	    for (i = 0; i < n; i++) {
		printf("%02x ", buf[i]);
		if (col++ % 16 == 15)
		    printf("\n");
	    }
	} else
	    fwrite(buf, 1, n, stdout);
#endif
	fsize -= n;
    }

    if (xflag && col % 16 != 0)
	printf("\n");

    return 0;
}

#ifndef __palmos__
int dwrite(int ac, char *av[])
{
    int n, p3, sw;
    FILE *f;
    unsigned char buf[CARDIOSIZE];

    if (ac != 2) {
	printf("usage: write input-filename\n");
	return -1;
    }

    if (fd < 0 && reset(0, NULL) < 0)
	return -1;

    f = fopen(av[1], "r");
    if (!f) {
	printf("can't open %s\n", av[1]);
	return -1;
    }

    n = 0;
    while ((p3 = fread(buf, 1, CARDIOSIZE, f)) > 0) {
	sectok_apdu(fd, cla, 0xd6, n >> 8, n & 0xff, p3, buf, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
	    printf("UpdateBinary: %s\n", sectok_get_sw(sw));
	    break;
	}
	n += p3;
    }
    fclose(f);

    return (n ? 0 : -1);
}
#else
int dwrite(int ac, char *av[])
{
    int n, sw;
    char *s;

    if (ac != 2) {
	printf("usage: write text\n");
	return -1;
    }
    s = av[1];
    n = strlen(s);
    sectok_apdu(fd, cla, 0xd6, 0, 0, n, s, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	printf("UpdateBinary: %s\n", sectok_get_sw(sw));
	return -1;
    }
    return 0;
}
#endif

int challenge(int ac, char *av[])
{
    int n = 8, sw;
    unsigned char buf[256];

    if (ac > 1)
	n = atoi(av[1]);

    n = sectok_apdu(fd, cla, 0x84, 0, 0, 0, NULL, n, buf, &sw);

    if (!sectok_swOK(sw)) {
	printf("GetChallenge: %s\n", sectok_get_sw(sw));
	return -1;
    }

    sectok_dump_reply(buf, n, sw);
    return 0;
}
