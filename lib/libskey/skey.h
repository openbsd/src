/*
 * OpenBSD S/Key (skey.h)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *          Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Main client header
 *
 * $OpenBSD: skey.h,v 1.15 2001/06/23 21:09:11 millert Exp $
 */

#ifndef _SKEY_H_
#define _SKEY_H_ 1

#include <sys/cdefs.h>

/* Server-side data structure for reading keys file during login */
struct skey {
	FILE *keyfile;
	char *logname;
	char *seed;
	char *val;
	int n;
	int len;
	long recstart;		/* needed so reread of buffer is efficient */
	char buf[256];
};

/* Client-side structure for scanning data stream for challenge */
struct mc {
	int skip;
	int cnt;
	char buf[256];
};

/* Maximum sequence number we allow */
#define SKEY_MAX_SEQ		10000

/* Minimum secret password length (rfc2289) */
#define SKEY_MIN_PW_LEN		10

/* Max secret password length (rfc2289 says 63 but allows more) */
#define SKEY_MAX_PW_LEN		255

/* Max length of an S/Key seed (rfc2289) */
#define SKEY_MAX_SEED_LEN	16

/* Max length of S/Key challenge (otp-???? 9999 seed) */
#define SKEY_MAX_CHALLENGE	(11 + SKEY_MAX_HASHNAME_LEN + SKEY_MAX_SEED_LEN)

/* Max length of hash algorithm name (md4/md5/sha1/rmd160) */
#define SKEY_MAX_HASHNAME_LEN	6

/* Size of a binary key (not NULL-terminated) */
#define SKEY_BINKEY_SIZE	8

/* Location of random file for bogus challenges */
#define _SKEY_RAND_FILE_PATH_	"/var/db/host.random"

__BEGIN_DECLS
void f __P((char *));
int keycrunch __P((char *, char *, char *));
char *btoe __P((char *, char *));
char *put8 __P((char *, char *));
int etob __P((char *, char *));
void rip __P((char *));
int skeychallenge __P((struct skey *, char *, char *));
int skeylookup __P((struct skey *, char *));
int skeyverify __P((struct skey *, char *));
int skeyzero __P((struct skey *, char *));
void sevenbit __P((char *));
void backspace __P((char *));
char *skipspace __P((char *));
char *readpass __P((char *, int));
char *readskey __P((char *, int));
int skey_authenticate __P((char *));
int skey_passcheck __P((char *, char *));
char *skey_keyinfo __P((char *));
int skey_haskey __P((char *));
int atob8 __P((char *, char *));
int btoa8 __P((char *, char *));
int htoi __P((int));
const char *skey_get_algorithm __P((void));
char *skey_set_algorithm __P((char *));
int skeygetnext __P((struct skey *));
int skey_unlock __P((struct skey *));
__END_DECLS

#endif /* _SKEY_H_ */
