/*	$OpenBSD: tcfsputkey.c,v 1.9 2000/06/20 01:29:14 provos Exp $	*/

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
#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfserrors.h"
#include <grp.h>

extern char *optarg;
extern int optind;
char *putkey_usage=
"usage: tcfsputkey [-k][-f fliesystem-label][-g group][-p mount-point]\n";

int
putkey_main(int argc, char *argv[])
{
	char *user, *password, *tcfskey;
	uid_t uid;
	gid_t gid;
	int es, treshold;
	char x;
	tcfspwdb *info;
	tcfsgpwdb *ginfo;
	char fslabel[MAXPATHLEN], fspath[MAXPATHLEN];
	int def = TRUE, havempname = FALSE, havefsname = FALSE;
	int isgroupkey = FALSE;
	int havename = FALSE, havefspath = FALSE, havekey = FALSE;

	while ((x = getopt(argc, argv, "kf:p:g:")) != EOF) {
		switch(x) {
		case 'k':
			def = FALSE;
			break;
		case 'p':
			havempname = TRUE;
			strlcpy(fspath, optarg, sizeof(fspath));
			break;
		case 'f':
			havefsname = TRUE;
			strlcpy(fslabel, optarg, sizeof(fslabel));
			break;
		case 'g':
			isgroupkey = TRUE;
			def = TRUE;
			gid = atoi(optarg);
			if (!gid && optarg[0] != 0) {
				struct group *grp;

				grp = (struct group *)getgrnam(optarg);
				if (!grp)
					tcfs_error(ER_CUSTOM,
					    "Nonexistant group\n");
				gid = grp->gr_gid;
			}
			break;
		default: 
			tcfs_error(ER_CUSTOM, putkey_usage);
			exit(ER_UNKOPT);
		}
	}
	if (argc - optind)
		tcfs_error(ER_UNKOPT, NULL);

	if (havefsname && havempname) {
		tcfs_error(ER_CUSTOM, putkey_usage);
		exit(1);
	}
			 
	if (havefsname) {
		es = tcfs_getfspath(fslabel, fspath);
		havename = TRUE;
	}

	if (havefspath)
		havename = TRUE;

	if (!havename)
		es = tcfs_getfspath("default", fspath);

	if (!es) {
		tcfs_error(ER_CUSTOM, "fs-label not found!\n");
		exit(1);
	}

	uid = getuid();
		
	if (isgroupkey) {
		if (!unix_auth(&user, &password, TRUE))
			tcfs_error(ER_AUTH, user);

		if (!tcfsgpwdbr_new(&ginfo))
			tcfs_error(ER_MEM, NULL);

		if (!tcfs_ggetpwnam(user, gid, &ginfo))
			tcfs_error(ER_CUSTOM, "Default key non found");

		if (!strlen(ginfo->gkey))
			tcfs_error(ER_CUSTOM, "Invalid default key");

		tcfskey = (char *)malloc(UUKEYSIZE);
		if (!tcfskey)
			tcfs_error(ER_MEM, NULL);	

		treshold = ginfo->soglia;

		if (!tcfs_decrypt_key(password, ginfo->gkey, tcfskey, GKEYSIZE))
			tcfs_error(ER_CUSTOM, "Could not decrypt group key");

		es = tcfs_group_enable(fspath, uid, gid, treshold, tcfskey);

		if (es == -1) {
			tcfs_error(ER_CUSTOM, "problems updating filesystem");
		}

		exit(0);
	}


	if (!def) {
		tcfskey = getpass("Insert tcfs-key:");
		havekey = TRUE;
	} else {
		if (!unix_auth(&user, &password, TRUE))
			tcfs_error(ER_AUTH, user);
				
		if (!tcfspwdbr_new(&info))
			tcfs_error(ER_MEM, NULL);	

		if (!tcfs_getpwnam(user, &info))
			tcfs_error(ER_CUSTOM, "Default key non found");
	
		if (!strlen(info->upw))
			tcfs_error(ER_CUSTOM, "Invalid default key");

		tcfskey = (char *)malloc(UUKEYSIZE);
		if (!tcfskey)
			tcfs_error(ER_MEM, NULL);	
		
		if (!tcfs_decrypt_key (password, info->upw, tcfskey, KEYSIZE))
			tcfs_error(ER_CUSTOM, "Could not decrypt key");
		havekey = TRUE;
	}

	es = tcfs_user_enable(fspath, uid, tcfskey);

	if (es == -1)
		tcfs_error(ER_CUSTOM, "problems updating filesystem");

	exit(0);
}
