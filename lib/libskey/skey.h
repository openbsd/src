/*
 * S/KEY v1.1b (skey.h)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *
 * Modifications:
 *          Scott Chasin <chasin@crimelab.com>
 *          Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Main client header
 *
 * $OpenBSD: skey.h,v 1.12 1998/07/03 02:06:21 angelos Exp $
 */

/* Server-side data structure for reading keys file during login */
struct skey {
	FILE *keyfile;
	char buf[256];
	char *logname;
	int n;
	char *seed;
	char *val;
	long recstart;		/* needed so reread of buffer is efficient */
};

/* Client-side structure for scanning data stream for challenge */
struct mc {
	char buf[256];
	int skip;
	int cnt;
};

/* Maximum sequence number we allow */
#ifndef SKEY_MAX_SEQ
#define SKEY_MAX_SEQ		10000
#endif

/* Minimum secret password length (rfc1938) */
#ifndef SKEY_MIN_PW_LEN
#define SKEY_MIN_PW_LEN		10
#endif

/* Max secret password length (rfc1938 says 63 but allows more) */
#ifndef SKEY_MAX_PW_LEN
#define SKEY_MAX_PW_LEN		255
#endif

/* Max length of an S/Key seed (rfc1938) */
#ifndef SKEY_MAX_SEED_LEN
#define SKEY_MAX_SEED_LEN	16
#endif

/* Max length of S/Key challenge (otp-???? 9999 seed) */
#ifndef SKEY_MAX_CHALLENGE
#define SKEY_MAX_CHALLENGE	(11 + SKEY_MAX_HASHNAME_LEN + SKEY_MAX_SEED_LEN)
#endif

/* Max length of hash algorithm name (md4/md5/sha1/rmd160) */
#define SKEY_MAX_HASHNAME_LEN	6

/* Size of a binary key (not NULL-terminated) */
#define SKEY_BINKEY_SIZE	8

/* Location of random file for bogus challenges */
#define _SKEY_RAND_FILE_PATH_	"/etc/host.random"

/* Prototypes */
void f __P((char *x));
int keycrunch __P((char *result, char *seed, char *passwd));
char *btoe __P((char *engout, char *c));
char *put8 __P((char *out, char *s));
int etob __P((char *out, char *e));
void rip __P((char *buf));
int skeychallenge __P((struct skey * mp, char *name, char *ss));
int skeylookup __P((struct skey * mp, char *name));
int skeyverify __P((struct skey * mp, char *response));
int skeyzero __P((struct skey * mp, char *response));
void sevenbit __P((char *s));
void backspace __P((char *s));
char *skipspace __P((char *s));
char *readpass __P((char *buf, int n));
char *readskey __P((char *buf, int n));
int skey_authenticate __P((char *username));
int skey_passcheck __P((char *username, char *passwd));
char *skey_keyinfo __P((char *username));
int skey_haskey __P((char *username));
int getskeyprompt __P((struct skey *mp, char *name, char *prompt));
int atob8 __P((char *out, char *in));
int btoa8 __P((char *out, char *in));
int htoi __P((int c));
const char *skey_get_algorithm __P((void));
char *skey_set_algorithm __P((char *new));
int skeygetnext __P((struct skey *mp));
