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
#include <err.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfserrors.h"

char *rmuser_usage="Usage: %s [OPTION]...
Remove an user entry from the TCFS dabatase.

  -l <user>   Username to remove from the TCFS database
  -h          Shows this help
  -v          Makes the output a little more verbose\n";

int 
rmuser_main (int argn, char *argv[])
{
	int have_user = FALSE;
	int be_verbose = FALSE;
	char *user, *passwd;
	tcfspwdb *user_info;
	int val;

	/*
	 * Going to check the arguments
	 */

	 if ((user = (char *) malloc(LOGIN_NAME_MAX + 1)) == NULL)
		 err(1, NULL);

	 while ((val=getopt (argn, argv, "l:hv"))!=EOF)
		 switch (val) {
			case 'l':
				strlcpy (user, optarg, LOGIN_NAME_MAX + 1);
				have_user = TRUE;
				break;

			case 'h':
				show_usage (rmuser_usage, argv[0]);
				exit (OK);
				break;
	
			case 'v':
				be_verbose = TRUE;
				break;
	
			default:
				fprintf (stderr, "Try %s --help for more information.\n", argv[0]);
				exit (ER_UNKOPT);
				break;
		}

	if (argn-optind)
		tcfs_error (ER_UNKOPT, NULL);

	/*
	 * Here we don't have to drop root privileges because only root
	 * should run us.
	 * However we can do better. Maybe in next versions.
	 */
	if (!have_user) {
		int len;

		printf ("Username to remove from TCFS database: ");
		fgets (user, LOGIN_NAME_MAX + 1, stdin);
		len = strlen(user) - 2;
		if (len < 0)
			exit (1);
		user[len] = user[len] == '\n' ? 0 : user[len];
	}

	if (be_verbose)
		printf ("Deleting the entry for user %s from the TCFS database...\n", user);

	/*
	 * Deleting an entry from the key database
	 */
	if (!tcfspwdbr_new (&user_info))
		tcfs_error (ER_MEM, NULL);

	if (!tcfspwdbr_edit (&user_info, F_USR, user))
		tcfs_error (ER_MEM, NULL);

	if (!tcfs_putpwnam (user, user_info, U_DEL))
		tcfs_error (ER_CUSTOM, "Error: cannot remove user.");

	if (be_verbose)
		printf ("User entry removed with success.\n");

	tcfs_error (OK, NULL);
}
