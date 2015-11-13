/*	$OpenBSD: cmd.c,v 1.85 2015/11/13 02:27:17 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <uuid.h>

#include "disk.h"
#include "misc.h"
#include "part.h"
#include "mbr.h"
#include "gpt.h"
#include "user.h"
#include "cmd.h"

int reinited;

/* Some helper functions for GPT handling. */
int Xgedit(char *);
int Xgsetpid(char *);

int
Xreinit(char *args, struct mbr *mbr)
{
	struct dos_mbr dos_mbr;
	int dogpt;

	if (strncasecmp(args, "gpt", 3) == 0)
		dogpt = 1;
	else if (strncasecmp(args, "mbr", 3) == 0)
		dogpt = 0;
	else if (strlen(args) > 0) {
		printf("Unrecognized modifier '%s'\n", args);
		return (CMD_CONT);
	} else if (MBR_protective_mbr(&initial_mbr) == 0)
		dogpt = 1;
	else
		dogpt = 0;

	MBR_make(&initial_mbr, &dos_mbr);
	MBR_parse(&dos_mbr, mbr->offset, mbr->reloffset, mbr);

	if (dogpt) {
		MBR_init_GPT(mbr);
		GPT_init();
		GPT_print("s");
	} else {
		MBR_init(mbr);
		MBR_print(mbr, "s");
	}
	reinited = 1;

	printf("Use 'write' to update disk.\n");

	return (CMD_DIRTY);
}

int
Xdisk(char *args, struct mbr *mbr)
{
	int maxcyl  = 1024;
	int maxhead = 256;
	int maxsec  = 63;

	/* Print out disk info */
	DISK_printgeometry(args);

#if defined (__powerpc__) || defined (__mips__)
	maxcyl  = 9999999;
	maxhead = 9999999;
	maxsec  = 9999999;
#endif

	/* Ask for new info */
	if (ask_yn("Change disk geometry?")) {
		disk.cylinders = ask_num("BIOS Cylinders",
		    disk.cylinders, 1, maxcyl);
		disk.heads = ask_num("BIOS Heads",
		    disk.heads, 1, maxhead);
		disk.sectors = ask_num("BIOS Sectors",
		    disk.sectors, 1, maxsec);

		disk.size = disk.cylinders * disk.heads * disk.sectors;
	}

	return (CMD_CONT);
}

int
Xswap(char *args, struct mbr *mbr)
{
	const char *errstr;
	char *from, *to;
	int pf, pt, maxpn;
	struct prt pp;
	struct gpt_partition gg;

	to = args;
	from = strsep(&to, " \t");

	if (to == NULL) {
		printf("partition number is invalid:\n");
		return (CMD_CONT);
	}

	if (letoh64(gh.gh_sig) == GPTSIGNATURE)
		maxpn = NGPTPARTITIONS - 1;
	else
		maxpn = NDOSPART - 1;

	pf = strtonum(from, 0, maxpn, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, from);
		return (CMD_CONT);
	}
	pt = strtonum(to, 0, maxpn, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, to);
		return (CMD_CONT);
	}

	if (pt == pf) {
		printf("%d same partition as %d, doing nothing.\n", pt, pf);
		return (CMD_CONT);
	}

	if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
		gg = gp[pt];
		gp[pt] = gp[pf];
		gp[pf] = gg;
	} else {
		pp = mbr->part[pt];
		mbr->part[pt] = mbr->part[pf];
		mbr->part[pf] = pp;
	}

	return (CMD_DIRTY);
}

int
Xgedit(char *args)
{
	const char *errstr;
	struct gpt_partition *gg;
	char *name;
	u_int64_t bs, ns;
	int pn, ret;

	pn = strtonum(args, 0, NGPTPARTITIONS - 1, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}
	gg = &gp[pn];

	/* Edit partition type */
	ret = Xgsetpid(args);

	/* Unused, so just zero out */
	if (uuid_is_nil(&gg->gp_type, NULL)) {
		memset(gg, 0, sizeof(struct gpt_partition));
		printf("Partition %d is disabled.\n", pn);
		return (ret);
	}

	/* Change table entry */
	bs = getuint64("Partition offset", letoh64(gh.gh_lba_start),
	    letoh64(gh.gh_lba_end));
	ns = getuint64("Partition size", letoh64(gh.gh_lba_end) - bs + 1,
	    letoh64(gh.gh_lba_end) - bs + 1);

	gg->gp_lba_start = htole64(bs);
	gg->gp_lba_end = htole64(bs + ns - 1);

	/* Ask for partition name. */
	name = ask_string("partition name", utf16le_to_string(gg->gp_name));
	if (strlen(name) >= GPTPARTNAMESIZE) {
		printf("partition name must be < %d characters\n",
		    GPTPARTNAMESIZE);
		return (CMD_CONT);
	}
	memset(gg->gp_name, 0, sizeof(gg->gp_name));
	memcpy(gg->gp_name, string_to_utf16le(name), sizeof(gg->gp_name));

	return (ret);
}

int
Xedit(char *args, struct mbr *mbr)
{
	const char *errstr;
	int pn, num, ret;
	struct prt *pp;

	if (letoh64(gh.gh_sig) == GPTSIGNATURE)
		return (Xgedit(args));

	pn = strtonum(args, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}
	pp = &mbr->part[pn];

	/* Edit partition type */
	ret = Xsetpid(args, mbr);

	/* Unused, so just zero out */
	if (pp->id == DOSPTYP_UNUSED) {
		memset(pp, 0, sizeof(*pp));
		printf("Partition %d is disabled.\n", pn);
		return (ret);
	}

	/* Change table entry */
	if (ask_yn("Do you wish to edit in CHS mode?")) {
		int maxcyl, maxhead, maxsect;

		/* Shorter */
		maxcyl = disk.cylinders - 1;
		maxhead = disk.heads - 1;
		maxsect = disk.sectors;

		/* Get data */
#define	EDIT(p, v, n, m)			\
	if ((num = ask_num(p, v, n, m)) != v)	\
		ret = CMD_DIRTY;		\
	v = num;
		EDIT("BIOS Starting cylinder", pp->scyl,  0, maxcyl);
		EDIT("BIOS Starting head",     pp->shead, 0, maxhead);
		EDIT("BIOS Starting sector",   pp->ssect, 1, maxsect);
		EDIT("BIOS Ending cylinder",   pp->ecyl,  0, maxcyl);
		EDIT("BIOS Ending head",       pp->ehead, 0, maxhead);
		EDIT("BIOS Ending sector",     pp->esect, 1, maxsect);
#undef EDIT
		/* Fix up off/size values */
		PRT_fix_BN(pp, pn);
		/* Fix up CHS values for LBA */
		PRT_fix_CHS(pp);
	} else {
		pp->bs = getuint64("Partition offset", pp->bs,
		    (u_int64_t)disk.size);
		pp->ns = getuint64("Partition size", pp->ns,
		    (u_int64_t)(disk.size - pp->bs));
		/* Fix up CHS values */
		PRT_fix_CHS(pp);
	}

	return (ret);
}

int
Xgsetpid(char *args)
{
	const char *errstr;
	struct uuid guid;
	struct gpt_partition *gg;
	int pn, num, status;

	pn = strtonum(args, 0, NGPTPARTITIONS - 1, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}
	gg = &gp[pn];

	/* Print out current table entry */
	GPT_print_parthdr();
	GPT_print_part(pn, "s");

	/* Ask for partition type or GUID. */
	num = ask_pid(0, &guid);
	if (num <= 0xff)
		guid = *(PRT_type_to_uuid(num));
	uuid_enc_le(&gg->gp_type, &guid);

	if (uuid_is_nil(&gg->gp_guid, NULL)) {
		uuid_create(&guid, &status);
		if (status != uuid_s_ok) {
			printf("could not create guid for partition\n");
			return (CMD_CONT);
		}
		uuid_enc_le(&gg->gp_guid, &guid);
	}

	return (CMD_DIRTY);
}

int
Xsetpid(char *args, struct mbr *mbr)
{
	const char *errstr;
	int pn, num;
	struct prt *pp;

	if (letoh64(gh.gh_sig) == GPTSIGNATURE)
		return (Xgsetpid(args));

	pn = strtonum(args, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}
	pp = &mbr->part[pn];

	/* Print out current table entry */
	PRT_print(0, NULL, NULL);
	PRT_print(pn, pp, NULL);

	/* Ask for MBR partition type */
	num = ask_pid(pp->id, NULL);
	if (num == pp->id)
		return (CMD_CONT);

	pp->id = num;

	return (CMD_DIRTY);
}

int
Xselect(char *args, struct mbr *mbr)
{
	const char *errstr;
	static off_t firstoff = 0;
	off_t off;
	int pn;

	pn = strtonum(args, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}

	off = mbr->part[pn].bs;

	/* Sanity checks */
	if ((mbr->part[pn].id != DOSPTYP_EXTEND) &&
	    (mbr->part[pn].id != DOSPTYP_EXTENDL)) {
		printf("Partition %d is not an extended partition.\n", pn);
		return (CMD_CONT);
	}

	if (firstoff == 0)
		firstoff = off;

	if (!off) {
		printf("Loop to offset 0!  Not selected.\n");
		return (CMD_CONT);
	} else {
		printf("Selected extended partition %d\n", pn);
		printf("New MBR at offset %lld.\n", (long long)off);
	}

	/* Recursion is beautiful! */
	USER_edit(off, firstoff);

	return (CMD_CONT);
}

int
Xprint(char *args, struct mbr *mbr)
{

	if (MBR_protective_mbr(mbr) == 0 && letoh64(gh.gh_sig) == GPTSIGNATURE)
		GPT_print(args);
	else
		MBR_print(mbr, args);

	return (CMD_CONT);
}

int
Xwrite(char *args, struct mbr *mbr)
{
	struct dos_mbr dos_mbr;
	int i, n;

	for (i = 0, n = 0; i < NDOSPART; i++)
		if (mbr->part[i].id == 0xA6)
			n++;
	if (n >= 2) {
		warnx("MBR contains more than one OpenBSD partition!");
		if (!ask_yn("Write MBR anyway?"))
			return (CMD_CONT);
	}

	MBR_make(mbr, &dos_mbr);

	printf("Writing MBR at offset %lld.\n", (long long)mbr->offset);
	if (MBR_write(mbr->offset, &dos_mbr) == -1) {
		warn("error writing MBR");
		return (CMD_CONT);
	}

	if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
		printf("Writing GPT.\n");
		if (GPT_write() == -1) {
			warn("error writing GPT");
			return (CMD_CONT);
		}
	} else if (reinited) {
		/* Make sure GPT doesn't get in the way. */
		MBR_zapgpt(&dos_mbr, DL_GETDSIZE(&dl) - 1);
	}

	/* Refresh in memory copy to reflect what was just written. */
	MBR_parse(&dos_mbr, mbr->offset, mbr->reloffset, mbr);

	return (CMD_CLEAN);
}

int
Xquit(char *args, struct mbr *mbr)
{
	return (CMD_SAVE);
}

int
Xabort(char *args, struct mbr *mbr)
{
	exit(0);
}

int
Xexit(char *args, struct mbr *mbr)
{
	return (CMD_EXIT);
}

int
Xhelp(char *args, struct mbr *mbr)
{
	char *mbrstr, *gpthelp;
	int i;

	for (i = 0; cmd_table[i].cmd != NULL; i++) {
		if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
			if (cmd_table[i].gpt == 0)
				continue;
			gpthelp = strdup(cmd_table[i].help);
			mbrstr = strstr(gpthelp, "MBR");
			if (mbrstr) {
				memcpy(mbrstr, "GPT", 3);
				printf("\t%s\t\t%s\n", cmd_table[i].cmd,
				    gpthelp);
				free(gpthelp);
				continue;
			}
		}
		printf("\t%s\t\t%s\n", cmd_table[i].cmd, cmd_table[i].help);
	}

	return (CMD_CONT);
}

int
Xupdate(char *args, struct mbr *mbr)
{
	/* Update code */
	memcpy(mbr->code, initial_mbr.code, sizeof(mbr->code));
	mbr->signature = DOSMBR_SIGNATURE;
	printf("Machine code updated.\n");
	return (CMD_DIRTY);
}

int
Xflag(char *args, struct mbr *mbr)
{
	const char *errstr;
	int i, maxpn, pn = -1;
	long long val = -1;
	char *part, *flag;

	flag = args;
	part = strsep(&flag, " \t");

	if (letoh64(gh.gh_sig) == GPTSIGNATURE)
		maxpn = NGPTPARTITIONS - 1;
	else
		maxpn = NDOSPART - 1;

	pn = strtonum(part, 0, maxpn, &errstr);
	if (errstr) {
		printf("partition number is %s: %s.\n", errstr, part);
		return (CMD_CONT);
	}

	if (flag != NULL) {
		/* Set flag to value provided. */
		if (letoh64(gh.gh_sig) == GPTSIGNATURE)
			val = strtonum(flag, 0, LLONG_MAX, &errstr);
		else
			val = strtonum(flag, 0, 0xff, &errstr);
		if (errstr) {
			printf("flag value is %s: %s.\n", errstr, flag);
			return (CMD_CONT);
		}
		if (letoh64(gh.gh_sig) == GPTSIGNATURE)
			gp[pn].gp_attrs = htole64(val);
		else
			mbr->part[pn].flag = val;
		printf("Partition %d flag value set to 0x%llx.\n", pn, val);
	} else {
		/* Set active flag */
		if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
			for (i = 0; i < NGPTPARTITIONS; i++) {
				if (i == pn)
					gp[i].gp_attrs = htole64(GPTDOSACTIVE);
				else
					gp[i].gp_attrs = htole64(0);
			}
		} else {
			for (i = 0; i < NDOSPART; i++) {
				if (i == pn)
					mbr->part[i].flag = DOSACTIVE;
				else
					mbr->part[i].flag = 0x00;
			}
		}
		printf("Partition %d marked active.\n", pn);
	}

	return (CMD_DIRTY);
}

int
Xmanual(char *args, struct mbr *mbr)
{
	char *pager = "/usr/bin/less";
	char *p;
	sig_t opipe;
	extern const unsigned char manpage[];
	extern const int manpage_sz;
	FILE *f;

	opipe = signal(SIGPIPE, SIG_IGN);
	if ((p = getenv("PAGER")) != NULL && (*p != '\0'))
		pager = p;
	if (asprintf(&p, "gunzip -qc|%s", pager) != -1) {
		f = popen(p, "w");
		if (f) {
			fwrite(manpage, manpage_sz, 1, f);
			pclose(f);
		}
		free(p);
	}

	signal(SIGPIPE, opipe);

	return (CMD_CONT);
}
