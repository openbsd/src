/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 *
 *	  $Source: /home/cvs/src/usr.bin/tcfs/Attic/tcfstatold.c,v $
 *	   $State: Exp $
 *	$Revision: 1.1.1.1 $
 *	  $Author: provos $
 *	    $Date: 2000/06/18 22:07:24 $
 *
 */

static const char *RCSid="$id: $";

/* RCS_HEADER_ENDS_HERE */



#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <des.h>
#include "tcfs.h"

void main(int argc, char *argv[], char *envp[])
{
	struct tcfs_status st;	
	int e;

	if(argc <2)
		{
			fprintf(stderr,"usage: tcfstat <filesystem>\n");
			exit(1);
		}
	
	e=tcfs_getstatus(argv[1],&st);
	if(e==-1)
		{
			fprintf(stderr,"filesystem %s not mounted\n",argv[0]);
			exit(1);
		}
	
	printf("Status: %d; user keys: %d, group keys: %d\n",st.status, st.n_ukey, st.n_gkey);
	printf("TCFS version: %d, Cipher: %s, keysize: %d, cipher version: %d\n",st.tcfs_version, st.cipher_desc, st.cipher_keysize, st.cipher_version);
			
	
}


