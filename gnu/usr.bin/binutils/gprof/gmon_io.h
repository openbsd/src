#ifndef gmon_io_h
#define gmon_io_h

#include "bfd.h"
#include "gmon.h"

/* Some platforms need to put stdin into binary mode, to read
   binary files.  */
#include "sysdep.h"
#ifdef HAVE_SETMODE
#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#define setmode _setmode
#else
#define O_BINARY 0
#endif
#endif
#if O_BINARY
#include <io.h>
#define SET_BINARY(f) do { if (!isatty(f)) setmode(f,O_BINARY); } while (0)
#endif
#endif

#define INPUT_HISTOGRAM		(1<<0)
#define INPUT_CALL_GRAPH	(1<<1)
#define INPUT_BB_COUNTS		(1<<2)

extern int gmon_input;		/* what input did we see? */
extern int gmon_file_version;	/* file version are we dealing with */

extern bfd_vma get_vma PARAMS ((bfd * abfd, bfd_byte * addr));
extern void put_vma PARAMS ((bfd * abfd, bfd_vma val, bfd_byte * addr));

extern void gmon_out_read PARAMS ((const char *filename));
extern void gmon_out_write PARAMS ((const char *filename));

#endif /* gmon_io_h */
