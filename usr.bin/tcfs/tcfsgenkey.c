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
#include <strings.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfserrors.h"

char *genkey_usage="Usage: %s [OPTION]
Generate a TCFS key adding it to the user entry into the TCFS database.

  -h       Shows this help\n";

int
genkey_main (int argn, char *argv[])
{
	int val;
	char *user, *passwd;
	tcfspwdb *userinfo;
	unsigned char *newkey, *cryptedkey;
	tcfspwdb *user_info = NULL;

	/*
	 * Going to check arguments
	 */
	while ((val = getopt(argn, argv, "h")) != EOF)
		switch (val) {
		case 'h':
			show_usage(genkey_usage, argv[0]);
			exit (OK);
			break; /* Useless code */
		default:
			fprintf (stderr, "Try %s --help for more information.\n", argv[0]);
			exit (ER_UNKOPT);
			break;
		}

	if (argn - optind)
		tcfs_error (ER_UNKOPT, NULL);

	/*
	 * Must be root to do all this stuff
	 */
	if (geteuid())
		tcfs_error (ER_CUSTOM, "I don't have root privileges!");

	/*
	 * Authenticate user
	 */
	if (!unix_auth (&user, &passwd, TRUE))
		tcfs_error (ER_CUSTOM, "Who are you?!");

	if (!tcfs_getpwnam (user, &user_info))
		tcfs_error (ER_CUSTOM, "You do not have an entry in the TCFS key database.");

	if (strlen(user_info->upw))
		tcfs_error (ER_CUSTOM,"You already have a TCFS key.");

	/*
	 * Generate a new key for the user.
	 */
	newkey = gentcfskey ();

	/*
	 * Encrypt the generated key with user password
	 */
	cryptedkey = (char*)calloc(UUKEYSIZE, sizeof(char));
	if (!cryptedkey)
		tcfs_error (ER_MEM, NULL);

	
	if (!tcfs_encrypt_key (user, passwd, newkey, cryptedkey, USERKEY))
		tcfs_error (ER_MEM, NULL);

	/*
	 * Update TCFS key database
	 */
	if (!tcfspwdbr_new (&userinfo))
		tcfs_error (ER_MEM, NULL);

	if (!tcfspwdbr_edit (&userinfo, F_USR|F_PWD, user, cryptedkey))
		tcfs_error (ER_MEM, NULL);

	/* TODO:
	   if (!change && tcfs_getpwnam (user, &userinfo))
	   tcfs_error (ER_CUSTOM, "Use -c to change the key.");
	*/

	if (!tcfs_putpwnam (user, userinfo, U_CHG))
		tcfs_error (ER_CUSTOM, "Error: cannot generate key.");

	tcfs_error (ER_CUSTOM, "\nKey succesfully generated.");
}
