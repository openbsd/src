/*	$OpenBSD: wrtvid.c,v 1.3 1998/08/22 08:52:56 smurph Exp $ */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#define __DBINTERFACE_PRIVATE
#include <db.h>
#include "disklabel.h" 	
/* disklabel.h is in current dir because of my 
   cross-compile env.  if <machine/disklabel.h>
   is newer, copy it here.
*/

main(argc, argv)
	int argc;
	char **argv;
{
	struct cpu_disklabel *pcpul;
	struct stat stat;
	int exe_file;
	int tape_vid;
	int tape_exe;
	unsigned int exe_addr;
	unsigned short exe_addr_u;
	unsigned short exe_addr_l;
	char *filename;
	char fileext[256];
	char filebase[256];

	if (argc == 1)
		filename = "a.out";
	else
		filename = argv[1];

	exe_file = open(filename, O_RDONLY,0444);
	if (exe_file == -1) {
		perror(filename);
		exit(2);
	}
	sprintf(fileext, "%c%cboot", filename[4], filename[5]);
	tape_vid = open(fileext, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	sprintf(fileext, "boot%c%c", filename[4], filename[5]);
	tape_exe = open(fileext, O_WRONLY|O_CREAT|O_TRUNC,0644);

	pcpul = (struct cpu_disklabel *)malloc(sizeof(struct cpu_disklabel));
	bzero(pcpul, sizeof(struct cpu_disklabel));

	pcpul->version = 1;
	strcpy(pcpul->vid_id, "M88K");

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

	write(tape_vid, pcpul, sizeof(struct cpu_disklabel));

	free(pcpul);

	copy_exe(exe_file, tape_exe);
	close(exe_file);
	close(tape_vid);
	close(tape_exe);
	return (0);
}

#define BUF_SIZ 512
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

swabvid(pcpul)
	struct cpu_disklabel *pcpul;
{
	M_32_SWAP(pcpul->vid_oss);
	M_16_SWAP(pcpul->vid_osl);
	/*
	M_16_SWAP(pcpul->vid_osa_u);
	M_16_SWAP(pcpul->vid_osa_l);
	*/
	M_32_SWAP(pcpul->vid_cas);
}

swabcfg(pcpul)
	struct cpu_disklabel *pcpul;
{
	M_16_SWAP(pcpul->cfg_atm);
	M_16_SWAP(pcpul->cfg_prm);
	M_16_SWAP(pcpul->cfg_atm);
	M_16_SWAP(pcpul->cfg_rec);
	M_16_SWAP(pcpul->cfg_trk);
	M_16_SWAP(pcpul->cfg_psm);
	M_16_SWAP(pcpul->cfg_shd);
	M_16_SWAP(pcpul->cfg_pcom);
	M_16_SWAP(pcpul->cfg_rwcc);
	M_16_SWAP(pcpul->cfg_ecc);
	M_16_SWAP(pcpul->cfg_eatm);
	M_16_SWAP(pcpul->cfg_eprm);
	M_16_SWAP(pcpul->cfg_eatw);
	M_16_SWAP(pcpul->cfg_rsvc1);
	M_16_SWAP(pcpul->cfg_rsvc2);
}
