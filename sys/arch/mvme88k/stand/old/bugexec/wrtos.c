#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "vid.h"

#define	sec2blk(x)	((x) * 2)
#define BUF_SIZ 512

main(int argc, char **argv)
{
	struct vid *pvid;
	struct cfg *pcfg;
	struct stat stat;
	int exe_file;
	int tape_vid;
	int tape_exe;
	char *filename;
	char fileext[256];
	char hdrbuf[BUF_SIZ];

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

	lseek(exe_file,0,SEEK_SET);
	memset (hdrbuf,0,BUF_SIZ);
	read(exe_file,hdrbuf, 0x20);	/* read the header */

	write(tape_vid,hdrbuf,BUF_SIZ);

	copy_exe(exe_file,tape_exe);
	close (exe_file);
	close (tape_vid);
	close (tape_exe);
}

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
