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

#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ucred.h>
#include <des.h>
#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_cmd.h>

#include "tcfsdefines.h"
#include "uuencode.h"

int
tcfs_verify_fs(char *fs)
{
	int ret;
	struct statfs buf;

	ret = statfs(fs, &buf);

	if (ret)
		 return 0;

	if (!strcmp("tcfs", buf.f_fstypename))
		return (1);
	else	
		return (0);
}

int
tcfs_callfunction(char *filesystem, struct tcfs_args *arg)
{
	int i;
	if (tcfs_verify_fs(filesystem))
		i = mount("tcfs",filesystem,MNT_UPDATE,(void*)arg);
	else
		i = -1;
	
	return (i);
}

int 
tcfs_decrypt_key (char *u, char *pwd, unsigned char *t, unsigned char *tk,
		  unsigned int flag)
{
	int i = 0;
	char pass[_PASSWORD_LEN], *cypher;
	char tcfskey[2*KEYSIZE];
	des_key_schedule ks;
	int keysize = (flag == GROUPKEY) ? KEYSIZE + KEYSIZE/8 : KEYSIZE;

	if (!tk)
		return 0;

	strcpy (pass, pwd);

	if (uudecode ((char *)t, tcfskey, sizeof(tcfskey)) == -1) {
		fprintf(stderr, "tcfs_decrypt_key: uudecode failed\n");
		return 0;
	}

	while (strlen (pass) < 8) {
		char tmp[_PASSWORD_LEN];
		strcpy (tmp, pass);
		strcat (tmp, pass);
		strcat (pass, tmp);
	}

	while ((i*8) < keysize) {
		des_set_key ((des_cblock *) pass, ks);

		des_ecb_encrypt ((des_cblock *) (tcfskey+i*8),
				 (des_cblock *) (tcfskey+i*8), ks, DES_DECRYPT);
		i++;
	}
	memset (pass, 0, strlen (pass));

	memcpy (tk, tcfskey, keysize);
	return 1;
}

int 
tcfs_encrypt_key (char *u, char *pw, unsigned char *key, unsigned char *ek,
		  unsigned int flag)
{
	int i = 0;
	char pass[_PASSWORD_LEN];
	des_key_schedule ks;
	int keysize = (flag == GROUPKEY) ? KEYSIZE + KEYSIZE/8 : KEYSIZE;
	int uulen = (flag == GROUPKEY) ? UUGKEYSIZE : UUKEYSIZE;
	int res;

	if (!ek)
		return 0;

	strcpy (pass, pw);

	while (strlen(pass) < 8) {
		char tmp[_PASSWORD_LEN];
      
		strcpy (tmp, pass);
		strcat (tmp, pass);
		strcat (pass, tmp);
	}

	while ((i*8) < keysize) {
		des_set_key((des_cblock *) pass, ks);
		des_ecb_encrypt((des_cblock *) (key + i * 8),
				(des_cblock *) (key + i * 8), ks, DES_ENCRYPT);
		i++;
	}

	res = uuencode (key, keysize, ek, uulen + 1);
	if (res != uulen) {
		fprintf(stderr, "tcfs_encrypt_key: uuencode length wrong\n");
		return (0);
	}

	return 1;
}

int
tcfs_user_enable(char *filesystem, uid_t user, u_char *key)
{
	struct tcfs_args a;
	a.user = user;
	memcpy(a.tcfs_key, key, sizeof(a.tcfs_key));
	a.cmd = TCFS_PUT_UIDKEY;
	return tcfs_callfunction(filesystem,&a);
}

int
tcfs_user_disable(char *filesystem, uid_t user)
{
	struct tcfs_args a;
	a.user = user;
	a.cmd = TCFS_RM_UIDKEY;
	return tcfs_callfunction(filesystem, &a);
}

int
tcfs_proc_enable(char *filesystem, uid_t user, pid_t pid, char *key)
{
	struct tcfs_args a;
	a.user = user;
	a.cmd = TCFS_PUT_PIDKEY;
	a.proc = pid;
	memcpy(a.tcfs_key, key, sizeof(a.tcfs_key));
	return tcfs_callfunction(filesystem, &a);
}

int
tcfs_proc_disable(char *filesystem, uid_t user, pid_t pid)
{
	struct tcfs_args a;
	a.user = user;
	a.cmd = TCFS_RM_PIDKEY;
	a.proc = pid;
	return tcfs_callfunction(filesystem, &a);
}

int
tcfs_group_enable(char *filesystem, uid_t uid, gid_t gid, 
		  int tre, char *key)
{
	struct tcfs_args a;
	a.cmd = TCFS_PUT_GIDKEY;
	a.user = uid;
	a.group = gid;
	a.treshold = tre;
	memcpy(a.tcfs_key, key, sizeof(a.tcfs_key));
	return tcfs_callfunction(filesystem,&a);
}

int tcfs_group_disable(char *filesystem, uid_t uid, gid_t gid)
{
	struct tcfs_args a;
	a.cmd = TCFS_RM_GIDKEY;
	a.user = uid;
	a.group = gid;
	return tcfs_callfunction(filesystem,&a);
}


