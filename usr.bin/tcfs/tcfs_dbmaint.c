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
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <db.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfslib.h"
#include "tcfspwdb.h"

#define PERM_SECURE	(S_IRUSR|S_IWUSR)

int 
tcfspwdbr_new (tcfspwdb **new)
{
	*new = (tcfspwdb *)calloc(1, sizeof(tcfspwdb));

	if (!*new)
		return (0);

	return (1);
}

int
tcfsgpwdbr_new (tcfsgpwdb **new)
{
	*new = (tcfsgpwdb *)calloc(1, sizeof(tcfsgpwdb));

	if (!*new)
		return (0);

	return (1);
}

int 
tcfspwdbr_edit (tcfspwdb **tmp, int flags,...)
{
	va_list argv;
	char *d;

	if (!*tmp)
		if (!tcfspwdbr_new (tmp))
			return 0;

	va_start (argv, flags);

	if (flags & F_USR) {
		d = va_arg (argv, char *);
		strcpy ((*tmp)->user, d);
	}

	if (flags & F_PWD) {
		d = va_arg (argv, char *);
		strcpy ((*tmp)->upw, d);
	}

	va_end (argv);
	return 1;
}

int 
tcfsgpwdbr_edit (tcfsgpwdb **tmp, int flags,...)
{
	va_list argv;
	char *d;

	if (!*tmp)
		if (!tcfsgpwdbr_new (tmp))
			return 0;

	va_start (argv, flags);

	if (flags & F_USR) {
		d = va_arg (argv, char *);
		strcpy ((*tmp)->user, d);
	}

	if (flags & F_GKEY) {
		d = va_arg (argv, char *);
		strcpy ((*tmp)->gkey, d);
	}

	if (flags & F_GID) {
		gid_t d;
		d = va_arg (argv, gid_t);
		(*tmp)->gid = d;
	}

	if (flags & F_MEMBERS) {
		int d;
		d = va_arg (argv, int);
		(*tmp)->n = d;
	}

	if (flags & F_THRESHOLD) {
		int d;
		d = va_arg (argv, int);
		(*tmp)->soglia = d;
	}

	va_end (argv);
	return (1);
}

int 
tcfspwdbr_read (tcfspwdb *t, int flags,...)
{
	va_list argv;
	int r;
	char *d;

	va_start (argv, flags);

	if (flags & F_USR) {
		d = va_arg (argv, char *);
		memset (d, 0, UserLen);
		strcpy (d, t->user);
	}

	if (flags & F_PWD) {
		d = va_arg (argv, char *);
		memset (d, 0, PassLen);
		strcpy (d, t->upw);
	}

	va_end (argv);
	return 0;
}

int 
tcfsgpwdbr_read (tcfsgpwdb *t, int flags,...)
{
	va_list argv;
	int r; 
	char *d;

	va_start (argv, flags);

	if (flags & F_USR) {
		d = va_arg (argv, char *);
		strcpy (d, t->user);
	}

	if (flags & F_GKEY) {
		d = va_arg (argv, char *);
		strcpy (d, t->gkey);
	}

	if (flags & F_GID) {
		gid_t *d;

		d = va_arg (argv, gid_t *);
		memcpy (d, &t->gid, sizeof (gid_t));
	}
	/* Incomplete... */

	va_end (argv);
	return 0;
}

void 
tcfspwdbr_dispose (tcfspwdb *t)
{
	free ((void *)t);
}

void 
tcfsgpwdbr_dispose (tcfsgpwdb *t)
{
	free ((void *)t);
}

tcfspwdb *
tcfs_getpwnam (char *user, tcfspwdb **dest)
{
	DB *pdb;
	DBT srchkey, r;

	if (!*dest)
		if (!tcfspwdbr_new (dest))
			return NULL;

	pdb = dbopen (TCFSPWDB, O_RDONLY, 0, DB_HASH, NULL);
	if (!pdb)
		return NULL;

	srchkey.data = user;
	srchkey.size = (int) strlen (user);

	if (pdb->get(pdb, &srchkey, &r, 0)) {
		pdb->close(pdb);
		return 0;
	}

	if (r.size != sizeof(tcfspwdb)) {
		fprintf(stderr, "db: incorrect record size: %d != %d\n",
			r.size, sizeof(tcfspwdb));
		pdb->close(pdb);
		return 0;
	}

	memcpy (*dest, r.data, sizeof (tcfspwdb));

	pdb->close (pdb);

	return (tcfspwdb *)*dest;
}

tcfsgpwdb *
tcfs_ggetpwnam (char *user, gid_t gid, tcfsgpwdb **dest)
{
	DB *pdb;
	DBT srchkey, r;
	char *key, *buf;
	int res;

	if (!*dest)
		if (!tcfsgpwdbr_new (dest))
			return NULL;

	pdb = dbopen (TCFSGPWDB, O_RDONLY, 0, DB_HASH, NULL);
	if (!pdb)
		return NULL;

	key = (char*)calloc(strlen(user)+4/*gid lenght*/+1/*null*/,sizeof(char));
	if (!key)
		return NULL;

	sprintf (key, "%s\33%d\0", user, (int)gid);
	srchkey.data = key;
	srchkey.size = (int)strlen (key);

	if ((res = pdb->get(pdb, &srchkey, &r, 0))) {
		if (res == -1)
			perror("dbget");
		pdb->close (pdb);
		return (NULL);
	}

	memcpy (*dest, r.data, sizeof (tcfsgpwdb));

	pdb->close (pdb);

	return (*dest);
}

int 
tcfs_putpwnam (char *user, tcfspwdb *src, int flags)
{
	DB *pdb;
	static DBT srchkey, d;
	int open_flag = 0, res;

	open_flag = O_RDWR|O_EXCL;
	if (access (TCFSPWDB, F_OK) < 0)
		open_flag |= O_CREAT;

	pdb = dbopen (TCFSPWDB, open_flag, PERM_SECURE, DB_HASH, NULL);
	if (!pdb)
		return 0;

	srchkey.data = user;
	srchkey.size = (int)strlen (user);

	if (flags != U_DEL) {
		d.data = (char *)src;
		d.size = (int)sizeof(tcfspwdb);

		if (pdb->put(pdb, &srchkey, &d, 0) == -1) {
			fprintf(stderr, "db: put failed\n");
			pdb->close (pdb);
			return 0;
		}
	} else if ((res = pdb->del (pdb, &srchkey, 0))) {
		fprintf(stderr, "db: del failed: %s\n", 
			res == -1 ? "error" : "not found");
		pdb->close (pdb);
		return 0;
	}

	pdb->close (pdb);
	return 1;
}

int 
tcfs_gputpwnam (char *user, tcfsgpwdb *src, int flags)
{
	DB *pdb;
	static DBT srchkey, d;
	int open_flag = 0;
	char *key, *buf;
	char *tmp;

	open_flag = O_RDWR|O_EXCL;
	if (access (TCFSGPWDB, F_OK) < 0)
		open_flag |= O_CREAT;

	pdb = dbopen (TCFSGPWDB, open_flag, PERM_SECURE, DB_HASH, NULL);
	if (!pdb) {
		perror("dbopen");
		return 0;
	}

	key = (char *) calloc (strlen(src->user) + 4 + 1, sizeof(char));
	sprintf (key, "%s\33%d\0", src->user, src->gid);

	srchkey.data = key;
	srchkey.size = strlen (key);

	if (flags != U_DEL) {
		d.data = (char *)src;
		d.size = sizeof(tcfsgpwdb);

		if (pdb->put (pdb, &srchkey, &d, 0) == -1) {
			fprintf(stderr, "db: put failed\n");
			pdb->close (pdb);
			return 0;
		}
	} else if (pdb->del (pdb, &srchkey, 0)) {
		fprintf(stderr, "db: del failed\n");
		pdb->close (pdb);
		return 0;
	}

	pdb->close (pdb);
	return 1;
}

int
tcfs_rmgroup (gid_t gid)
{
	DB *gdb;
	DBT dbkey;

	gdb = dbopen(TCFSGPWDB, O_RDWR|O_EXCL, PERM_SECURE, DB_HASH, NULL);
	if (!gdb)
		return 0;

	if (gdb->seq(gdb, &dbkey, NULL, R_FIRST))
		dbkey.data = NULL;

	while (dbkey.data) {
		char *tmp;

		tmp = (char*)calloc(1024, sizeof(char));

		sprintf(tmp, "\33%d\0", gid);
		if (strstr (dbkey.data, tmp)) {
			if (gdb->del(gdb, &dbkey, 0)) {
				gdb->close (gdb);

				free (tmp);
				return 0;
			}
		}
		free (tmp);

		if (gdb->seq(gdb, &dbkey, NULL, R_NEXT)) {
			gdb->close(gdb);
			return (0);
		}
	}

	gdb->close (gdb);
	return (1);
}


int
tcfs_group_chgpwd (char *user, gid_t gid, char *old, char *new)
{
	tcfsgpwdb *group_info;
	unsigned char *key;

	key=(unsigned char *)calloc(UUKEYSIZE, sizeof (char));
	if (!key)
		return 0;

	if (!tcfs_decrypt_key (user, old, (unsigned char*)group_info->gkey, key, GROUPKEY))
		return 0;

	if (!tcfs_encrypt_key (user, new, key, (unsigned char *)group_info->gkey, GROUPKEY))
		return 0;

	if (!tcfs_gputpwnam (user, group_info, U_CHG))
		return 0;

	free (group_info);
	free (key);

	return 1;
}

int 
tcfs_chgpwd (char *user, char *old, char *new)
{
	tcfspwdb *user_info=NULL;
	unsigned char *key;

	key = (unsigned char*)calloc(UUKEYSIZE, sizeof(char));

	if (!tcfs_getpwnam (user, &user_info))
		return 0;

	if (!tcfs_decrypt_key (user, old, (unsigned char *)user_info->upw, key, USERKEY))
		return 0;

	if (!tcfs_encrypt_key (user, new, key, (unsigned char *)user_info->upw, USERKEY))
		return 0;

	if (!tcfs_putpwnam (user, user_info, U_CHG))
		return 0;

	free (user_info);
	free (key);

	return 1;
}

int
tcfs_chgpassword (char *user, char *old, char *new)
{
	int error1=0, error2=0;
	DB *gpdb;
	DBT found, key;
	unsigned char *ckey;

	ckey = (unsigned char*)calloc(UUKEYSIZE, sizeof(char));
	if (!ckey)
		return 0;

	gpdb = dbopen (TCFSGPWDB, O_RDWR|O_EXCL, PERM_SECURE, DB_HASH, NULL);
	if (!gpdb)
		return 0;

	error1 = tcfs_chgpwd (user, old, new);
	if (!error1)
		return 0;

	/* Reencrypt group shares */
	if (gpdb->seq(gpdb, &key, NULL, R_FIRST)) 
		key.data = NULL;
	
	while (key.data) {
		if (strncmp (user, key.data, strlen(user))) {
			if (gpdb->seq(gpdb, &key, NULL, R_NEXT))
			    key.data = NULL;
			continue;
		}

		gpdb->get(gpdb, &key, &found, 0);

		if (!tcfs_decrypt_key (user, old, (unsigned char *)((tcfsgpwdb *)found.data)->gkey, ckey, USERKEY))
			return 0;

		if (!tcfs_encrypt_key (user, new, ckey, (unsigned char *)((tcfsgpwdb *)found.data)->gkey, USERKEY))
			return 0;

		if (gpdb->put (gpdb, &key, &found, 0)) {
			free (ckey);

			gpdb->close (gpdb);
			return (0);
		}

		free (ckey);

		if (gpdb->seq(gpdb, &key, NULL, R_NEXT))
		    key.data = NULL;
	}

	return 1;
}
