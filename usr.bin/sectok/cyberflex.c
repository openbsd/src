/* $Id: cyberflex.c,v 1.5 2001/07/16 23:09:36 rees Exp $ */

/*
copyright 1999, 2000
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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#ifdef __linux
#include <openssl/des.h>
#else /* __linux */
#include <des.h>
#endif
#include <sha1.h>
#include <sectok.h>
#include <sc7816.h>

#include "sc.h"

#ifdef __sun
#define des_set_key(key, schedule) des_key_sched(key, schedule)
#endif

#define MAX_KEY_FILE_SIZE 1024
#define NUM_RSA_KEY_ELEMENTS 5
#define RSA_BIT_LEN 1024
#define KEY_FILE_HEADER_SIZE 8

static unsigned char key_fid[] = {0x00, 0x11};
static unsigned char DFLTATR[] = {0x81, 0x10, 0x06, 0x01};
static unsigned char DFLTAUT0[] = {0xad, 0x9f, 0x61, 0xfe, 0xfa, 0x20, 0xce, 0x63};

int aut0_vfyd;

/* default signed applet key of Cyberflex Access */
static des_cblock app_key = {0x6A, 0x21, 0x36, 0xF5, 0xD8, 0x0C, 0x47, 0x83};

char *apptype[] = {
    "?",
    "applet",
    "app",
    "app/applet",
};

char *appstat[] = {
    "?",
    "created",
    "installed",
    "registered",
};

char *filestruct[] = {
    "binary",
    "fixed rec",
    "variable rec",
    "cyclic",
    "program",
};

static int
get_AUT0(int ac, char *av[], char *prompt, unsigned char *digest)
{
    int i, dflag = 0, xflag = 0;
    SHA1_CTX ctx;
    char *s;

    optind = optreset = 1;
    opterr = 0;

    while ((i = getopt(ac, av, "dx:")) != -1) {
	switch (i) {
	case 'd':
	    memmove(digest, DFLTAUT0, sizeof DFLTAUT0);
	    dflag = 1;
	    break;
	case 'x':
	    if (parse_input(optarg, digest, 8) != 8) {
		printf("AUT0 must be length 8\n");
		return -1;
	    }
	    xflag = 1;
	    break;
	}
    }

    if (!dflag && !xflag) {
	SHA1Init(&ctx);
	s = getpass(prompt);
	SHA1Update(&ctx, s, strlen(s));
	bzero(s, strlen(s));
	SHA1Final(digest, &ctx);
    }

    return 0;
}

int jaut(int ac, char *av[])
{
    int i, vflag = 0, sw;
    unsigned char AUT0[20];

    if (fd < 0)
	reset(0, NULL);

    optind = optreset = 1;

    while ((i = getopt(ac, av, "dvx:")) != -1) {
	switch (i) {
	case 'v':
	    vflag = 1;
	    break;
	}
    }

    if (get_AUT0(ac, av, "Enter AUT0 passphrase: ", AUT0) < 0)
	return -1;

    cla = cyberflex_inq_class(fd);

    if (vflag) {
	printf("Class %02x\n", cla);
	for (i = 0; i < 8; i++)
	    printf("%02x ", AUT0[i]);
	printf("\n");
    }

    sectok_apdu(fd, cla, 0x2a, 0, 0, 8, AUT0, 0, NULL, &sw);

    if (!sectok_swOK(sw)) {
	printf("AUT0 failed: %s\n", sectok_get_sw(sw));
	aut0_vfyd = 0;
	return -1;
    }
    aut0_vfyd = 1;
    return 0;
}

int jdefault(int ac, char *av[])
{
    unsigned char buf[8];
    int i, p1 = 4, sw;

    optind = optreset = 1;

    while ((i = getopt(ac, av, "d")) != -1) {
	switch (i) {
	case 'd':
	    p1 = 5;
	    break;
	}
    }

    if (fd < 0)
	reset(0, NULL);
    if (!aut0_vfyd)
	jaut(0, NULL);

    sectok_apdu(fd, cla, 0x08, p1, 0, 0, buf, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	sectok_print_sw(sw);
	return -1;
    }
    return 0;
}

int jatr(int ac, char *av[])
{
    unsigned char buf[64];
    int n = 0, sw;

    if (fd < 0)
	reset(0, NULL);

    buf[n++] = 0x90;
    buf[n++] = 0x94;		/* TA1 */
    buf[n++] = 0x40;		/* TD1 */
    buf[n++] = 0x28;		/* TC2 (WWT=4sec) */
    if (ac > optind) {
	/* set historical bytes from command line */
	n += parse_input(av[1], &buf[n], 15);
    } else {
	/* no historical bytes given, use default */
	memmove(&buf[n], DFLTATR, sizeof DFLTATR);
	n += sizeof DFLTATR;
    }
    buf[0] |= ((n - 2) & 0xf);
    sectok_apdu(fd, cla, 0xfa, 0, 0, n, buf, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	sectok_print_sw(sw);
	return -1;
    }
    return 0;
}

int jdata(int ac, char *av[])
{
    unsigned char buf[32];
    int i, sw;

    if (fd < 0)
	reset(0, NULL);

    sectok_apdu(fd, cla, 0xca, 0, 1, 0, NULL, 0x16, buf, &sw);
    if (sectok_swOK(sw)) {
	printf("serno ");
	for (i = 0; i < 6; i++)
	    printf("%02x ", buf[i]);
	printf("batch %02x sver %d.%02d ", buf[6], buf[7], buf[8]);
	if (buf[9] == 0x0c)
	    printf("augmented ");
	else if (buf[9] == 0x0b)
	    ;
	else
	    printf("unknown ");
	printf("crypto %9.9s class %02x\n", &buf[10], buf[19]);
    } else {
	/* error */
	sectok_print_sw(sw);
    }
    return 0;
}

#define JDIRSIZE 40

int ls(int ac, char *av[])
{
    int p2, f0, f1, sw;
    char ftype[32], fname[6];
    unsigned char buf[JDIRSIZE];

    if (fd < 0)
	reset(0, NULL);

    for (p2 = 0; ; p2++) {
	if (sectok_apdu(fd, cla, 0xa8, 0, p2, 0, NULL, JDIRSIZE, buf, &sw) < 0)
	    break;
	if (!sectok_swOK(sw))
	    break;
	f0 = buf[4];
	f1 = buf[5];
	if (f0 == 0xff || f0 + f1 == 0)
	    continue;
	sectok_fmt_fid(fname, f0, f1);
	if (buf[6] == 1)
	    /* root */
	    sprintf(ftype, "root");
	else if (buf[6] == 2) {
	    /* DF */
	    if (buf[12] == 27)
		sprintf(ftype, "%s %s", appstat[buf[10]], apptype[buf[9]]);
	    else
		sprintf(ftype, "directory");
	} else if (buf[6] == 4)
	    /* EF */
	    sprintf(ftype, "%s", filestruct[buf[13]]);
	printf("%4s %5d %s\n", fname, (buf[2] << 8) | buf[3], ftype);
    }
    return 0;
}

int jcreate(int ac, char *av[])
{
    unsigned char fid[2];
    int sw, fsize;

    if (ac != 3) {
	printf("usage: create fid size\n");
	return -1;
    }

    sectok_parse_fname(av[1], fid);
    sscanf(av[2], "%d", &fsize);

    if (fd < 0)
	reset(0, NULL);
    if (!aut0_vfyd)
	jaut(0, NULL);

    if (cyberflex_create_file(fd, cla, fid, fsize, 3, &sw) < 0) {
	printf("create_file: %s\n", sectok_get_sw(sw));
	return -1;
    }

    return 0;
}

int jdelete(int ac, char *av[])
{
    unsigned char fid[2];
    int sw;

    if (ac != 2) {
	printf("usage: delete fid\n");
	return -1;
    }

    sectok_parse_fname(av[1], fid);

    if (fd < 0)
	reset(0, NULL);
    if (!aut0_vfyd)
	jaut(0, NULL);

    if (cyberflex_delete_file(fd, cla, fid, &sw) < 0) {
	printf("delete_file: %s\n", sectok_get_sw(sw));
	return -1;
    }

    return 0;
}

#define MAX_BUF_SIZE 256
#define MAX_APP_SIZE 4096
#define MAX_APDU_SIZE 0xfa
#define BLOCK_SIZE 8
#define MAXTOKENS 16

unsigned char *app_name;
unsigned char progID[2], contID[2], aid[MAX_BUF_SIZE];
int cont_size, inst_size;
int aid_len;

int analyze_load_options(int ac, char *av[])
{
    int i, rv;

    progID[0] = 0x77;
    progID[1] = 0x77;
    contID[0] = 0x77;
    contID[1] = 0x78;
    cont_size = 1152;
    inst_size = 1024;
    aid_len = 5;

    for (i = 0 ; i < 16 ; i ++)
	aid[i] = 0x77;

    /* applet file name */
    app_name = av[ac - 1];

    optind = optreset = 1;

    /* switch on options */
    while (1) {
	rv = getopt (ac, av, "p:c:s:i:a:");
	if (rv == -1) break;
	/*printf ("rv=%c, optarg=%s\n", rv, optarg);*/
	switch (rv) {
	case 'p':
	    parse_input(optarg, progID, 2);
	    break;
	case 'c':
	    parse_input(optarg, contID, 2);
	    break;
	case 's':
	    sscanf(optarg, "%d", &cont_size);
	    break;
	case 'i':
	    sscanf(optarg, "%d", &inst_size);
	    break;
	case 'a':
	    aid_len = parse_input(optarg, aid, sizeof aid);
	    /*printf ("aid_len = %d\n", aid_len);*/
	    break;
	default:
	    printf ("unknown option.  command aborted.\n");
	    return -1;
	}
    }

    return 0;
}

int jload(int ac, char *av[])
{
    char progname[5], contname[5];
    unsigned char app_data[MAX_APP_SIZE],
    data[MAX_BUF_SIZE];
    int i, j, fd_app, size, rv, sw;
    des_cblock tmp;
    des_key_schedule schedule;

    if (analyze_load_options(ac, av) < 0)
	return -1;

    if (fd < 0)
	reset(0, NULL);
    if (!aut0_vfyd)
	jaut(0, NULL);

    sectok_fmt_fid(progname, progID[0], progID[1]);
    sectok_fmt_fid(contname, contID[0], contID[1]);

    printf ("applet file             \"%s\"\n", app_name);
    printf ("program ID              %s\n", progname);
    printf ("container ID            %s\n", contname);
    printf ("instance container size %d\n", cont_size);
    printf ("instance data size      %d\n", inst_size);
    printf ("AID                     ");
    for (i = 0 ; i < aid_len ; i ++ )
	printf ("%02x", (unsigned char)aid[i]);
    printf ("\n");

    /* open the input file */
    fd_app = open (app_name, O_RDONLY, NULL);
    if (fd_app == -1) {
	fprintf (stderr, "cannot open file \"%s\"\n", app_name);
	return -1;
    }

    /* read the input file */
    size = read (fd_app, app_data, MAX_APP_SIZE);
    if (size == 0) {
	fprintf (stderr, "file %s size 0??\n", app_name);
	return -1;
    }
    if (size == -1) {
	fprintf (stderr, "error reading file %s\n", app_name);
	return -1;
    }

    /*printf ("file size %d\n", size);*/

    /* size must be able to be divided by BLOCK_SIZE */
    if (size % BLOCK_SIZE != 0) {
	fprintf (stderr, "file (%s) size cannot be divided by BLOCK_SIZE.\n",
		 app_name);
	return -1;
    }

    /* compute the signature of the applet */
    /* initialize the result buffer */
    for (j = 0; j < BLOCK_SIZE; j++ ) {
	tmp[j] = 0;
    }

    /* chain.  DES encrypt one block, XOR the cyphertext with the next block,
       ... continues until the end of the buffer */

    des_set_key (&app_key, schedule);

    for (i = 0; i < size/BLOCK_SIZE; i++) {
	for (j = 0; j < BLOCK_SIZE; j++ ){
	    tmp[j] = tmp[j] ^ app_data[i*BLOCK_SIZE + j];
	}
	des_ecb_encrypt (&tmp, &tmp, schedule, DES_ENCRYPT);
    }

    /* print out the signature */
    printf ("signature ");
    for (j = 0; j < BLOCK_SIZE; j++ ) {
	printf ("%02x", tmp[j]);
    }
    printf ("\n");

    /* select the default loader */
    rv = sectok_apdu(fd, cla, 0xa4, 0x04, 0, 0, NULL, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	printf("can't select default loader: %s\n", sectok_get_sw(sw));
	return -1;
    }

    /* select 3f.00 (root) */
    if (sectok_selectfile(fd, cla, root_fid, &sw) < 0)
	return -1;

    /* create program file */
    if (cyberflex_create_file(fd, cla, progID, size, 3, &sw) < 0) {
	/* error */
	printf("can't create %s: %s\n", progname, sectok_get_sw(sw));
	return -1;
    }

    /* update binary */
    for (i = 0; i < size; i += MAX_APDU_SIZE) {
	int send_size;

	/* compute the size to be sent */
	if (size - i > MAX_APDU_SIZE) send_size = MAX_APDU_SIZE;
	else send_size = size - i;

	rv = sectok_apdu(fd, cla, 0xd6, i / 256, i % 256, send_size, app_data + i, 0, NULL, &sw);

	if (!sectok_swOK(sw)) {
	    /* error */
	    printf("updating binary %s: %s\n", progname, sectok_get_sw(sw));
	    return -1;
	}
    }

    /* manage program .. validate */
    rv = sectok_apdu(fd, cla, 0x0a, 01, 0, 0x08, tmp, 0, NULL, &sw);

    if (!sectok_swOK(sw)) {
	/* error */
	printf("validating applet in %s: %s\n", progname, sectok_get_sw(sw));
	return -1;
    }

    /* select the default loader */
    rv = sectok_apdu(fd, cla, 0xa4, 0x04, 0, 0, NULL, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	printf("selecting default loader: %s\n", sectok_get_sw(sw));
	return -1;
    }

    /* execute method -- call the install() method in the cardlet.
       cardlet type 01 (applet, not application)
       program ID (7777)
       instance container size (0800 (1152))
       instance container ID (7778)
       instance data size (0400 (1024))
       AID length (0005 (5 byte))
       AID (7777777777) */

    data[0] = 0x01;		/* cardlet type = 1 (applet, not application) */
    data[1] = progID[0];	/* FID, upper */
    data[2] = progID[1];	/* FID, lower */
    data[3] = cont_size / 256;	/* instance container size 0x0800 (1152) byte, upper */
    data[4] = cont_size % 256;	/* instance container size 0x0800 (1152) byte, lower */
    data[5] = contID[0];	/* container ID (7778), upper */
    data[6] = contID[1];	/* container ID (7778), lower */
    data[7] = inst_size / 256;	/* instance size 0x0400 (1024) byte, upper */
    data[8] = inst_size % 256;	/* instance size 0x0400 (1024) byte, lower */
    data[9] = 0x00;		/* AID length 0x0005, upper */
    data[10] = aid_len;		/* AID length 0x0005, lower */
    for (i = 0; i < aid_len; i++) data[i + 11] = (unsigned int)aid[i];
    /* AID (7777777777) */

    rv = sectok_apdu(fd, cla, 0x0c, 0x13, 0, 11 + aid_len, data, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	printf("executing install() method in applet %s: %s\n", progname, sectok_get_sw(sw));
	return -1;
    }

    /* That's it! :) */
    return 0;
}

int junload(int ac, char *av[])
{
    char progname[5], contname[5];
    int sw, rv;

    if (analyze_load_options(ac, av) < 0)
	return -1;

    if (fd < 0)
	reset(0, NULL);
    if (!aut0_vfyd)
	jaut(0, NULL);

    sectok_fmt_fid(progname, progID[0], progID[1]);
    sectok_fmt_fid(contname, contID[0], contID[1]);

    printf ("program ID              %s\n", progname);
    printf ("container ID            %s\n", contname);

    /* select 3f.00 (root) */
    if (sectok_selectfile(fd, cla, root_fid, &sw) < 0) {
	printf("can't select root: %s\n", sectok_get_sw(sw));
	return -1;
    }

    /* select program file */
    if (sectok_selectfile(fd, cla, progID, &sw) >= 0) {

	/* manage program -- reset */
	rv = sectok_apdu(fd, cla, 0x0a, 02, 0, 0, NULL, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
	    /* error */
	    printf("resetting applet: %s\n", sectok_get_sw(sw));
	}

	/* delete program file */
	if (cyberflex_delete_file(fd, cla, progID, &sw) < 0)
	    printf("delete_file %s: %s\n", progname, sectok_get_sw(sw));
    } else
	printf ("no program file... proceed to delete data container\n");

    /* delete data container */
    if (cyberflex_delete_file(fd, cla, contID, &sw) < 0)
	printf("delete_file %s: %s\n", contname, sectok_get_sw(sw));

    return 0;
}

int jselect(int ac, char *av[])
{
    int i, sw, rv;
    unsigned char data[MAX_BUF_SIZE];

    optind = optreset = 1;

    if (analyze_load_options(ac, av) < 0)
	return -1;

    if (fd < 0)
	reset(0, NULL);

    printf ("select applet\n");
    printf ("AID                     ");
    for (i = 0 ; i < aid_len ; i ++ )
	printf ("%02x", (unsigned char)aid[i]);
    printf ("\n");

    /* select the cardlet (7777777777) */
    for (i = 0; i < aid_len; i++) data[i] = (unsigned char)aid[i];
    /* quick hack in select_applet()
       even with F0 card, select applet APDU (00 a4 04)
       only accepts class byte 00 (not f0) */

    rv = sectok_apdu(fd, cla, 0xa4, 0x04, 0, aid_len, data, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	printf ("selecting the cardlet: ");
	for (i = 0 ; i < aid_len ; i ++ ) {
	    printf ("%02x", (unsigned char)aid[i]);
	}
	printf ("\n");
	sectok_print_sw(sw);
	return -1;
    }

    return 0;
}

int jdeselect(int ac, char *av[])
{
    int sw;

    if (fd < 0)
	reset(0, NULL);

    sectok_apdu(fd, cla, 0xa4, 0x04, 0, 0, NULL, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	printf("selecting default loader: %s\n", sectok_get_sw(sw));
	return -1;
    }

    return 0;
}

#define DELIMITER " :\t\n"
#define KEY_BLOCK_SIZE 14

/* download DES keys into 3f.00/00.11 */
int cyberflex_load_key (int fd, unsigned char *buf)
{
    int sw, rv, argc = 0, i, j, tmp;
    unsigned char *token;
    unsigned char data[MAX_BUF_SIZE];
    unsigned char key[BLOCK_SIZE];

#if 0
    /* select the default loader */
    rv = scwrite(fd, cla, 0xa4, 0x04, 0, 0x00, NULL, &r1, &r2);
    if (r1 != 0x90 && r1 != 0x61) {
	// error
	    printf ("selecting the default loader: ");
	print_r1r2 (r1, r2);
	return -1;
    }
#endif

    printf ("ca_load_key buf=%s\n", buf);
    token = strtok (buf, DELIMITER);
    token = strtok (NULL, DELIMITER);
    if (token == NULL) {
	printf ("Usage: jk number_of_keys\n");
	return -1;
    }
    argc = atoi (token);

    if (argc > 2) {
	printf ("current Cyberflex Access cannot download more than 2 keys to the key file.  Sorry. :(\n");
	return -1;
    }

    if (argc < 0) {
	printf ("you want to down load %d keys??\n", argc);
	return -1;
    }

    if (!aut0_vfyd)
	jaut(0, NULL);

    /* Now let's do it. :) */

    /* add the AUT0 */
    cyberflex_fill_key_block (data, 0, 1, DFLTAUT0);

    /* add the applet sign key */
    cyberflex_fill_key_block (data+KEY_BLOCK_SIZE, 5, 0, app_key);

    /* then add user defined keys */
    for ( i = 0 ; i < argc ; i++ ) {
	printf ("key %d : ", i);
	for ( j = 0 ; j < BLOCK_SIZE ; j++ ) {
	    fscanf (cmdf, "%02x", &tmp);
	    key[j] = (unsigned char)tmp;
	}

	cyberflex_fill_key_block (data + 28 + i*KEY_BLOCK_SIZE, 6 + i, 0, key);
    }

    /* add the suffix */
    data[28 + argc*KEY_BLOCK_SIZE] = 0;
    data[28 + argc*KEY_BLOCK_SIZE + 1] = 0;

    for ( i = 0 ; i < KEY_BLOCK_SIZE * (argc + 2) + 2; i++ )
	printf ("%02x ", data[i]);
    printf ("\n");

    /* select 3f.00 (root) */
    if (sectok_selectfile(fd, cla, root_fid, &sw) < 0) {
	printf("select root: %s\n", sectok_get_sw(sw));
	return -1;
    }

    /* select 00.11 (key file) */
    if (sectok_selectfile(fd, cla, key_fid, &sw) < 0) {
	printf("select key file: %s\n", sectok_get_sw(sw));
	return -1;
    }

    /* all righty, now let's send it to the card! :) */
    rv = sectok_apdu(fd, cla, 0xd6, 0, 0, KEY_BLOCK_SIZE * (argc + 2) + 2, data, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	printf("writing the key file 00.11: %s\n", sectok_get_sw(sw));
	return -1;
    }

    return 0;
}

/* download AUT0 key into 3f.00/00.11 */
int jsetpass(int ac, char *av[])
{
    int sw, rv;
    unsigned char data[MAX_BUF_SIZE];
    unsigned char AUT0[20];

    if (!aut0_vfyd && jaut(0, NULL) < 0)
	return -1;

    if (get_AUT0(ac, av, "Enter new AUT0 passphrase: ", AUT0) < 0)
	return -1;

    cyberflex_fill_key_block (data, 0, 1, AUT0);

#if 0
    /* add the suffix */
    data[KEY_BLOCK_SIZE] = 0;
    data[KEY_BLOCK_SIZE + 1] = 0;
#endif

#ifdef DEBUG
    for (i = 0; i < KEY_BLOCK_SIZE; i++)
	printf ("%02x ", data[i]);
    printf ("\n");
#endif

    /* select 3f.00 (root) */
    if (sectok_selectfile(fd, cla, root_fid, &sw) < 0)
	return -1;

    /* select 00.11 (key file) */
    if (sectok_selectfile(fd, cla, key_fid, &sw) < 0)
	return -1;

    /* all righty, now let's send it to the card! :) */
    rv = sectok_apdu(fd, cla, 0xd6, 0, 0, KEY_BLOCK_SIZE, data, 0, NULL, &sw);
    if (!sectok_swOK(sw)) {
	/* error */
	printf("writing the key file 00.11: %s\n", sectok_get_sw(sw));
	return -1;
    }

    return 0;
}

/* download RSA private key into 3f.00/00.12 */
int cyberflex_load_rsa(int fd, unsigned char *buf)
{
    int rv, sw, i, j, tmp;
    static unsigned char key_fid[] = {0x00, 0x12};
    static char *key_names[NUM_RSA_KEY_ELEMENTS]= {"p", "q", "1/p mod q",
						       "d mod (p-1)", "d mod (q-1)"};
    unsigned char *key_elements[NUM_RSA_KEY_ELEMENTS];

    printf ("ca_load_rsa_priv buf=%s\n", buf);

    printf ("input 1024 bit RSA CRT key\n");
    for (i = 0; i < NUM_RSA_KEY_ELEMENTS; i++) {
	printf ("%s (%d bit == %d byte) : ", key_names[i],
		RSA_BIT_LEN/2, RSA_BIT_LEN/2/8);
	key_elements[i] = (unsigned char *) malloc(RSA_BIT_LEN/8);
	for ( j = 0 ; j < RSA_BIT_LEN/8/2 ; j++ ) {
	    fscanf (cmdf, "%02x", &tmp);
	    key_elements[i][j] = (unsigned char)tmp;
	}
    }

#ifdef DEBUG
    printf ("print RSA CRT key\n");
    for (i = 0 ; i < NUM_RSA_KEY_ELEMENTS ; i ++ ) {
	printf ("%s : ", key_names[i]);
	for ( j = 0 ; j < RSA_BIT_LEN/8/2 ; j++ ) {
	    printf ("%02x ", key_elements[i][j]);
	}
    }
#endif

    if (!aut0_vfyd)
	jaut(0, NULL);

    rv = cyberflex_load_rsa_priv(fd, cla, key_fid, NUM_RSA_KEY_ELEMENTS, RSA_BIT_LEN,
				 key_elements, &sw);

    if (rv < 0)
	printf("load_rsa_priv: %s\n", sectok_get_sw(sw));

    for (i = 0; i < NUM_RSA_KEY_ELEMENTS; i++)
	free(key_elements[i]);
    return rv;
}
