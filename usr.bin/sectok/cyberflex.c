/*	$OpenBSD: cyberflex.c,v 1.28 2007/12/30 13:35:27 sobrado Exp $ */

/*
 * copyright 1999, 2000
 * the regents of the university of michigan
 * all rights reserved
 *
 * permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the university of
 * michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  if the
 * above copyright notice or any other identification of the
 * university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * this software is provided as is, without representation
 * from the university of michigan as to its fitness for any
 * purpose, and without warranty by the university of
 * michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose. the
 * regents of the university of michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#ifndef __palmos__
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <des.h>
#ifdef __linux
#include <sha.h>
#define SHA1_CTX SHA_CTX
#define SHA1Init SHA1_Init
#define SHA1Update SHA1_Update
#define SHA1Final SHA1_Final
#else				/* __linux */
#include <sha1.h>
#endif
#else
#pragma pack(2)
#include <Common.h>
#include <System/SysAll.h>
#include <System/Unix/sys_types.h>
#include <System/Unix/unix_stdio.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/unix_string.h>
#include <UI/UIAll.h>
#include "getopt.h"
#include "field.h"
#define NO_SHA
#endif
#include <sectok.h>

#include "sc.h"

#ifdef __sun
#define des_set_key(key, schedule) des_key_sched(key, schedule)
#endif

#define MAX_KEY_FILE_SIZE 1024
#define NUM_RSA_KEY_ELEMENTS 5
#define RSA_BIT_LEN 1024
#define KEY_FILE_HEADER_SIZE 8

#define myisprint(x) ((x) >= '!' && (x) <= 'z')

static u_char key_fid[] = {0x00, 0x11};
static u_char DFLTATR[] = {0x81, 0x10, 0x06, 0x01};
static u_char DFLTAUT0[] = {0xad, 0x9f, 0x61, 0xfe, 0xfa, 0x20, 0xce, 0x63};
static u_char AUT0[20];

int	aut0_vfyd;

static void print_acl(int isdir, u_char *acl);

#ifndef __palmos__
/* default signed applet key of Cyberflex Access */
static des_cblock app_key = {0x6A, 0x21, 0x36, 0xF5, 0xD8, 0x0C, 0x47, 0x83};
#endif

static int
get_AUT0(int argc, char *argv[], char *prompt, int confirm, u_char *digest)
{
#ifdef NO_SHA
	memcpy(digest, DFLTAUT0, sizeof DFLTAUT0);
#else
	int	i, dflag = 0, xflag = 0;
	SHA1_CTX ctx;
	char   *s, *s2;

	optind = optreset = 1;
	opterr = 0;

	while ((i = getopt(argc, argv, "dk:x:")) != -1) {
		switch (i) {
		case 'd':
			memcpy(digest, DFLTAUT0, sizeof DFLTAUT0);
			dflag = 1;
			break;
		case 'x':
			if (sectok_parse_input(optarg, digest, 8) != 8) {
				printf("AUT0 must be length 8\n");
				return -1;
			}
			xflag = 1;
			break;
		}
	}

	if (!dflag && !xflag) {
		SHA1Init(&ctx);
		/* "-" means DFLTAUT0 */
		s = getpass(prompt);
		if (!strcmp(s, "-"))
			memcpy(digest, DFLTAUT0, sizeof DFLTAUT0);
		else {
			if (confirm) {
				s2 = strdup(s);
				s = getpass("Re-enter passphrase: ");
				if (strcmp(s, s2)) {
					printf("passphrase mismatch\n");
					return -1;
				}
				bzero(s2, strlen(s2));
				free(s2);
			}
			SHA1Update(&ctx, s, strlen(s));
			bzero(s, strlen(s));
			SHA1Final(digest, &ctx);
		}
	}
#endif

	return 0;
}

int
jlogin(int argc, char *argv[])
{
	int	i, keyno = 0, vflag = 0, sw;

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;

	cla = cyberflex_inq_class(fd);
	if (cla < 0) {
		printf("can't determine Cyberflex application class\n");
		return -1;
	}
	optind = optreset = 1;

	while ((i = getopt(argc, argv, "dk:vx:")) != -1) {
		switch (i) {
		case 'k':
			keyno = atoi(optarg);
			break;
		case 'v':
			vflag = 1;
			break;
		}
	}

	if (get_AUT0(argc, argv, "Enter AUT0 passphrase: ", 0, AUT0) < 0)
		return -1;

	if (vflag) {
		printf("Class %02x\n", cla);
		for (i = 0; i < 8; i++)
			printf("%02x ", AUT0[i]);
		printf("\n");
	}
	sectok_apdu(fd, cla, 0x2a, 0, keyno, 8, AUT0, 0, NULL, &sw);

	if (!sectok_swOK(sw)) {
		printf("AUT0 failed: %s\n", sectok_get_sw(sw));
		aut0_vfyd = 0;
		return -1;
	}
	aut0_vfyd = 1;
	return 0;
}

int
jaut(int argc, char *argv[])
{
	static char *jlav[] = {"login", "-d", NULL};

	return jlogin(2, jlav);
}

int
jdefault(int argc, char *argv[])
{
	u_char buf[8];
	int	i, p1 = 4, sw;

	optind = optreset = 1;

	while ((i = getopt(argc, argv, "d")) != -1) {
		switch (i) {
		case 'd':
			p1 = 5;
			break;
		}
	}

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;
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

int
jatr(int argc, char *argv[])
{
	u_char buf[64];
	int	n = 0, sw;

	buf[n++] = 0x90;
	buf[n++] = 0x94;	/* TA1 */
	buf[n++] = 0x40;	/* TD1 */
	buf[n++] = 0x28;	/* TC2 (WWT=4sec) */
	if (argc > 1) {
		/* set historical bytes from command line */
		n += sectok_parse_input(argv[1], &buf[n], 15);
	} else {
		/* no historical bytes given, use default */
		memcpy(&buf[n], DFLTATR, sizeof DFLTATR);
		n += sizeof DFLTATR;
	}
	buf[0] |= ((n - 2) & 0xf);

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;

	sectok_apdu(fd, cla, 0xfa, 0, 0, n, buf, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
		/* error */
		sectok_print_sw(sw);
		return -1;
	}
	return 0;
}

int
jdata(int argc, char *argv[])
{
	u_char buf[32];
	int	i, sw;

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;

	cla = cyberflex_inq_class(fd);
	if (cla < 0) {
		printf("can't determine Cyberflex application class\n");
		return -1;
	}
	sectok_apdu(fd, cla, 0xca, 0, 1, 0, NULL, 0x16, buf, &sw);
	if (sectok_swOK(sw)) {
		printf("serno ");
		for (i = 0; i < 6; i++)
			printf("%02x ", buf[i]);
		if (buf[20] == 0x13) {
			/* these cards have a different format */
			printf("scrambled sver %d.%02d ", buf[19], buf[20]);
			if (buf[21] == 0x0c)
				printf("augmented ");
			else
				if (buf[21] != 0x0b)
					printf("unknown ");
			printf("crypto %5.5s class %02x\n", &buf[14],
			    cyberflex_inq_class(fd));
		} else {
			printf("batch %02x sver %d.%02d ", buf[6], buf[7], buf[8]);
			if (buf[9] == 0x0c)
				printf("augmented ");
			else
				if (buf[9] != 0x0b)
					printf("unknown ");
			printf("crypto %9.9s class %02x\n", &buf[10], buf[19]);
		}
	} else {
		/* error */
		sectok_print_sw(sw);
	}
	return 0;
}
#define JDIRSIZE 40

static char *apptype[] = {
	"?",
	"applet",
	"app",
	"app/applet",
};

static char *appstat[] = {
	"?",
	"created",
	"installed",
	"registered",
};

static char *filestruct[] = {
	"binary",
	"fixed rec",
	"variable rec",
	"cyclic",
	"program",
};

static char *principals[] = {
	"world", "CHV1", "CHV2", "AUT0", "AUT1", "AUT2", "AUT3", "AUT4"
};

static char *f_rights[] = {
	"r", "w", "x/a", "inval", "rehab", NULL, "dec", "inc"
};

static char *d_rights[] = {
	"l", "d", "a", NULL, NULL, "i", "manage", NULL
};

static void
print_acl(int isdir, u_char *acl)
{
	int	i, j;
	char   *as;

	for (i = 0; i < 8; i++) {
		if (acl[i]) {
			printf(" %s: ", principals[i]);
			for (j = 0; j < 8; j++)
				if (acl[i] & (1 << j)) {
					as = isdir ? d_rights[j] : f_rights[j];
					if (as)
						printf("%s ", as);
				}
			printf("\n");
		}
	}
}

void
sectok_fmt_aidname(char *aidname, int aidlen, u_char *aid, size_t len)
{
	int	i, istext = 1;

	for (i = 1; i < aidlen; i++)
		if (!myisprint(aid[i])) {
			istext = 0;
			break;
		}
	if (istext) {
		if (aidlen + 1 > len)
			aidlen = len - 1;
		memcpy(aidname, aid, aidlen);
		aidname[aidlen] = '\0';
		if (aid[0] == 0xfc)
			aidname[0] = '#';
	} else {
		for (i = 0; i < aidlen; i++)
			snprintf(&aidname[i * 2], len - ( i * 2),
			    "%02x", aid[i]);
	}
}

int
ls(int argc, char *argv[])
{
	int	i, p2, fid, lflag = 0, buflen, sw;
	int	isdir, fsize;
	char    ftype[32], fname[6], aidname[34];
	u_char buf[JDIRSIZE];

	optind = optreset = 1;

	while ((i = getopt(argc, argv, "l")) != -1) {
		switch (i) {
		case 'l':
			lflag = 1;
			break;
		}
	}

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;

	for (p2 = 0;; p2++) {
		buflen = sectok_apdu(fd, cla, 0xa8, 0, p2, 0, NULL,
		    JDIRSIZE, buf, &sw);
		if (!sectok_swOK(sw))
			break;

		/* Don't show reserved fids */
		fid = sectok_mksw(buf[4], buf[5]);
		if (fid == 0x3f11 || fid == 0x3fff || fid == 0xffff)
			continue;

		/* Format name */
		sectok_fmt_fid(fname, sizeof fname, &buf[4]);

		/* Format size */
		fsize = (buf[2] << 8) | buf[3];

		/* Format file type */
		isdir = 0;
		aidname[0] = '\0';
		if (buf[6] == 1) {
			/* root */
			snprintf(ftype, sizeof ftype, "root");
			isdir = 1;
		} else
			if (buf[6] == 2) {
				/* DF */
				if (buf[12] == 27) {
					/* application */
					snprintf(ftype, sizeof ftype, "%s %s",
					    appstat[buf[10]], apptype[buf[9]]);
					if (buflen > 23 && buf[23]) {
						aidname[0] = ' ';
						sectok_fmt_aidname(&aidname[1],
						    buf[23], &buf[24],
						    sizeof aidname - 1);
					}
				} else
					snprintf(ftype, sizeof ftype,
					    "directory");
				isdir = 1;
			} else
				if (buf[6] == 4) {
					/* EF */
					snprintf(ftype, sizeof ftype, "%s",
					    filestruct[buf[13]]);
				}
		if (!lflag)
			printf("%-4s\n", fname);
		else
			printf("%-4s %5d %s%s\n", fname, fsize, ftype, aidname);
	}
	return 0;
}

int
acl(int argc, char *argv[])
{
	int	i, j, xflag = 0, isdir, prno, rt, sw;
	u_char fid[2], buf[256], acl[8];
	char   *prin;

	optind = optreset = 1;

	while ((i = getopt(argc, argv, "x")) != -1) {
		switch (i) {
		case 'x':
			xflag = 1;
			break;
		}
	}

	if (argc - optind < 1) {
 usage:
		printf("usage: acl [-x] fid [principal: r1 r2 ...]\n");
		return -1;
	}
	/* Select the fid */
	sectok_parse_fname(argv[optind++], fid);
	sectok_apdu(fd, cla, 0xa4, 0, 0, 2, fid, sizeof buf, buf, &sw);
	if (!sectok_swOK(sw)) {
		printf("Select: %s\n", sectok_get_sw(sw));
		return -1;
	}
	isdir = (buf[6] == 1 || buf[6] == 2);

	/* Get current acl */
	sectok_apdu(fd, cla, 0xfe, 0, 0, 0, NULL, 8, acl, &sw);
	if (!sectok_swOK(sw)) {
		printf("GetFileACL: %s\n", sectok_get_sw(sw));
		return -1;
	}
	if (argc - optind < 1) {
		/* No acl given; print acl and exit */
		if (xflag) {
			for (i = 0; i < 8; i++)
				printf("%02x ", acl[i]);
			printf("\n");
		} else
			print_acl(isdir, acl);
		return 0;
	}
	prin = argv[optind++];

	/* strip trailing ':' */
	if (prin[0] != '\0' && prin[strlen(prin) - 1] == ':')
		prin[strlen(prin) - 1] = '\0';
	else
		goto usage;

	/* Find principal */
	for (prno = 0; prno < 8; prno++)
		if (!strcasecmp(prin, principals[prno]))
			break;
	if (prno >= 8) {
		printf("unknown principal \"%s\"\n", prin);
		return -1;
	}
	/* Parse new rights */
	rt = 0;
	for (i = optind; i < optind + 8 && i < argc; i++) {
		for (j = 0; j < 8; j++) {
			if ((d_rights[j] && !strcasecmp(argv[i], d_rights[j])) ||
			    (f_rights[j] && !strcasecmp(argv[i], f_rights[j])))
				rt |= (1 << j);
		}
	}
	acl[prno] = rt;

	/* Set acl */
	sectok_apdu(fd, cla, 0xfc, 0, 0, 8, acl, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
		printf("ChangeFileACL: %s\n", sectok_get_sw(sw));
		return -1;
	}
	print_acl(isdir, acl);

	return 0;
}

int
jcreate(int argc, char *argv[])
{
	u_char fid[2];
	int	sw, fsize;

	if (argc != 3) {
		printf("usage: create fid size\n");
		return -1;
	}
	sectok_parse_fname(argv[1], fid);
	sscanf(argv[2], "%d", &fsize);

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;
	if (!aut0_vfyd)
		jaut(0, NULL);

	if (cyberflex_create_file(fd, cla, fid, fsize, 3, &sw) < 0) {
		printf("create_file: %s\n", sectok_get_sw(sw));
		return -1;
	}
	return 0;
}

int
jdelete(int argc, char *argv[])
{
	u_char fid[2];
	int	sw;

	if (argc != 2) {
		printf("usage: delete fid\n");
		return -1;
	}
	sectok_parse_fname(argv[1], fid);

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;
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

u_char progID[2], contID[2];

#ifndef __palmos__
int
jload(int argc, char *argv[])
{
	char   *cp, *filename, progname[5], contname[5];
	u_char aid[16], app_data[MAX_APP_SIZE], data[MAX_BUF_SIZE];
	int	i, j, vflag = 0, gotprog = 0, gotcont = 0, fd_app, size;
	int	aidlen = 0, sw;
	int	cont_size = 1152, inst_size = 1024;
	des_cblock tmp;
	des_key_schedule schedule;
	static u_char acl[] = {0x81, 0, 0, 0xff, 0, 0, 0, 0};

	optind = optreset = 1;

	while ((i = getopt(argc, argv, "p:c:s:i:a:v")) != -1) {
		switch (i) {
		case 'p':
			sectok_parse_input(optarg, progID, 2);
			gotprog = 1;
			break;
		case 'c':
			sectok_parse_input(optarg, contID, 2);
			gotcont = 1;
			break;
		case 's':
			sscanf(optarg, "%d", &cont_size);
			break;
		case 'i':
			sscanf(optarg, "%d", &inst_size);
			break;
		case 'a':
			aidlen = sectok_parse_input(optarg, aid, sizeof aid);
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			printf("unknown option.  command aborted.\n");
			return -1;
		}
	}

	if (argc - optind < 1) {
		printf("missing file name\n");
		return -1;
	}
	filename = argv[optind++];

	/*
         * We prepend 0xfc to the aid to make it a "proprietary aid".
         * See 7816-5 sec 5.2.4.
         */
	if (aidlen <= 0) {
		/* No aid given, derive from file name */
		cp = strrchr(filename, '/');
		if (cp)
			cp++;
		else
			cp = filename;
		aid[0] = 0xfc;
		strncpy(&aid[1], cp, sizeof aid - 1);
		aidlen = (aid[15] == '\0') ? strlen(aid) : 16;
	} else
		if (aid[0] == '#')
			aid[0] = 0xfc;

	if (!gotprog) {
		/* No progID given, derive from aid */
		progID[0] = aid[1];
		progID[1] = 'p';
	}
	if (!gotcont) {
		/* No contID given, derive from aid */
		contID[0] = aid[1];
		contID[1] = 'c';
	}
	if (fd < 0 && reset(0, NULL) < 0)
		return -1;
	if (!aut0_vfyd)
		jaut(0, NULL);

	sectok_fmt_fid(progname, sizeof progname, progID);
	sectok_fmt_fid(contname, sizeof contname, contID);

	if (vflag) {
		printf("applet file             \"%s\"\n", filename);
		printf("program ID              %s\n", progname);
		printf("container ID            %s\n", contname);
		printf("instance container size %d\n", cont_size);
		printf("instance data size      %d\n", inst_size);
		printf("AID                     ");
		for (i = 0; i < aidlen; i++)
			printf("%02x ", aid[i]);
		printf("\n");
	}
	/* open the input file */
	fd_app = open(filename, O_RDONLY, NULL);
	if (fd_app == -1) {
		fprintf(stderr, "cannot open file \"%s\"\n", filename);
		return -1;
	}
	/* read the input file */
	size = read(fd_app, app_data, MAX_APP_SIZE);
	if (size <= 0) {
		fprintf(stderr, "error reading file %s\n", filename);
		return -1;
	}
	/* size must be able to be divided by BLOCK_SIZE */
	if (size % BLOCK_SIZE != 0) {
		fprintf(stderr, "file \"%s\" size %d not divisible by %d\n", filename, size, BLOCK_SIZE);
		return -1;
	}
	/* compute the signature of the applet */
	/* initialize the result buffer */
	memset(tmp, 0, BLOCK_SIZE);

	/* chain.  DES encrypt one block, XOR the cyphertext with the next
	 * block, ... continues until the end of the buffer */

	des_set_key(&app_key, schedule);

	for (i = 0; i < size / BLOCK_SIZE; i++) {
		for (j = 0; j < BLOCK_SIZE; j++)
			tmp[j] = tmp[j] ^ app_data[i * BLOCK_SIZE + j];
		des_ecb_encrypt(&tmp, &tmp, schedule, DES_ENCRYPT);
	}

	if (vflag) {
		/* print out the signature */
		printf("signature ");
		for (j = 0; j < BLOCK_SIZE; j++)
			printf("%02x ", tmp[j]);
		printf("\n");
	}
	/* select the default loader */
	sectok_apdu(fd, cla, 0xa4, 0x04, 0, 0, NULL, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
		/* error */
		printf("can't select default loader: %s\n", sectok_get_sw(sw));
		return -1;
	}
	/* select 3f.00 (root) */
	if (sectok_selectfile(fd, cla, root_fid, &sw) < 0)
		return -1;

	/* create program file */
	if (cyberflex_create_file_acl(fd, cla, progID, size, 3, acl, &sw) < 0) {
		/* error */
		printf("can't create %s: %s\n", progname, sectok_get_sw(sw));
		return -1;
	}
	/* update binary */
	for (i = 0; i < size; i += MAX_APDU_SIZE) {
		int	send_size;

		/* compute the size to be sent */
		if (size - i > MAX_APDU_SIZE)
			send_size = MAX_APDU_SIZE;
		else
			send_size = size - i;

		sectok_apdu(fd, cla, 0xd6, i / 256, i % 256, send_size,
		    app_data + i, 0, NULL, &sw);

		if (!sectok_swOK(sw)) {
			/* error */
			printf("updating binary %s: %s\n", progname,
			    sectok_get_sw(sw));
			return -1;
		}
	}

	/* manage program .. validate */
	sectok_apdu(fd, cla, 0x0a, 01, 0, 0x08, tmp, 0, NULL, &sw);

	if (!sectok_swOK(sw)) {
		/* error */
		printf("validating applet in %s: %s\n", progname,
		    sectok_get_sw(sw));
		return -1;
	}
	/* select the default loader */
	sectok_apdu(fd, cla, 0xa4, 0x04, 0, 0, NULL, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
		/* error */
		printf("selecting default loader: %s\n", sectok_get_sw(sw));
		return -1;
	}
	/* execute method -- call the install() method in the cardlet. cardlet
	 * type 01 (applet, not application) */

	data[0] = 0x01;		/* cardlet type = 1 (applet, not application) */
	data[1] = progID[0];	/* FID, upper */
	data[2] = progID[1];	/* FID, lower */
	data[3] = cont_size >> 8;	/* instance container size 0x0800
					 * (1152) byte, upper */
	data[4] = cont_size & 0xff;	/* instance container size 0x0800
					 * (1152) byte, lower */
	data[5] = contID[0];	/* container ID (7778), upper */
	data[6] = contID[1];	/* container ID (7778), lower */
	data[7] = inst_size >> 8;	/* instance size 0x0400 (1024) byte,
					 * upper */
	data[8] = inst_size & 0xff;	/* instance size 0x0400 (1024) byte,
					 * lower */
	data[9] = 0x00;		/* AID length 0x0005, upper */
	data[10] = aidlen;	/* AID length 0x0005, lower */
	memcpy(&data[11], aid, aidlen);

	sectok_apdu(fd, cla, 0x0c, 0x13, 0, 11 + aidlen, data, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
		/* error */
		printf("executing install() method in applet %s: %s\n",
		    progname, sectok_get_sw(sw));
		return -1;
	}
	/* That's it! :) */
	return 0;
}
#endif

int
junload(int argc, char *argv[])
{
	char    progname[5], contname[5];
	int	i, vflag = 0, gotprog = 0, gotcont = 0, sw;

	optind = optreset = 1;

	while ((i = getopt(argc, argv, "p:c:v")) != -1) {
		switch (i) {
		case 'p':
			sectok_parse_input(optarg, progID, 2);
			gotprog = 1;
			break;
		case 'c':
			sectok_parse_input(optarg, contID, 2);
			gotcont = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			printf("unknown option.  command aborted.\n");
			return -1;
		}
	}

	if (argc - optind >= 1) {
		/* Derive progID and contID from filename */
		if (!gotprog) {
			progID[0] = argv[optind][0];
			progID[1] = 'p';
			gotprog = 1;
		}
		if (!gotcont) {
			contID[0] = argv[optind][0];
			contID[1] = 'c';
			gotcont = 1;
		}
	}
	/* Use old defaults */
	if (!gotprog)
		memcpy(progID, "ww", 2);
	if (!gotcont)
		memcpy(contID, "wx", 2);

	if (fd < 0 && reset(0, NULL) < 0)
		return -1;
	if (!aut0_vfyd)
		jaut(0, NULL);

	sectok_fmt_fid(progname, sizeof progname, progID);
	sectok_fmt_fid(contname, sizeof contname, contID);

	if (vflag) {
		printf("program ID              %s\n", progname);
		printf("container ID            %s\n", contname);
	}
	/* select 3f.00 (root) */
	if (sectok_selectfile(fd, cla, root_fid, &sw) < 0) {
		printf("can't select root: %s\n", sectok_get_sw(sw));
		return -1;
	}
	/* select program file */
	if (sectok_selectfile(fd, cla, progID, &sw) >= 0) {

		/* manage program -- reset */
		sectok_apdu(fd, cla, 0x0a, 02, 0, 0, NULL, 0, NULL, &sw);
		if (!sectok_swOK(sw)) {
			/* error */
			printf("resetting applet: %s\n", sectok_get_sw(sw));
		}
		/* delete program file */
		if (cyberflex_delete_file(fd, cla, progID, &sw) < 0)
			printf("delete_file %s: %s\n", progname, sectok_get_sw(sw));
	} else
		if (vflag)
			printf("no program file... proceed to delete data container\n");

	/* delete data container */
	if (cyberflex_delete_file(fd, cla, contID, &sw) < 0)
		printf("delete_file %s: %s\n", contname, sectok_get_sw(sw));

	return 0;
}

#ifndef __palmos__
#define DELIMITER " :\t\n"
#define KEY_BLOCK_SIZE 14

/* download DES keys into 3f.00/00.11 */
int
cyberflex_load_key(int fd, u_char *buf)
{
	int	sw, argc = 0, i, j, tmp;
	u_char *token;
	u_char data[MAX_BUF_SIZE];
	u_char key[BLOCK_SIZE];

#if 0
	/* select the default loader */
	rv = scwrite(fd, cla, 0xa4, 0x04, 0, 0x00, NULL, &r1, &r2);
	if (r1 != 0x90 && r1 != 0x61) {
		//error
		    printf("selecting the default loader: ");
		print_r1r2(r1, r2);
		return -1;
	}
#endif

	printf("ca_load_key buf=%s\n", buf);
	token = strtok(buf, DELIMITER);
	token = strtok(NULL, DELIMITER);
	if (token == NULL) {
		printf("usage: jk number_of_keys\n");
		return -1;
	}
	argc = atoi(token);

	if (argc > 2) {
		printf("current Cyberflex Access cannot download more than 2 keys to the key file.  Sorry. :(\n");
		return -1;
	}
	if (argc < 0) {
		printf("you want to down load %d keys??\n", argc);
		return -1;
	}
	if (!aut0_vfyd)
		jaut(0, NULL);

	/* Now let's do it. :) */

	/* add the AUT0 */
	cyberflex_fill_key_block(data, 0, 1, AUT0);

	/* add the applet sign key */
	cyberflex_fill_key_block(data + KEY_BLOCK_SIZE, 5, 0, app_key);

	/* then add user defined keys */
	for (i = 0; i < argc; i++) {
		printf("key %d : ", i);
		for (j = 0; j < BLOCK_SIZE; j++) {
			fscanf(cmdf, "%02x", &tmp);
			key[j] = (u_char) tmp;
		}

		cyberflex_fill_key_block(data + 28 + i * KEY_BLOCK_SIZE,
		    6 + i, 0, key);
	}

	/* add the suffix */
	data[28 + argc * KEY_BLOCK_SIZE] = 0;
	data[28 + argc * KEY_BLOCK_SIZE + 1] = 0;

	for (i = 0; i < KEY_BLOCK_SIZE * (argc + 2) + 2; i++)
		printf("%02x ", data[i]);
	printf("\n");

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
	sectok_apdu(fd, cla, 0xd6, 0, 0, KEY_BLOCK_SIZE * (argc + 2) + 2,
	    data, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
		/* error */
		printf("writing the key file 00.11: %s\n", sectok_get_sw(sw));
		return -1;
	}
	return 0;
}

/* download AUT0 key into 3f.00/00.11 */
int
jsetpass(int argc, char *argv[])
{
	int	sw;
	u_char data[MAX_BUF_SIZE];
	u_char AUT0[20];

	if (!aut0_vfyd && jaut(0, NULL) < 0)
		return -1;

	if (get_AUT0(argc, argv, "Enter new AUT0 passphrase: ", 1, AUT0) < 0)
		return -1;

	cyberflex_fill_key_block(data, 0, 1, AUT0);

#if 0
	/* add the suffix */
	data[KEY_BLOCK_SIZE] = 0;
	data[KEY_BLOCK_SIZE + 1] = 0;
#endif

#ifdef DEBUG
	for (i = 0; i < KEY_BLOCK_SIZE; i++)
		printf("%02x ", data[i]);
	printf("\n");
#endif

	/* select 3f.00 (root) */
	if (sectok_selectfile(fd, cla, root_fid, &sw) < 0)
		return -1;

	/* select 00.11 (key file) */
	if (sectok_selectfile(fd, cla, key_fid, &sw) < 0)
		return -1;

	/* all righty, now let's send it to the card! :) */
	sectok_apdu(fd, cla, 0xd6, 0, 0, KEY_BLOCK_SIZE, data, 0, NULL, &sw);
	if (!sectok_swOK(sw)) {
		/* error */
		printf("writing the key file 00.11: %s\n", sectok_get_sw(sw));
		return -1;
	}
	return 0;
}

/* download RSA private key into 3f.00/00.12 */
int
cyberflex_load_rsa(int fd, u_char *buf)
{
	int	sw, i, j, tmp;
	static u_char key_fid[] = {0x00, 0x12};
	static char *key_names[NUM_RSA_KEY_ELEMENTS] = {
		"p", "q", "1/p mod q", "d mod (p-1)", "d mod (q-1)"
	};
	u_char *key_elements[NUM_RSA_KEY_ELEMENTS];

	printf("ca_load_rsa_priv buf=%s\n", buf);

	printf("input 1024 bit RSA CRT key\n");
	for (i = 0; i < NUM_RSA_KEY_ELEMENTS; i++) {
		printf("%s (%d bit == %d byte) : ", key_names[i],
		    RSA_BIT_LEN / 2, RSA_BIT_LEN / 2 / 8);
		key_elements[i] = (u_char *) malloc(RSA_BIT_LEN / 8);
		for (j = 0; j < RSA_BIT_LEN / 8 / 2; j++) {
			fscanf(cmdf, "%02x", &tmp);
			key_elements[i][j] = (u_char) tmp;
		}
	}

#ifdef DEBUG
	printf("print RSA CRT key\n");
	for (i = 0; i < NUM_RSA_KEY_ELEMENTS; i++) {
		printf("%s : ", key_names[i]);
		for (j = 0; j < RSA_BIT_LEN / 8 / 2; j++) {
			printf("%02x ", key_elements[i][j]);
		}
	}
#endif

	if (!aut0_vfyd)
		jaut(0, NULL);

	cyberflex_load_rsa_priv(fd, cla, key_fid, NUM_RSA_KEY_ELEMENTS, RSA_BIT_LEN,
	    key_elements, &sw);

	if (!sectok_swOK(sw))
		printf("load_rsa_priv: %s\n", sectok_get_sw(sw));

	for (i = 0; i < NUM_RSA_KEY_ELEMENTS; i++)
		free(key_elements[i]);
	return 0;
}
#endif
