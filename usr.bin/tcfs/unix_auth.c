/*	$OpenBSD: unix_auth.c,v 1.4 2000/06/20 08:01:21 fgsch Exp $	*/

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

#include <sys/param.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfserrors.h"

int
unix_auth(char **user, char **password, int flag)
{
	char *luser, *passwd;
	struct passwd *passentry;

	luser = (char *)calloc(LOGIN_NAME_MAX, sizeof(char));
	passwd = (char *)calloc(_PASSWORD_LEN, sizeof(char));

	if (!luser || !passwd)
		tcfs_error(ER_MEM, NULL);

	if (flag) {
		passentry = getpwuid(getuid());
		strlcpy(luser, passentry->pw_name, LOGIN_NAME_MAX);
	} else {
		printf("Enter user: ");
		fgets(luser, LOGIN_NAME_MAX, stdin);
		luser[strlen(luser)-1] = '\0';
		passentry = getpwnam(luser);
	}

	passwd = getpass("Password:");
	
	if (passentry == NULL) {
		bzero(passwd, strlen(passwd));
		return (0);
	}

	if (strcmp(crypt(passwd, passentry->pw_passwd), passentry->pw_passwd))
		return (0);

	*user = luser;
	*password = passwd;

	return (1);
}
