/*	$OpenBSD: tcfsaddgroup.c,v 1.13 2002/06/09 02:37:03 itojun Exp $	*/

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
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfserrors.h"

char *addgroup_usage="Usage: %s [OPTION]
Add a TCFS group to the TCFS group database.

  -g <group>          Group id [or name] of the TCFS group
  -m <members>        Number of members of the group
  -t <threshold>      Threshold of the group
  -v                  Makes the output a little more verbose
  -h                  Shows this help\n";

int threshold;
unsigned char coeff[KEYSIZE][256];
unsigned char *S = NULL;          /* Pointer to a 64-bit TCFS group key */

union bobbit
{
	unsigned char byte;
	struct
	{ 
		unsigned char b1:1;
		unsigned char b2:1;
		unsigned char b3:1;
		unsigned char b4:1;
		unsigned char b5:1;
		unsigned char b6:1;
		unsigned char b7:1;
		unsigned char b8:1;
	} bf;
};

int
tcfsgetuid(char *login) 
{    
	struct passwd *entry;

	setpwent();

	while ((entry = getpwent()) != NULL) {
		if (strcmp(login, entry->pw_name) == 0)
			return (entry->pw_uid);
	}

	endpwent();
	free(entry);

	return (-1);
}    

void
gencoeff(void)
{    
	int i, j;

	for (i = 0; i < KEYSIZE; i++) {
		for (j = 1; j < threshold; j++) {
			coeff[j][i] = arc4random();
		}
	}
}

unsigned char *
gengrpkey(char *login)
{
	int x1, i, j, k = 0;
	unsigned int x;

	unsigned char *res = NULL;
	unsigned int tmp;
	union bobbit obits;

	res = (unsigned char *)calloc(KEYSIZE + KEYSIZE / 8, sizeof(char));
	if (!res)
		tcfs_error(ER_MEM, NULL);

	x1 = tcfsgetuid(login);
	x = (x1 % 257);

#ifdef DEBUG_TCFS
	printf("La chiave utente di %u e':\n", x);
#endif

	for (i = 0; i < KEYSIZE; i++) {
		tmp = 0;
		for (j = 1; j < threshold; j++) {
			tmp += (eleva(x1, j, 257) * coeff[j][i]) % 257;
#ifdef DEBUG_TCFS
			printf("x1= %u\tj=%d\tcoeff[%d][%d]=%u\ttmp=%u\tchiave: ", x1, j, j, i, coeff[j][i], tmp);
#endif
		}

		tmp += (unsigned int)S[i];
		tmp %= 257;

		memcpy(res+k++, &tmp, 1);
#ifdef DEBUG_TCFS
		printf("%u\n", *(res+k-1));
#endif
		switch (i % 8) {
		case 0:
			obits.bf.b1 = tmp >> 8;
			break;
		case 1:
			obits.bf.b2 = tmp >> 8;
			break;
		case 2:
			obits.bf.b3 = tmp >> 8;
			break;
		case 3:
			obits.bf.b4 = tmp >> 8;
			break;
		case 4:
			obits.bf.b5 = tmp >> 8;
			break;
		case 5:
			obits.bf.b6 = tmp >> 8;
			break;
		case 6:
			obits.bf.b7 = tmp >> 8;
			break;
		case 7:
			obits.bf.b8 = tmp >> 8;
			break;
		}

		if ((i % 8) == 7) {
			res[k] = obits.byte;
			k++;

#ifdef DEBUG_TCFS
			printf("%u\n", res[k-1]);
#endif

			obits.byte = 0;
		}
	}

	/*
	res[KEYSIZE]=obits.byte;
	*/
	return (res);
}

int 
addgroup_main(int argn, char *argv[])
{
	int val;
	gid_t gid = 0;
	int have_gid = FALSE, have_members = FALSE, have_threshold = FALSE;
	int be_verbose = FALSE;
	int temp_members, members = 0;
	tcfsgpwdb **group_info;

	/*
	 * Going to check the arguments
	 */
	while ((val = getopt(argn, argv, "vg:m:t:h")) != -1)
		switch (val) {
		case 'm':
			members = atoi(optarg);
			have_members = TRUE;
			break;
		case 'g':
			gid = (gid_t)atoi(optarg);
			if (!gid && optarg[0] != '0') { /* group name given */
				struct group *group_id;

				group_id = getgrnam(optarg);
				if (!group_id)
					tcfs_error(ER_CUSTOM,
						"Nonexistent group.");

				gid = group_id->gr_gid;
			}

			have_gid = TRUE;
			break;
		case 't':
			threshold = atoi(optarg);
			have_threshold = TRUE;
			break;
		case 'h':
			printf(addgroup_usage, argv[0]);
			exit(OK);
		case 'v':
			be_verbose = TRUE;
			break;
		default:
			fprintf(stderr,
			    "Try %s --help for more information.\n", argv[0]);
			exit(ER_UNKOPT);
		}

	if (argn-optind)
		tcfs_error(ER_UNKOPT, NULL);

	if (!have_gid) {
		char *buff = NULL;
		int len;

		buff = (char *)malloc(2048);
		if (!buff)
			tcfs_error(ER_MEM, NULL);

		printf("Group ID (or name) of TCFS group to add to the database: ");
		fgets(buff, 2048, stdin);
		len = strlen(buff) - 1;
		buff[len] = buff[len] == '\n' ? 0 : buff[len];
		gid = atoi(buff);

		if (!gid && buff[0] != '0') { /* group name given */ 
			struct group *group_id;

			group_id = getgrnam(buff);
			if (!group_id)
				tcfs_error(ER_CUSTOM, "Nonexistent group.");

			gid = group_id->gr_gid;
		}

		if (gid <= 0)
			tcfs_error(ER_CUSTOM, "A positive ID please!");

		free(buff);
	}

	if (!have_members) {
		char *buff = NULL;
		int len;

		buff = (char *)calloc(2048, sizeof(char));
		if (!buff)
			tcfs_error(ER_MEM, NULL);

		printf("Number of members for the TCFS group ID #%d: ", gid);
		fgets(buff, 2048, stdin);
		len = strlen(buff) - 1;
		buff[len] = buff[len] == '\n' ? 0 : buff[len];
		members = atoi(buff);

		free(buff);
	}

	if (!have_threshold) {
		char *buff = NULL;
		int len;

		buff = (char *)calloc(2048, sizeof(char));
		if (!buff)
			tcfs_error(ER_MEM, NULL);

		printf("Threshold for the TCFS group ID #%d: ", gid);
		fgets(buff, 2048, stdin);
		len = strlen(buff) - 1;
		buff[len] = buff[len] == '\n' ? 0 : buff[len];
		threshold = atoi(buff);

		free(buff);
	}

	if (members < 2)
		tcfs_error(ER_CUSTOM, "At least two members!");

	if (threshold > members || threshold <= 0)
		tcfs_error(ER_CUSTOM, "The threshold must be no greater than the number of members and greater than zero!");

	S = gentcfskey();
#ifdef DEBUG_TCFS
	{
		int i;

		printf("La chiave segreta e':\n");

		for (i = 0; i < KEYSIZE; i++)
			printf("%u:", S[i]);

		printf("\n");
	}
#endif

	gencoeff();

	temp_members = members;

	group_info = (tcfsgpwdb **)calloc(members, sizeof(tcfsgpwdb *));

	/*
	 * Creating user entry
	 */
	while (members) {
		char *user = NULL, *passwd = NULL;
		unsigned char *newkey = NULL, *cryptedkey = NULL;
		tcfsgpwdb *tmp = NULL;
		int tmpmemb = temp_members, cont = 0;

		group_info[members - 1] = (tcfsgpwdb *)calloc(1,
		    sizeof(tcfsgpwdb));

		group_info[members - 1]->gid = gid;
		group_info[members - 1]->n = members;
		group_info[members - 1]->soglia = threshold;

		if (!unix_auth(&user, &passwd, FALSE)) {
			fprintf(stderr, "Invalid password or the user does not exist.\n");
			continue;
		}

		if (tcfs_ggetpwnam(user, gid, &tmp))
			tcfs_error(ER_CUSTOM, "Group already exists.");

		while (tmpmemb > members) {
			if (!strcmp(user, group_info[tmpmemb-1]->user)) {
				fprintf(stderr, "User already present into the group.\n");
				cont = 1;
				break;
			}
			tmpmemb--;
		}

		if (cont)
			continue;

		strcpy(group_info[members - 1]->user, user);

		newkey = (unsigned char *)calloc(GKEYSIZE + 1, sizeof(char));
		if (!newkey)
			tcfs_error(ER_MEM, NULL);

		cryptedkey = (unsigned char *)calloc(UUGKEYSIZE, sizeof(char));
		if (!cryptedkey)
			tcfs_error(ER_MEM, NULL);

		memcpy(newkey, gengrpkey(user), GKEYSIZE);
		newkey[GKEYSIZE] = '\0';

		/*
		 * Encrypt the just generated key with the user password
		 */
		if (!tcfs_encrypt_key(passwd, newkey, GKEYSIZE, cryptedkey,
		    UUGKEYSIZE))
			tcfs_error(ER_MEM, NULL);

		free(newkey);

		strlcpy(group_info[members - 1]->gkey, cryptedkey,
		    sizeof(group_info[members - 1]->gkey));
		free(cryptedkey);

		members--;
	}

	members = temp_members;

	while (members) {
		if (be_verbose)
			printf("Creating a new entry for group %d and user %s in the TCFS database...\n", 
				group_info[members - 1]->gid,
				group_info[members - 1]->user);

		if (!tcfs_gputpwnam(group_info[members - 1]->user,
				     group_info[members - 1], U_NEW)) {
				/* TODO: Remove the group entries saved before */
			tcfs_error(ER_CUSTOM, "Error: cannot add a user to the group.");
		}

		if (be_verbose)
			printf("TCFS group entry for user %s created.\n", group_info[members - 1]->user);

		members--;
	}

	tcfs_error(ER_CUSTOM, "\nAll group keys generated.");

	return (0);
}


int
eleva(int x, int y, int z)
{
	int mask = 0x80000000;
	int res = 1, i;

	for (i = 0; i < 32; i++) {
		res = (res * res) % z;
		if (y & mask)
			res = (x * res) % z;
		mask = mask >> 1;
	}

	return (res);
}
