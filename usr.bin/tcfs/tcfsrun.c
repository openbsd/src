/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <ctype.h>
#include <des.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"

char *cmd_def="/bin/sh";
char *run_usage = "usage: tcfsrun [-p mount-point | -f fs-label] [cmd] [args...]";

int
run_main(int argc, char *argv[], char *envp[])
{
	char *key, *cmd, x;
	char fspath[MAXPATHLEN], cmdname[MAXPATHLEN];
	uid_t uid;
	pid_t pid;
	int es;
	int havefspath = 0,havecmd = 0;

	uid = getuid();

	while ((x = getopt(argc,argv,"p:f:")) != EOF) {
		switch(x) {
		case 'p':
			strlcpy(fspath, optarg, sizeof(fspath));
			havefspath = 1;
			break;
		case 'f':
			es = tcfs_getfspath(optarg,fspath);
			if (!es) {
				fprintf(stderr, 
					"filesystem label not found!\n");
				exit(1);
			}
			havefspath=1;
			break;
		}
	}

	if (argc - optind) {
		strlcpy(cmdname, argv[optind], sizeof(cmdname));
		havecmd = 1;
		cmd = cmdname;
	}

	if (!havefspath) {
		es = tcfs_getfspath("default",fspath);
		if (!es)
			exit(1);
	}

	if (!havecmd)
		cmd = cmd_def;

	key = getpass("tcfs key:");

	pid = fork();
	if (!pid) {
		pid = getpid();
		if (tcfs_proc_enable(fspath, uid, pid, key) != -1) {
			setuid(uid);
			execve(cmd,argv + optind, envp);
		}

		fprintf(stderr, "Operation failed\n");
		exit(1);
	}
	
	wait(0);

	if (tcfs_proc_disable(fspath,uid,pid) == -1) {
		fprintf (stderr, "Problems removing process key\n");
		exit(1);
	}
	exit(0);
}


