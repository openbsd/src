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
#include <sys/ucred.h>
#include <blf.h>
#include <ctype.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

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
tcfs_decrypt_key (char *pwd, u_char *t, u_char *tk, int tklen)
{
	char pass[_PASSWORD_LEN];
	char tcfskey[2*KEYSIZE], iv[8];
	blf_ctx ctx;
	int len;

	if (!tk)
		return 0;

	strlcpy (pass, pwd, sizeof(pass));

	len = uudecode ((char *)t, tcfskey, sizeof(tcfskey));
	if (len == -1) {
		fprintf(stderr, "tcfs_decrypt_key: uudecode failed\n");
		return 0;
	} else 	if (len != tklen) {
		fprintf(stderr, "tcfs_decrypt_key: uudecode wrong length\n");
		return 0;
	}

	while (strlen (pass) < 8) {
		char tmp[_PASSWORD_LEN];
		strcpy (tmp, pass);
		strcat (tmp, pass);
		strcat (pass, tmp);
	}

	blf_key(&ctx, pass, strlen(pass));
	memset(iv, 0, sizeof(iv));
	blf_cbc_decrypt(&ctx, iv, tcfskey, tklen);

	memset (pass, 0, strlen (pass));
	memset (&ctx, 0, sizeof(ctx));

	memcpy (tk, tcfskey, tklen);
	return 1;
}

int 
tcfs_encrypt_key (char *pw, u_char *key, int klen, u_char *ek, int eklen)
{
	char pass[_PASSWORD_LEN], iv[8];
	blf_ctx ctx;
	int res;

	if (!ek)
		return 0;

	strlcpy (pass, pw, sizeof(pass));

	while (strlen(pass) < 8) {
		char tmp[_PASSWORD_LEN];
      
		strcpy (tmp, pass);
		strcat (tmp, pass);
		strcat (pass, tmp);
	}
	
	blf_key(&ctx, pass, strlen(pass));
	memset(iv, 0, sizeof(iv));
	blf_cbc_encrypt(&ctx, iv, key, klen);

	memset(&ctx, 0, sizeof(ctx));

	res = uuencode (key, klen, ek, eklen);
	if (res != eklen - 1) {
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


