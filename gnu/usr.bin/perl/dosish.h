#define ABORT() abort();

#define BIT_BUCKET "\dev\nul"
#define PERL_SYS_INIT(c,v)
#define PERL_SYS_TERM()
#define dXSUB_SYS int dummy
#define TMPPATH "plXXXXXX"

/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 fwrite

#define Stat(fname,bufptr) stat((fname),(bufptr))
#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#define Fflush(fp)         fflush(fp)

#define my_getenv(var)  getenv(var)
