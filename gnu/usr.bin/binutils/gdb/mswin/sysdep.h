#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#if __STDC__ && defined(_MSC_VER)
/* stats.h defines _stat but not stat if __STDC__ */
#define fstat    _fstat
#define stat     _stat
#define S_IFMT   _S_IFMT
#define S_IFDIR  _S_IFDIR
#define S_IFCHR  _S_IFCHR
#define S_IFREG  _S_IFREG
#define S_IREAD  _S_IREAD
#define S_IWRITE _S_IWRITE
#define S_IEXEC  _S_IEXEC
#endif

#include <ctype.h>
#include <string.h>
#if __STDC__ && defined(_MSC_VER)
#define strnicmp _strnicmp
#define strdup _strdup
#endif
#include <sys/stat.h>

/*#include <sys/file.h>*/
#include "../../bfd/sysdep.h"

#ifndef	O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif
#define SEEK_SET 0
#define SEEK_CUR 1
#define __ALMOST_STDC__
#define NO_FCNTL

#include "fopen-bin.h"

#define SIGQUIT 5
#define SIGTRAP 6
#define SIGHUP  7



/* Used to set up bfd/targets.c and bfd/archures.c */
#include "bfdtarget.h"

