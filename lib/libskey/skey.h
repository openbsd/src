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
 *
 * Main client header
 *
 * $Id: skey.h,v 1.6 1996/10/14 03:09:12 millert Exp $
 */

/* Server-side data structure for reading keys file during login */
struct skey
{
	FILE *keyfile;
	char buf[256];
	char *logname;
	int n;
	char *seed;
	char *val;
	long recstart;		/* needed so reread of buffer is efficient */
};

/* Client-side structure for scanning data stream for challenge */
struct mc
{
	char buf[256];
	int skip;
	int cnt;
};

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
const char * skey_get_algorithm __P((void));
char * skey_set_algorithm __P((char *new));
