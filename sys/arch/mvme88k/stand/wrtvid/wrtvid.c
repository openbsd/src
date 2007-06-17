/*	$OpenBSD: wrtvid.c,v 1.7 2007/06/17 00:28:57 deraadt Exp $ */

/*
 * Copyright (c) 1995 Dale Rahn <drahn@openbsd.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void	copy_exe(int, int);
void	swabcfg(struct mvmedisklabel *);
void	swabvid(struct mvmedisklabel *);

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct mvmedisklabel *pcpul;
	struct stat stat;
	int exe_file;
	int tape_vid;
	int tape_exe;
	unsigned int exe_addr;
	char *filename;
	char fileext[256];

	if (argc == 1)
		filename = "a.out";
	else
		filename = argv[1];

	exe_file = open(filename, O_RDONLY,0444);
	if (exe_file == -1) {
		perror(filename);
		exit(2);
	}
	snprintf(fileext, sizeof fileext, "%c%cboot", filename[4], filename[5]);
	tape_vid = open(fileext, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	snprintf(fileext, sizeof fileext, "boot%c%c", filename[4], filename[5]);
	tape_exe = open(fileext, O_WRONLY|O_CREAT|O_TRUNC,0644);

	pcpul = (struct mvmedisklabel *)malloc(sizeof(struct mvmedisklabel));
	bzero(pcpul, sizeof(struct mvmedisklabel));

	pcpul->version = 1;
	memcpy(pcpul->vid_id, "M88K", sizeof pcpul->vid_id);

	fstat(exe_file, &stat);
	/* size in 256 byte blocks round up after a.out header removed */

	if (filename[5] == 't' ) {
		pcpul->vid_oss = 1;
	}else {
		pcpul->vid_oss = 2;
	}
	pcpul->vid_osl = (((stat.st_size -0x20) +511) / 512) *2;

	lseek(exe_file, 0x14, SEEK_SET);
	read(exe_file, &exe_addr, 4);

	/* check this, it may not work in both endian. */
	/* No, it doesn't.  Use a big endian machine for now. SPM */
	
	{
		union {
			struct s {
				unsigned short s1;
				unsigned short s2;
			} s;
			unsigned long l;
		} a;
		a.l = exe_addr;
		pcpul->vid_osa_u = a.s.s1;
		pcpul->vid_osa_l = a.s.s2;

	}
	pcpul->vid_cas = 1;
	pcpul->vid_cal = 1;
	/* do not want to write past end of structure, not null terminated */
	strncpy(pcpul->vid_mot, "MOTOROLA", 8);

	if (BYTE_ORDER != BIG_ENDIAN)
		swabvid(pcpul);

	pcpul->cfg_rec = 0x100;
	pcpul->cfg_psm = 0x200;

	if (BYTE_ORDER != BIG_ENDIAN)
		swabcfg(pcpul);

	write(tape_vid, pcpul, sizeof(struct mvmedisklabel));

	free(pcpul);

	copy_exe(exe_file, tape_exe);
	close(exe_file);
	close(tape_vid);
	close(tape_exe);
	return (0);
}

#define BUF_SIZ 512
void
copy_exe(exe_file, tape_exe)
	int exe_file, tape_exe;
{
	char *buf;
	int cnt = 0;

	buf = (char *)malloc(BUF_SIZ);

	lseek (exe_file, 0x20, SEEK_SET);
	while (BUF_SIZ == (cnt = read(exe_file, buf, BUF_SIZ))) {
		write(tape_exe, buf, cnt);
	}
	bzero(&buf[cnt], BUF_SIZ-cnt);
	write(tape_exe, buf, BUF_SIZ);
}

void
swabvid(pcpul)
	struct mvmedisklabel *pcpul;
{
	swap32(pcpul->vid_oss);
	swap16(pcpul->vid_osl);
#if 0
	swap16(pcpul->vid_osa_u);
	swap16(pcpul->vid_osa_l);
#endif
	swap32(pcpul->vid_cas);
}

void
swabcfg(pcpul)
	struct mvmedisklabel *pcpul;
{
	swap16(pcpul->cfg_atm);
	swap16(pcpul->cfg_prm);
	swap16(pcpul->cfg_atm);
	swap16(pcpul->cfg_rec);
	swap16(pcpul->cfg_trk);
	swap16(pcpul->cfg_psm);
	swap16(pcpul->cfg_shd);
	swap16(pcpul->cfg_pcom);
	swap16(pcpul->cfg_rwcc);
	swap16(pcpul->cfg_ecc);
	swap16(pcpul->cfg_eatm);
	swap16(pcpul->cfg_eprm);
	swap16(pcpul->cfg_eatw);
	swap16(pcpul->cfg_rsvc1);
	swap16(pcpul->cfg_rsvc2);
}
