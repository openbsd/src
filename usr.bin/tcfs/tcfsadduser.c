/*	$OpenBSD: tcfsadduser.c,v 1.5 2000/06/19 23:06:25 aaron Exp $	*/

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

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include <miscfs/tcfs/tcfs.h>

#include "tcfslib.h"
#include "tcfserrors.h"


char *adduser_usage="Usage: %s [OPTION]...
Add an user entry to the TCFS database.

  -l <user>    Username to add to the TCFS database
  -h           Shows this help
  -v           Makes the output a little more verbose\n";

int 
adduser_main(int argn, char *argv[])
{
	char val;
	int have_user = FALSE, be_verbose = FALSE;
	char user[LOGIN_NAME_MAX + 1];
	tcfspwdb *user_info;

	/*
	 * Going to check the arguments
	 */
	while ((val = getopt(argn, argv, "g:l:hv")) != EOF)
		switch (val) {
		case 'l':
			strlcpy(user, optarg, sizeof(user));
			have_user = 1;
			break;
		case 'h':
			show_usage(adduser_usage, argv[0]);
			exit(OK);
			break;
		case 'v':
			be_verbose = TRUE;
			break;
		default:
			fprintf(stderr,
				 "Try %s --help for more information.\n",
				 argv[0]);
			exit(ER_UNKOPT);
			break;
		}

	if (argn - optind)
		tcfs_error(ER_UNKOPT, NULL);

	/*
	 * Here we don't have to drop root privileges because only root
	 * should run us.
	 * However we can do better. Maybe in next versions.
	 */
	if (!have_user) {
		printf("Username to add to TCFS database: ");
		fgets(user, sizeof(user), stdin);
		user[strlen(user) - 1] = '\0';
	}

	if (be_verbose)
		printf("Creating a new entry for user %s in the TCFS database...\n", user);

	/*
	 * Creating a new entry into the key database
	 */
	if (!tcfspwdbr_new(&user_info))
		tcfs_error(ER_MEM, NULL);

	if (!tcfspwdbr_edit(&user_info, F_USR, user))
		tcfs_error(ER_MEM, NULL);

	if (!tcfs_putpwnam(user, user_info, U_NEW))
		tcfs_error(ER_CUSTOM, "Error: cannot add user.");

	if (be_verbose)
		printf("User entry created with success.\n");

	tcfs_error(OK, NULL);
}
