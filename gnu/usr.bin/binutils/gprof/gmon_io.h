#ifndef gmon_io_h
#define gmon_io_h

#include "bfd.h"
#include "gmon.h"

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
