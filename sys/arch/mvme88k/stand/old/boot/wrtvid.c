#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "vid.h"

#define	sec2blk(x)	((x) * 2)

main(int argc, char **argv)
{
	struct vid *pvid;
	struct cfg *pcfg;
	struct stat stat;
	int exe_file;
	int tape_vid;
	int tape_exe;
	unsigned int exe_addr;
	unsigned short exe_addr_u;
	unsigned short exe_addr_l;
	char *filename;
	char fileext[256];

	if (argc == 0){
		filename = "a.out";
	} else {
		filename = argv[1];
	}
	exe_file = open(filename, O_RDONLY,0444);
	if (exe_file == -1)
	{
		printf("file %s does not exist\n",filename);
		exit(2);
	}
	sprintf (fileext,"%s%s",filename,".1");
	tape_vid = open(fileext, O_WRONLY|O_CREAT|O_TRUNC,0644);
	sprintf (fileext,"%s%s",filename,".2");
	tape_exe = open(fileext, O_WRONLY|O_CREAT|O_TRUNC,0644);

	pvid = (struct vid *) malloc(sizeof (struct vid));

	memset(pvid,0,sizeof(struct vid));

	strcpy(pvid->vid_id, "NBSD");

	fstat (exe_file,&stat);
	/* size in 512 byte blocks round up after a.out header removed */
	/* Actually, blocks == 256 bytes */

	pvid->vid_oss = 1;
	pvid->vid_osl = (short)sec2blk((stat.st_size - 0x20 + 511) / 512);

	lseek(exe_file,0x14,SEEK_SET);
	read(exe_file,&exe_addr,4);
	{
		union {
			struct {
				short osa_u;
				short osa_l;
			} osa_u_l; 
			int	osa;
		} u;
		u.osa = exe_addr;
		pvid->vid_osa_u = u.osa_u_l.osa_u;
		pvid->vid_osa_l = u.osa_u_l.osa_l;
	}
	pvid->vid_cas = 1;
	pvid->vid_cal = 1;
	/* do not want to write past end of structure, not null terminated */
	strcpy(pvid->vid_mot,"MOTOROL");
	pvid->vid_mot[7] = 'A';

	write(tape_vid,pvid,sizeof(struct vid));

	free(pvid);

	pcfg = (struct cfg *) malloc (sizeof(struct cfg));

	memset(pcfg,0,sizeof(struct cfg));

	pcfg->cfg_rec = 0x100;
	pcfg->cfg_psm = 0x200;

	write(tape_vid,pcfg,sizeof(struct cfg));

	free(pcfg);

	copy_exe(exe_file,tape_exe);
	close (exe_file);
	close (tape_vid);
	close (tape_exe);
}

#define BUF_SIZ 512
copy_exe(exe_file,tape_exe)
{
	char *buf;
	int cnt = 0;

	buf = (char *)malloc (BUF_SIZ);

	lseek (exe_file,0x20,SEEK_SET);
	while (BUF_SIZ == (cnt = read(exe_file, buf , BUF_SIZ))) {
		write (tape_exe,buf,cnt);
	}
	memset (&buf[cnt],0,BUF_SIZ-cnt);
	write (tape_exe,buf,BUF_SIZ);
}
