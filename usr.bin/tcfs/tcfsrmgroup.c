/*	$OpenBSD: tcfsrmgroup.c,v 1.10 2000/06/20 18:15:57 aaron Exp $	*/

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

#include <grp.h>
#include <stdio.h>
#include <stdlib.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfserrors.h"

char *rmgroup_usage="Usage: %s [OPTION]...
Remove a TCFS group from the TCFS group database.

  -g <group>   Specifies the TCFS group to be removed
  -h           Shows this help
  -v           Makes the output a little more verbose\n";

int
rmgroup_main(int argn, char *argv[])
{
	int val;
	gid_t gid = 0;
	int have_gid = FALSE, be_verbose = FALSE;

	/*
	 * Going to check the arguments
	 */
	while ((val = getopt(argn, argv, "hg:v")) != -1)
		switch (val) {
		case 'g':
			gid = (gid_t)atoi(optarg);
			if (!gid && optarg[0] != '0') { /* group name given */ 
				struct group *group_id;

				group_id = getgrnam(optarg);
				if (!group_id)
					tcfs_error(ER_CUSTOM, "Nonexistent group.");
				gid = group_id->gr_gid;
			}

			have_gid = TRUE;
			break;
		case 'h':
			printf(rmgroup_usage, argv[0]);
			exit(OK);
		case 'v':
			be_verbose = TRUE;
			break;
		default:
			fprintf(stderr,
			    "Try %s --help for more informations.\n", argv[0]);
			exit(ER_UNKOPT);
		}

	if (argn - optind)
		tcfs_error(ER_UNKOPT, NULL);

	if (!have_gid) {
		char *buff = NULL;
		int len;

		buff = (char *)malloc(2048);
		if (!buff)
			tcfs_error(ER_MEM, NULL);

		printf("Group ID of the TCFS group to remove from the database: ");
		fgets(buff, 2048, stdin);
		len = strlen(buff) - 1;
		buff[len] = buff[len] == '\n' ? 0 : buff[len];
		gid = (gid_t)atoi(buff);

		if (!gid && optarg[0] != '0') { /* group name given */
			struct group *group_id;

			group_id = getgrnam(optarg);
			if (!group_id)
				tcfs_error(ER_CUSTOM, "Nonexistent group.");
			gid = group_id->gr_gid;
		}

		if (gid <= 0)
			tcfs_error(ER_CUSTOM, "A positive ID please!");

		free(buff);
	}

	if (!tcfs_rmgroup(gid))
		tcfs_error(ER_CUSTOM, "Wrong ID or an error as occurred.\n");

	exit(0);
}
