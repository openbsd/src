/*	$NetBSD: sys_inst.c,v 1.3 1995/10/04 07:24:33 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.
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
 *	by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Portions of this program are inspired by (and have borrowed code from)
 * the `editlabel' program that accompanies NetBSD/vax, which carries
 * the following notice:
 *
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of
 *	Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#define DKTYPENAMES

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <a.out.h>
#include "stand.h"
#include "samachdep.h"

char line[100];

extern	u_int opendev;
extern	char *lowram;
extern	int noconsole;
extern	int cons_scode;

char	*kernel_name = "/bsd";

void	dsklabel __P((void));
void	miniroot __P((void));
void	bootmini __P((void));
void	resetsys __P((void));
void	gethelp __P((void));
int	opendisk __P((char *, char *, int, char, int *));
void	disklabel_edit __P((struct disklabel *));
void	disklabel_show __P((struct disklabel *));
int	disklabel_write __P((char *, int, struct open_file *));

struct	inst_command {
	char	*ic_cmd;		/* command name */
	char	*ic_desc;		/* command description */
	void	(*ic_func) __P((void));	/* handling function */
} inst_commands[] = {
	{ "disklabel",	"place partition map on disk",	dsklabel },
	{ "miniroot",	"place miniroot on disk",	miniroot },
	{ "boot",	"boot from miniroot",		bootmini },
	{ "reset",	"reset the system",		resetsys },
	{ "help",	"display command list",		gethelp },
};
#define NCMDS	(sizeof(inst_commands) / sizeof(inst_commands[0]))

main()
{
	int i, currname = 0;

	printf("\n>> NetBSD INSTALLATION HP9000/%s CPU\n",
	       getmachineid());
	printf(">> $NetBSD: sys_inst.c,v 1.3 1995/10/04 07:24:33 thorpej Exp $\n");
	gethelp();

	for (;;) {
		printf("sys_inst> ");
		bzero(line, sizeof(line));
		gets(line);
		if (line[0] == '\n' || line[0] == '\0')
			continue;

		for (i = 0; i < NCMDS; ++i)
			if (strcmp(line, inst_commands[i].ic_cmd) == 0) {
				(*inst_commands[i].ic_func)();
				break;
			}


		if (i == NCMDS)
			printf("unknown command: %s\n", line);
	}
}

void
gethelp()
{
	int i;

	printf(">> Available commands:\n");
	for (i = 0; i < NCMDS; ++i)
		printf(">>     %s - %s\n", inst_commands[i].ic_cmd,
		    inst_commands[i].ic_desc);
}

/*
 * Do all the steps necessary to place a disklabel on a disk.
 * Note, this assumes 512 byte sectors.
 */
void
dsklabel()
{
	struct disklabel *lp;
	struct open_file *disk_ofp;
	int dfd, error;
	size_t xfersize;
	char block[DEV_BSIZE], diskname[64];
	extern struct open_file files[];

	printf("
You will be asked several questions about your disk, most of which
require prior knowledge of the disk's geometry.  There is no easy way
for the system to provide this information for you.  If you do not have
this information, please consult your disk's manual or another
informative source.\n\n");

	/* Error message printed by opendisk() */
	if (opendisk("Disk to label?", diskname, sizeof(diskname),
	    ('a' + RAW_PART), &dfd))
		return;

	disk_ofp = &files[dfd];

	bzero(block, sizeof(block));
	if (error = (*disk_ofp->f_dev->dv_strategy)(disk_ofp->f_devdata,
	    F_READ, LABELSECTOR, sizeof(block), block, &xfersize)) {
		printf("cannot read disk %s, errno = %d\n", diskname, error);
		return;
	}

	printf("Sucessfully read %d bytes from %s\n", xfersize, diskname);

	lp = (struct disklabel *)((void *)(&block[LABELOFFSET]));

 disklabel_loop:
	bzero(line, sizeof(line));
	printf("(z)ap, (e)dit, (s)how, (w)rite, (d)one > ");
	gets(line);
	if (line[0] == '\n' || line[0] == '\0')
		goto disklabel_loop;

	switch (line[0]) {
	case 'z':
	case 'Z': {
		char zap[DEV_BSIZE];
		bzero(zap, sizeof(zap));
		(void)(*disk_ofp->f_dev->dv_strategy)(disk_ofp->f_devdata,
		    F_WRITE, LABELSECTOR, sizeof(zap), zap, &xfersize);
		}
		goto out;
		/* NOTREACHED */

	case 'e':
	case 'E':
		disklabel_edit(lp);
		break;

	case 's':
	case 'S':
		disklabel_show(lp);
		break;

	case 'w':
	case 'W':
		/*
		 * Error message will be displayed by disklabel_write()
		 */
		if (disklabel_write(block, sizeof(block), disk_ofp))
			goto out;
		else
			printf("Sucessfully wrote label to %s\n", diskname);
		break;

	case 'd':
	case 'D':
		goto out;
		/* NOTREACHED */

	default:
		printf("unkown command: %s\n", line);
	}

	goto disklabel_loop;
	/* NOTREACHED */

 out:
	/*
	 * Close disk.  Marks disk `not alive' so that partition
	 * information will be reloaded upon next open.
	 */
	(void)close(dfd);
}

#define GETNUM(out, num)						\
	printf((out), (num));						\
	bzero(line, sizeof(line));					\
	gets(line);							\
	if (line[0])							\
		(num) = atoi(line);

#define GETNUM2(out, num1, num2)					\
	printf((out), (num1), (num2));					\
	bzero(line, sizeof(line));					\
	gets(line);							\
	if (line[0])							\
		(num2) = atoi(line);

#define GETSTR(out, str)						\
	printf((out), (str));						\
	bzero(line, sizeof(line));					\
	gets(line);							\
	if (line[0])							\
		strcpy((str), line);

#define FLAGS(out, flag)						\
	printf((out), lp->d_flags & (flag) ? 'y' : 'n');		\
	bzero(line, sizeof(line));					\
	gets(line);							\
	if (line[0] == 'y' || line[0] == 'Y')				\
		lp->d_flags |= (flag);					\
	else								\
		lp->d_flags &= ~(flag);

void
disklabel_edit(lp)
	struct disklabel *lp;
{
	int i;

	printf("Select disk type.  Valid types:\n");
	for (i = 0; i < DKMAXTYPES; i++)
		printf("%d     %s\n", i, dktypenames[i]);
	printf("\n");

	GETNUM("Disk type (number)? [%d] ", lp->d_type);
	GETSTR("Disk model name? [%s] ", lp->d_typename);
	GETSTR("Disk pack name? [%s] ", lp->d_packname);
	FLAGS("Bad sectoring? [%c] ", D_BADSECT);
	FLAGS("Ecc? [%c] ", D_ECC);
	FLAGS("Removable? [%c] ", D_REMOVABLE);

	printf("\n");

	GETNUM("Interleave? [%d] ", lp->d_interleave);
	GETNUM("Rpm? [%d] ", lp->d_rpm);
	GETNUM("Trackskew? [%d] ", lp->d_trackskew);
	GETNUM("Cylinderskew? [%d] ", lp->d_cylskew);
	GETNUM("Headswitch? [%d] ", lp->d_headswitch);
	GETNUM("Track-to-track? [%d] ", lp->d_trkseek);
	GETNUM("Drivedata 0? [%d] ", lp->d_drivedata[0]);
	GETNUM("Drivedata 1? [%d] ", lp->d_drivedata[1]);
	GETNUM("Drivedata 2? [%d] ", lp->d_drivedata[2]);
	GETNUM("Drivedata 3? [%d] ", lp->d_drivedata[3]);
	GETNUM("Drivedata 4? [%d] ", lp->d_drivedata[4]);

	printf("\n");

	GETNUM("Bytes/sector? [%d] ", lp->d_secsize);
	GETNUM("Sectors/track? [%d] ", lp->d_nsectors);
	GETNUM("Tracks/cylinder? [%d] ", lp->d_ntracks);
	GETNUM("Sectors/cylinder? [%d] ", lp->d_secpercyl);
	GETNUM("Cylinders? [%d] ", lp->d_ncylinders);

	printf("
Enter partition table.  Note, sizes and offsets are in sectors.\n\n");

	lp->d_npartitions = MAXPARTITIONS;
	for (i = 0; i < lp->d_npartitions; ++i) {
		GETNUM2("%c partition: offset? [%d] ", ('a' + i),
		    lp->d_partitions[i].p_offset);
		GETNUM("             size? [%d] ", lp->d_partitions[i].p_size);
	}

	/* Perform magic. */
	lp->d_magic = lp->d_magic2 = DISKMAGIC;

	/* Calculate disklabel checksum. */
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
}

void
disklabel_show(lp)
	struct disklabel *lp;
{
	int i, npart;
	struct partition *pp;

	/*
	 * Check for valid disklabel.
	 */
	if (lp->d_magic != DISKMAGIC || lp->d_magic2 != DISKMAGIC) {
		printf("No disklabel to show.\n");
		return;
	}

	if (lp->d_npartitions > MAXPARTITIONS || dkcksum(lp) != 0) {
		printf("Corrupted disklabel.\n");
		return;
	}

	printf("\ndisk type %d (%s), %s: %s%s%s\n", lp->d_type,
	    lp->d_type < DKMAXTYPES ? dktypenames[lp->d_type] :
	    dktypenames[0], lp->d_typename,
	    (lp->d_flags & D_REMOVABLE) ? " removable" : "",
	    (lp->d_flags & D_ECC) ? " ecc" : "",
	    (lp->d_flags & D_BADSECT) ? " badsect" : "");

	printf("interleave %d, rpm %d, trackskew %d, cylinderskew %d\n",
	    lp->d_interleave, lp->d_rpm, lp->d_trackskew, lp->d_cylskew);

	printf("headswitch %d, track-to-track %d, drivedata: %d %d %d %d %d\n",
	    lp->d_headswitch, lp->d_trkseek, lp->d_drivedata[0],
	    lp->d_drivedata[1], lp->d_drivedata[2], lp->d_drivedata[3],
	    lp->d_drivedata[4]);

	printf("\nbytes/sector: %d\n", lp->d_secsize);
	printf("sectors/track: %d\n", lp->d_nsectors);
	printf("tracks/cylinder: %d\n", lp->d_ntracks);
	printf("sectors/cylinder: %d\n", lp->d_secpercyl);
	printf("cylinders: %d\n", lp->d_ncylinders);

	printf("\n%d partitions:\n", lp->d_npartitions);
	printf("     size   offset\n");
	pp = lp->d_partitions;
	for (i = 0; i < lp->d_npartitions; i++) {
		printf("%c:   %d,    %d\n", 97 + i, lp->d_partitions[i].p_size,
		    lp->d_partitions[i].p_offset);
	}
	printf("\n");
}

int
disklabel_write(block, len, ofp)
	char *block;
	int len;
	struct open_file *ofp;
{
	int error = 0;
	size_t xfersize;

	if (error = (*ofp->f_dev->dv_strategy)(ofp->f_devdata, F_WRITE,
	    LABELSECTOR, len, block, &xfersize))
		printf("cannot write disklabel, errno = %d\n", error);

	return (error);
}

int
opendisk(question, diskname, len, partition, fdp)
	char *question, *diskname;
	int len;
	char partition;
	int *fdp;
{
	char fulldiskname[64], *filename;
	int i, error = 0;

 getdiskname:
	printf("%s ", question);
	bzero(diskname, len);
	bzero(fulldiskname, sizeof(fulldiskname));
	gets(diskname);
	if (diskname[0] == '\n' || diskname[0] == '\0')
		goto getdiskname;

	/*
	 * devopen() is picky.  Make sure it gets the sort of string it
	 * wants.
	 */
	bcopy(diskname, fulldiskname,
	    len < sizeof(fulldiskname) ? len : sizeof(fulldiskname));
	for (i = 0; fulldiskname[i + 1] != '\0'; ++i)
		/* Nothing. */ ;
	if (fulldiskname[i] < '0' || fulldiskname[i] > '9') {
		printf("invalid disk name %s\n", diskname);
		goto getdiskname;
	}
	fulldiskname[++i] = partition; fulldiskname[++i] = ':';

	/*
	 * We always open for writing.
	 */
	if ((*fdp = open(fulldiskname, 1)) < 0)
		printf("cannot open %s\n", diskname);

	return (errno);
}

/*
 * Copy a miniroot image from an NFS server or tape to the `b' partition
 * of the specified disk.  Note, this assumes 512 byte sectors.
 */
void
miniroot()
{
	int sfd, dfd, i, nblks;
	char diskname[64], minirootname[128];
	char block[DEV_BSIZE];
	struct stat st;
	size_t xfersize;
	struct open_file *disk_ofp;
	extern struct open_file files[];

	/* Error message printed by opendisk() */
	if (opendisk("Disk for miniroot?", diskname, sizeof(diskname),
	    'b', &dfd))
		return;

	disk_ofp = &files[dfd];

	/*
	 * XXX We only support NFS for now.
	 */
 getsource:
	printf("Source? (N)FS, (t)ape, (d)one > ");
	bzero(line, sizeof(line));
	gets(line);
	if (line[0] == '\0')
		goto getsource;

	switch (line[0]) {
	case 'n':
	case 'N':
 name_of_miniroot:
		printf("Name of miniroot file? ");
		bzero(line, sizeof(line));
		bzero(minirootname, sizeof(minirootname));
		gets(line);
		if (line[0] == '\0')
			goto name_of_miniroot;
		(void)strcat(minirootname, "le0a:");
		(void)strcat(minirootname, line);
		if ((sfd = open(minirootname, 0)) < 0) {
			printf("can't open %s\n", line);
			return;
		}
		break;

	case 't':
	case 'T':
		printf("Sorry, no tape support yet.\n");
		return;

	case 'd':
	case 'D':
		return;

	default:
		printf("Unknown source: %s\n", line);
		goto getsource;
	}

	/*
	 * Have file descriptor, will copy.
	 */
	if (fstat(sfd, &st) < 0) {
		printf("can't stat %s\n", line);
		goto done;
	}

	nblks = (int)(st.st_size / sizeof(block));
	printf("Copying %d blocks from %s to %s\n", nblks, line, diskname);

	for (i = 0; i < nblks; ++i) {
		if (read(sfd, block, sizeof(block)) != sizeof(block)) {
			printf("Short read, errno = %d\n", errno);
			goto done;
		}
		if ((*disk_ofp->f_dev->dv_strategy)(disk_ofp->f_devdata,
		    F_WRITE, i, sizeof(block), block, &xfersize) ||
		    xfersize != sizeof(block)) {
			printf("Bad write, errno = %d\n", errno);
			goto done;
		}
	}

	printf("Successfully copied miniroot image.\n");
 done:
	close(sfd);
	close(dfd);
}

/*
 * Boot the kernel from the miniroot image into single-user.
 */
void
bootmini()
{
	char diskname[64], bootname[64];
	int i;

 getdiskname:
	printf("Disk to boot from? ");
	bzero(diskname, sizeof(diskname));
	bzero(bootname, sizeof(bootname));
	gets(diskname);
	if (diskname[0] == '\n' || diskname[0] == '\0')
		goto getdiskname;

	/*
	 * devopen() is picky.  Make sure it gets the sort of string it
	 * wants.
	 */
	(void)strcat(bootname, diskname);
	for (i = 0; bootname[i + 1] != '\0'; ++i)
		/* Nothing. */ ;
	if (bootname[i] < '0' || bootname[i] > '9') {
		printf("invalid disk name %s\n", diskname);
		goto getdiskname;
	}
	bootname[++i] = 'b'; bootname[++i] = ':';
	(void)strcat(bootname, kernel_name);

	howto = RB_SINGLE;	/* _Always_ */

	printf("booting: %s -s\n", bootname);
	exec(bootname, lowram, howto);
	printf("boot: %s\n", strerror(errno));
}

/*
 * Reset the system.
 */
void
resetsys()
{

	call_req_reboot();
	printf("panic: can't reboot, halting\n");
	asm("stop #0x2700");
}

void
machdep_start(entry, howto, loadaddr, ssym, esym)
	char *entry;
	int howto;
	char *loadaddr;
	char *ssym, *esym;
{

	asm("movl %0,d7" : : "m" (howto));
	asm("movl %0,d6" : : "m" (opendev));
	asm("movl %0,d5" : : "m" (cons_scode));
	asm("movl %0,a5" : : "a" (loadaddr));
	asm("movl %0,a4" : : "a" (esym));
	(*((int (*)())entry))();
}
