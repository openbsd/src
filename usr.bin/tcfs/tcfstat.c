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
#include <des.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"

char *stat_usage= "usage: tcfstat [-p mount-point | fs-label]";

int
stat_main(int argc, char *argv[], char *envp[])
{
	struct tcfs_status st;	
	int e, es, ok=0;
	char filesystem[MAXPATHLEN];
	
	if (argc == 3 && !strcmp("-p", argv[1])) {
		strlcpy(filesystem, argv[2], sizeof(filesystem));
		ok = 1;
	}

	if (argc == 2) {
		if (!(es = tcfs_getfspath(argv[1], filesystem))) {
			fprintf(stderr, "filesystem label not found!\n");
			exit(1);
		}
		ok = 1;
	}

	if (ok == 0 || argc < 2 || argc > 3) {
                fprintf(stderr, "%s\n", stat_usage);
                exit(1);
        }

	
	e = tcfs_getstatus(filesystem, &st);
	if (e == -1) {
		fprintf(stderr, "filesystem %s not mounted\n", filesystem);
		exit(1);
	}
	
	printf("Status: %d; user keys: %d, group keys: %d\n", st.status, st.n_ukey, st.n_gkey);
	printf("TCFS version: %d, Cipher: %s, keysize: %d, cipher version: %d\n", st.tcfs_version, st.cipher_desc, st.cipher_keysize, st.cipher_version);
			
	exit(0);
}

