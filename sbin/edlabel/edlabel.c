/*	$OpenBSD: edlabel.c,v 1.4 1996/07/27 10:28:42 deraadt Exp $	*/
/*	$NetBSD: edlabel.c,v 1.1.1.1 1995/10/08 22:39:09 gwr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#define DKTYPENAMES
#include <sys/disklabel.h>

#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <util.h>

/*
 * Machine dependend constants you want to retrieve only once...
 */
int rawpartition, maxpartitions;

/*
 * This is a data-driven program
 */
struct field {
	char *f_name;
	int f_offset;
	int f_type;	/* 1:char, 2:short, 4:int, >4:string */
};

/* Table describing fields in the head of a disklabel. */
#define	dloff(f) (int)(&((struct disklabel *)0)->f)
struct field label_head[] = {
  { "        type_num", dloff(d_type), 2 },
  { "        sub_type", dloff(d_subtype), 2 },
  { "       type_name", dloff(d_typename), 16 },
  { "       pack_name", dloff(d_packname),  16 },
  { "    bytes/sector", dloff(d_secsize), 4 },
  { "   sectors/track", dloff(d_nsectors), 4 },
  { " tracks/cylinder", dloff(d_ntracks),  4 },
  { "       cylinders", dloff(d_ncylinders), 4 },
  { "sectors/cylinder", dloff(d_secpercyl), 4 },
  /* Don't care about the others until later... */
  { 0 },
};
#undef dloff

char tmpbuf[64];

void
edit_head_field(v, f, modify)
	void *v;
	struct field *f;
	int modify;	/* also modify */
{
	u_int8_t  *cp;
	u_int tmp;

	cp = v;
	cp += f->f_offset;

	printf("%s: ", f->f_name);

	/* Print current value... */
	switch (f->f_type) {
	case 1:
		tmp = *cp;
		printf("%d", tmp);
		break;
	case 2:
		tmp = *((u_int16_t *)cp);
		printf("%d", tmp);
		break;
	case 4:
		tmp = *((u_int32_t *)cp);
		printf("%d", tmp);
		break;

	default:
		/* must be a string. */
		strncpy(tmpbuf, (char*)cp, sizeof(tmpbuf));
		printf("%s", tmpbuf);
		break;
	}

	if (modify == 0) {
		printf("\n");
		return;
	}
	printf(" ? ");
	fflush(stdout);

	tmpbuf[0] = '\0';
	if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
		return;
	if ((tmpbuf[0] == '\0') || (tmpbuf[0] == '\n')) {
		/* no new value supplied. */
		return;
	}

	/* store new value */
	if (f->f_type <= 4)
		if (sscanf(tmpbuf, "%d", &tmp) != 1)
			return;

	switch (f->f_type) {
	case 1:
		*cp = tmp;
		break;
	case 2:
		*((u_int16_t *)cp) = tmp;
		break;
	case 4:
		*((u_int32_t *)cp) = tmp;
		break;
	default:
		/* Get rid of the trailing newline. */
		tmp = strlen(tmpbuf);
		if (tmp < 1)
			break;
		if (tmpbuf[tmp-1] == '\n')
			tmpbuf[tmp-1] = '\0';
		strncpy((char*)cp, tmpbuf, f->f_type);
		break;
	}
}

void
edit_head_all(d, modify)
	struct disklabel *d;
	int modify;
{
	struct field *f;

	/* Edit head stuff. */
	for (f = label_head; f->f_name; f++)
		edit_head_field(d, f, modify);
}

void
edit_geo(d)
	struct disklabel *d;
{
	int nsect, ntrack, ncyl, spc;

	nsect = ntrack = ncyl = spc = 0;

	printf("Sectors/track: ");
	fflush(stdout);
	if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
		return;
	if (sscanf(tmpbuf, "%d", &nsect) != 1)
		nsect = d->d_nsectors;
	printf("Track/cyl: ");
	fflush(stdout);
	if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
		return;
	if (sscanf(tmpbuf, "%d", &ntrack) != 1)
		ntrack = d->d_ntracks;
	if (!nsect || !ntrack)
		return;
	spc = nsect * ntrack;
	if (!(ncyl = d->d_secperunit / spc))
		return;
	d->d_nsectors   = nsect;
	d->d_ntracks    = ntrack;
	d->d_ncylinders = ncyl;
	d->d_secpercyl  = spc;
}

void
print_val_cts(d, val)
	struct disklabel *d;
	u_long val;
{
	int	sects, trks, cyls;
	char	marker;
	char	buf[80];

	marker = (val % d->d_secpercyl) ? '*' : ' ';
	sects  = val % d->d_nsectors;
	cyls   = val / d->d_nsectors;
	trks   = cyls % d->d_ntracks;
	cyls  /= d->d_ntracks;
	sprintf(buf, "(%d/%02d/%02d)%c", cyls, trks, sects, marker);
	printf(" %9ld %16s", val, buf);
}

void
get_val_cts(d, buf, result)
	struct disklabel *d;
	char		 *buf;
	u_int32_t	 *result;
{
	u_long tmp;
	int	cyls, trks, sects;

	tmp = sscanf(buf, "%d/%d/%d", &cyls, &trks, &sects);
	if (tmp == 1)
		*result = cyls;	/* really nblks! */
	if (tmp == 3) {
		tmp = cyls;
		tmp *= d->d_ntracks;
		tmp += trks;
		tmp *= d->d_nsectors;
		tmp += sects;
		*result = tmp;
	}
}

void
get_fstype(tmpbuf, fstype)
	char	 *tmpbuf;
	u_int8_t *fstype;
{
	int	i, len;

	/* An empty response retains previous value */
	if (tmpbuf[0] == '\n')
		return;
	for (i = 0, len = strlen(tmpbuf) - 1; i < FSMAXTYPES; i++) {
		if (!strncasecmp(tmpbuf, fstypenames[i], len)) {
			*fstype = i;
			return;
		}
	}
}

void
edit_partition(d, idx, modify)
	struct disklabel *d;
	int idx;
{
	struct partition *p;
	char letter, *comment;

	if ((idx < 0) || (idx >= maxpartitions)) {
		printf("bad partition index\n");
		return;
	}
	
	p = &d->d_partitions[idx];
	letter = 'a' + idx;

	/* Set hint about partition type */
	if (idx == rawpartition)
		comment = "disk";
	else {
		comment = "user";
		switch(idx) {
			case 0:
				comment = "root";
			break;
			case 1:
				comment = "swap";
				break;
		}
	}

	/* Print current value... */
	printf(" %c (%s) ", letter, comment);
	print_val_cts(d, p->p_offset);
	print_val_cts(d, p->p_size);
	printf(" %s\n", fstypenames[p->p_fstype]);

	if (modify == 0)
		return;

	/* starting block, or cyls/trks/sects */
	printf("start as <blkno> or <cyls/trks/sects> : ");
	fflush(stdout);
	if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
		return;
	get_val_cts(d, tmpbuf, &p->p_offset);

	/* number of blocks, or cyls/trks/sects */
	printf("length as <nblks> or <cyls/trks/sects> : ");
	fflush(stdout);
	if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
		return;
	get_val_cts(d, tmpbuf, &p->p_size);
	/* partition type */
	printf("type: ");
	fflush(stdout);
	if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
		return;
	get_fstype(tmpbuf, &p->p_fstype);
}

/*****************************************************************/

void
check_divisors(d)
	struct disklabel *d;
{
	if (d->d_nsectors == 0) {
		d->d_nsectors = 1;
		printf("bad sect/trk, set to 1\n");
	}
	if (d->d_ntracks == 0) {
		d->d_ntracks = 1;
		printf("bad trks/cyl, set to 1\n");
	}
	if (d->d_ncylinders == 0) {
		d->d_ncylinders = 1;
		printf("bad cyls, set to 1\n");
	}
	if (d->d_secpercyl == 0) {
		d->d_secpercyl = (d->d_nsectors * d->d_ntracks);
		printf("bad sect/cyl, set to %d\n", d->d_secpercyl);
	}

}

u_short
dkcksum(d)
	struct disklabel *d;
{
	u_short *start, *end;
	u_short sum = 0;

	start = (u_short *)d;
	end = (u_short *)&d->d_partitions[d->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return (sum);
}

void
label_write(d, dn)
	struct disklabel *d;
	char *dn;
{
	int fd;

	d->d_magic = DISKMAGIC;
	d->d_magic2 = DISKMAGIC;
	d->d_checksum = 0;
	d->d_checksum = dkcksum(d);

	fd = open(dn, O_RDWR, 0);
	if (fd < 0) {
		perror(dn);
		return;
	}
	if (ioctl(fd, DIOCWDINFO, d) < 0) {
		perror("ioctl DIOCWDINFO");
	}
	close(fd);
}

void
label_read(dl, dn)
	struct disklabel *dl;
	char *dn;
{
	int fd;

	fd = open(dn, O_RDONLY, 0);
	if (fd < 0) {
		perror(dn);
		exit(1);
	}
	if (ioctl(fd, DIOCGDINFO, dl) < 0) {
		if (errno == ESRCH)
			fprintf(stderr, "edlabel: No disk label on disk\n");
		else perror("ioctl DIOCGDINFO");
		exit(1);
	}

	/* Make sure divisors are non-zero. */
	check_divisors(dl);

	close(fd);
}

/*****************************************************************/

void
label_print(dl, dn)
	struct disklabel *dl;
	char *dn;
{
	int i;

	/* Print out head stuff. */
	edit_head_all(dl, 0);

	/* And the partition header. */
	printf("partition%6sstart%9s(c/t/s)%6snblks%9s(c/t/s)  type\n\n"
							"", "", "", "", "");
	for (i = 0; i < dl->d_npartitions; i++)
		edit_partition(dl, i, 0);
}

char modify_cmds[] = "modify subcommands:\n\
 @   : modify disk parameters\n\
 a-%c : modify partition\n%s\
 q   : quit this subcommand\n";

void
label_modify(dl, dn)
	struct disklabel *dl;
	char *dn;
{
	int c, i;
	int scsi_fict = 0;

	if (!strcmp(dl->d_typename, "SCSI disk")
	     && !strcmp(dl->d_packname, "fictitious"))
		scsi_fict = 1;

	printf(modify_cmds, 'a' + maxpartitions,
		scsi_fict ? " s   : standarize geometry\n" : "");
	for (;;) {
		printf("edlabel/modify> ");
		fflush(stdout);
		if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
			break;
		c = tmpbuf[0];
		if ((c == '\0') || (c == '\n'))
			continue;	/* blank line */
		if (c == 'q')
			break;
		if (c == '@') {
			edit_head_all(dl, 1);
			check_divisors(dl);
			continue;
		}
		if ((c == 's') && scsi_fict) {
			edit_geo(dl);
			continue;
		}
		if ((c < 'a') || (c > 'q')) {
			printf("bad input.  ");
			printf(modify_cmds);
			continue;
		}
		edit_partition(dl, c - 'a', 1);
	}
	/* Set the d_npartitions field correctly */
	for (i = 0; i < maxpartitions; i++) {
		if (dl->d_partitions[i].p_size)
			dl->d_npartitions = i + 1;
	}

}

void
label_quit()
{
	exit(0);
}

struct cmd {
	void (*cmd_func)();
	char *cmd_name;
	char *cmd_descr;
} cmds[] = {
	{ label_print,  "print",  "display the current disk label" },
	{ label_modify, "modify", "prompt for changes to the label" },
	{ label_write,  "write",  "write the new label to disk" },
	{ label_quit,   "quit",   "terminate program" },
	{ 0 },
};

void
menu()
{
	struct cmd *cmd;

	printf("edlabel menu:\n");
	for (cmd = cmds; cmd->cmd_func; cmd++)
		printf("%s\t- %s\n", cmd->cmd_name, cmd->cmd_descr);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct disklabel dl;
	struct cmd *cmd;
	char *devname;

	if (argc != 2) {
		fprintf(stderr, "usage: edlabel RAWDISK\n");
		exit(1);
	}
	devname = argv[1];

	rawpartition = getrawpartition();
	maxpartitions = getmaxpartitions();

	label_read(&dl, devname);

	menu();

	for (;;) {
		printf("edlabel> ");
		fflush(stdout);
		if (fgets(tmpbuf, sizeof(tmpbuf), stdin) == NULL)
			break;
		for (cmd = cmds; cmd->cmd_func; cmd++)
			if (cmd->cmd_name[0] == tmpbuf[0])
				goto found;
		printf("Invalid command.  ");
		menu();
		continue;

	found:
		cmd->cmd_func(&dl, devname);
	}
	exit(0);
}

