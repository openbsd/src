/*    handy.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#if !defined(__STDC__)
#ifdef NULL
#undef NULL
#endif
#ifndef I286
#  define NULL 0
#else
#  define NULL 0L
#endif
#endif

#define Null(type) ((type)NULL)
#define Nullch Null(char*)
#define Nullfp Null(FILE*)
#define Nullsv Null(SV*)

/* bool is built-in for g++-2.6.3, which might be used for an extension.
   If the extension includes <_G_config.h> before this file then
   _G_HAVE_BOOL will be properly set.  If, however, the extension includes
   this file first, then you will have to manually set -DHAS_BOOL in 
   your command line to avoid a conflict.
*/
#ifdef _G_HAVE_BOOL
# if _G_HAVE_BOOL
#  ifndef HAS_BOOL
#   define HAS_BOOL 1
#  endif
# endif
#endif

#ifndef HAS_BOOL
# ifdef UTS
#  define bool int
# else
#  define bool char
# endif
#endif

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE (1)
#define FALSE (0)

typedef char		I8;
typedef unsigned char	U8;

typedef short		I16;
typedef unsigned short	U16;

#if BYTEORDER > 0x4321
  typedef int		I32;
  typedef unsigned int	U32;
#else
  typedef long		I32;
  typedef unsigned long	U32;
#endif

#define Ctl(ch) (ch & 037)

#define strNE(s1,s2) (strcmp(s1,s2))
#define strEQ(s1,s2) (!strcmp(s1,s2))
#define strLT(s1,s2) (strcmp(s1,s2) < 0)
#define strLE(s1,s2) (strcmp(s1,s2) <= 0)
#define strGT(s1,s2) (strcmp(s1,s2) > 0)
#define strGE(s1,s2) (strcmp(s1,s2) >= 0)
#define strnNE(s1,s2,l) (strncmp(s1,s2,l))
#define strnEQ(s1,s2,l) (!strncmp(s1,s2,l))

#ifdef HAS_SETLOCALE  /* XXX Is there a better test for this? */
#  ifndef CTYPE256
#    define CTYPE256
#  endif
#endif

#ifdef USE_NEXT_CTYPE 
#define isALNUM(c)   (NXIsAlpha((unsigned int)c) || NXIsDigit((unsigned int)c) || c == '_')
#define isIDFIRST(c) (NXIsAlpha((unsigned int)c) || c == '_')
#define isALPHA(c)   NXIsAlpha((unsigned int)c)
#define isSPACE(c)   NXIsSpace((unsigned int)c)
#define isDIGIT(c)   NXIsDigit((unsigned int)c)
#define isUPPER(c)   NXIsUpper((unsigned int)c)
#define isLOWER(c)   NXIsLower((unsigned int)c)
#define toUPPER(c)   NXToUpper((unsigned int)c)
#define toLOWER(c)   NXToLower((unsigned int)c)
#else /* USE_NEXT_CTYPE */
#if defined(CTYPE256) || (!defined(isascii) && !defined(HAS_ISASCII))
#define isALNUM(c)   (isalpha((unsigned char)(c)) || isdigit((unsigned char)(c)) || c == '_')
#define isIDFIRST(c) (isalpha((unsigned char)(c)) || (c) == '_')
#define isALPHA(c)   isalpha((unsigned char)(c))
#define isSPACE(c)   isspace((unsigned char)(c))
#define isDIGIT(c)   isdigit((unsigned char)(c))
#define isUPPER(c)   isupper((unsigned char)(c))
#define isLOWER(c)   islower((unsigned char)(c))
#define toUPPER(c)   toupper((unsigned char)(c))
#define toLOWER(c)   tolower((unsigned char)(c))
#else
#define isALNUM(c)   (isascii(c) && (isalpha(c) || isdigit(c) || c == '_'))
#define isIDFIRST(c) (isascii(c) && (isalpha(c) || (c) == '_'))
#define isALPHA(c)   (isascii(c) && isalpha(c))
#define isSPACE(c)   (isascii(c) && isspace(c))
#define isDIGIT(c)   (isascii(c) && isdigit(c))
#define isUPPER(c)   (isascii(c) && isupper(c))
#define isLOWER(c)   (isascii(c) && islower(c))
#define toUPPER(c)   toupper(c)
#define toLOWER(c)   tolower(c)
#endif
#endif /* USE_NEXT_CTYPE */

/* Line numbers are unsigned, 16 bits. */
typedef U16 line_t;
#ifdef lint
#define NOLINE ((line_t)0)
#else
#define NOLINE ((line_t) 65535)
#endif

#ifndef lint
#ifndef LEAKTEST
#ifndef safemalloc
char *safemalloc _((MEM_SIZE));
char *saferealloc _((char *, MEM_SIZE));
void safefree _((char *));
#endif
#ifndef MSDOS
#define New(x,v,n,t)  (v = (t*)safemalloc((MEM_SIZE)((n) * sizeof(t))))
#define Newc(x,v,n,t,c)  (v = (c*)safemalloc((MEM_SIZE)((n) * sizeof(t))))
#define Newz(x,v,n,t) (v = (t*)safemalloc((MEM_SIZE)((n) * sizeof(t)))), \
    memzero((char*)(v), (n) * sizeof(t))
#define Renew(v,n,t) (v = (t*)saferealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Renewc(v,n,t,c) (v = (c*)saferealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#else
#define New(x,v,n,t)  (v = (t*)safemalloc(((unsigned long)(n) * sizeof(t))))
#define Newc(x,v,n,t,c)  (v = (c*)safemalloc(((unsigned long)(n) * sizeof(t))))
#define Newz(x,v,n,t) (v = (t*)safemalloc(((unsigned long)(n) * sizeof(t)))), \
    memzero((char*)(v), (n) * sizeof(t))
#define Renew(v,n,t) (v = (t*)saferealloc((char*)(v),((unsigned long)(n)*sizeof(t))))
#define Renewc(v,n,t,c) (v = (c*)saferealloc((char*)(v),((unsigned long)(n)*sizeof(t))))
#endif /* MSDOS */
#define Safefree(d) safefree((char*)d)
#define NEWSV(x,len) newSV(len)
#else /* LEAKTEST */
char *safexmalloc();
char *safexrealloc();
void safexfree();
#define New(x,v,n,t)  (v = (t*)safexmalloc(x,(MEM_SIZE)((n) * sizeof(t))))
#define Newc(x,v,n,t,c)  (v = (c*)safexmalloc(x,(MEM_SIZE)((n) * sizeof(t))))
#define Newz(x,v,n,t) (v = (t*)safexmalloc(x,(MEM_SIZE)((n) * sizeof(t)))), \
    memzero((char*)(v), (n) * sizeof(t))
#define Renew(v,n,t) (v = (t*)safexrealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Renewc(v,n,t,c) (v = (c*)safexrealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Safefree(d) safexfree((char*)d)
#define NEWSV(x,len) newSV(x,len)
#define MAXXCOUNT 1200
long xcount[MAXXCOUNT];
long lastxcount[MAXXCOUNT];
#endif /* LEAKTEST */
#define Move(s,d,n,t) (void)memmove((char*)(d),(char*)(s), (n) * sizeof(t))
#define Copy(s,d,n,t) (void)memcpy((char*)(d),(char*)(s), (n) * sizeof(t))
#define Zero(d,n,t) (void)memzero((char*)(d), (n) * sizeof(t))
#else /* lint */
#define New(x,v,n,s) (v = Null(s *))
#define Newc(x,v,n,s,c) (v = Null(s *))
#define Newz(x,v,n,s) (v = Null(s *))
#define Renew(v,n,s) (v = Null(s *))
#define Move(s,d,n,t)
#define Copy(s,d,n,t)
#define Zero(d,n,t)
#define Safefree(d) d = d
#endif /* lint */

#ifdef USE_STRUCT_COPY
#define StructCopy(s,d,t) *((t*)(d)) = *((t*)(s))
#else
#define StructCopy(s,d,t) Copy(s,d,1,t)
#endif
