/*     $OpenBSD: supextern.h,v 1.2 1997/09/16 11:01:22 deraadt Exp $  */

#ifndef __P
#ifdef __STDC__
#define __P(a)	a
#else
#define	__P(a) ()
#endif
#endif

/* atoo.c */
unsigned int atoo __P((char *));

#if 0
/* ci.c */
int ci __P((char *, FILE *, int, CIENTRY *, char *, char *));
#endif

/* errmsg.c */
char *errmsg __P((int));

/* expand.c */
int expand __P((char *, char **, int));

/* ffilecopy.c */
int ffilecopy __P((FILE *, FILE *));

/* filecopy.c */
int filecopy __P((int, int ));

/* log.c */
void logopen __P((char *));
void logquit __P((int, char *, ...));
void logerr __P((char *, ...));
void loginfo __P((char *, ...));

/* netcryptvoid.c */
int netcrypt __P((char *));
int getcryptbuf __P((int));
void decode __P((char *, char *, int));
void encode __P((char *, char *, int));

/* nxtarg.c */
char *nxtarg __P((char **, char *));

/* path.c */
void path __P((char *, char *, char *, int));

/* quit.c */
void quit __P((int, char *, ...));

/* run.c */
int run __P((char *, ...));
int runv __P((char *, char **));
int runp __P((char *, ...));
int runvp __P((char *, char **));
int runio __P((char *const[], const char *, const char *, const char *));

/* salloc.c */
char *salloc __P((char *));

/* scan.c */
int getrelease __P((char *));
void makescanlists __P((void));
void getscanlists __P((void));
void cdprefix __P((char *));

/* scm.c */
int servicesetup __P((char *));
int service __P((void));
int serviceprep __P((void));
int servicekill __P((void));
int serviceend __P((void));
int dobackoff __P((int *, int *));
int request __P((char *, char *, int *));
int requestend __P((void));
char *remotehost __P((void));
int thishost __P((char *));
int samehost __P((void));
int matchhost __P((char *));
int scmerr __P((int, char *, ...));
int byteswap __P((int));

/* scmio.c */
int writemsg __P((int));
int writemend __P((void));
int writeint __P((int));
int writestring __P((char *));
int writefile __P((int));
int writemnull __P((int));
int writemint __P((int, int ));
int writemstr __P((int, char *));
int prereadcount __P((int *));
int readflush __P((void));
int readmsg __P((int));
int readmend __P((void));
int readskip __P((void));
int readint __P((int *));
int readstring __P((char **));
int readfile __P((int));
int readmnull __P((int));
int readmint __P((int, int *));
int readmstr __P((int, char **));
void crosspatch __P((void));

/* skipto.c */
char *skipto __P((char *, char *));
char *skipover __P((char *, char *));

/* stree.c */
void Tfree __P((TREE **));
TREE *Tinsert __P((TREE **, char *, int));
TREE *Tsearch __P((TREE *, char *));
TREE *Tlookup __P((TREE *, char *));
int Trprocess __P((TREE *, int (*)(TREE *, void *), void *));
int Tprocess __P((TREE *, int (*)(TREE *, void *), void *));
void Tprint __P((TREE *, char *));

/* supcmeat.c */
int getonehost __P((TREE *, void *));
TREE *getcollhost __P((int *, int *, long *, int *));
void getcoll __P((void));
int signon __P((TREE *, int, int *));
int setup __P((TREE *));
void login __P((void));
void listfiles __P((void));
void recvfiles __P((void));
int prepare __P((char *, int, int *, struct stat *));
int recvdir __P((TREE *, int, struct stat *));
int recvsym __P((TREE *, int, struct stat *));
int recvreg __P((TREE *, int, struct stat *));
int copyfile __P((char *, char *));
void finishup __P((int));
void done __P((int, char *, ...));
void goaway __P((char *, ...));

/* supcmisc.c */
void prtime __P((void));
int establishdir __P((char *));
int estabd __P((char *, char *));
void ugconvert __P((char *, char *, int *, int *, int *));
void notify __P((char *, ...));
void lockout __P((int));
char *fmttime __P((time_t));

/* supcname.c */
void getnams __P((void));

/* supcparse.c */
int parsecoll __P((COLLECTION *, char *, char *));
time_t getwhen __P((char *, char *));
int putwhen __P((char *, time_t));

/* supmsg.c */
int msgsignon __P((void));
int msgsignonack __P((void));
int msgsetup __P((void));
int msgsetupack __P((void));
int msgcrypt __P((void));
int msgcryptok __P((void));
int msglogin __P((void));
int msglogack __P((void));
int msgrefuse __P((void));
int msglist __P((void));
int msgneed __P((void));
int msgdeny __P((void));
int msgsend __P((void));
int msgrecv __P((int (*)(TREE *, va_list), ...));
int msgdone __P((void));
int msggoaway __P((void));
int msgxpatch __P((void));
int msgcompress __P((void));

/* vprintf.c */
int vprintf __P((const char *, va_list));
int vfprintf __P((FILE *, const char *, va_list));
int vsprintf __P((char *, const char *, va_list));
int vsnprintf __P((char *, size_t, const char *, va_list));
