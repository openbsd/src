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
 * $Id: skey.h,v 1.2 1995/12/20 09:48:23 deraadt Exp $
 */

#if	defined(__TURBOC__) || defined(__STDC__) || defined(LATTICE)
#define	ANSIPROTO	1
#endif

#ifndef	__ARGS
#ifdef	ANSIPROTO
#define	__ARGS(x)	x
#else
#define	__ARGS(x)	()
#endif
#endif

#ifdef SOLARIS
#define setpriority(x,y,z)      z
#endif

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

void f __ARGS ((char *x));
int keycrunch __ARGS ((char *result, char *seed, char *passwd));
char *btoe __ARGS ((char *engout, char *c));
char *put8 __ARGS ((char *out, char *s));
int etob __ARGS ((char *out, char *e));
void rip __ARGS ((char *buf));
int skeychallenge __ARGS ((struct skey * mp, char *name, char *ss));
int skeylookup __ARGS ((struct skey * mp, char *name));
int skeyverify __ARGS ((struct skey * mp, char *response));
int skeyzero __ARGS ((struct skey * mp, char *response));
void sevenbit __ARGS ((char *s));
void backspace __ARGS ((char *s));
char *skipspace __ARGS ((char *s));
char *readpass __ARGS ((char *buf, int n));
char *readskey __ARGS ((char *buf, int n));
int skey_authenticate __ARGS ((char *));
int skey_passcheck __ARGS ((char *, char *));
char *skey_keyinfo __ARGS ((char *));
int skey_haskey __ARGS ((char *));
int getskeyprompt __ARGS ((struct skey *, char *, char *));
int atob8 __ARGS((char *, char *));
int btoa8 __ARGS((char *, char *));
int htoi __ARGS((char));

