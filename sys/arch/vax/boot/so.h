/*	$NetBSD: so.h,v 1.1 1996/08/02 11:22:41 ragge Exp $	*/

#ifndef _SO_H_INCLUDE
#define _SO_H_INCLUDE

/* Definitions for standalone I/O lib */

/* #define SCSI_POLLED	0x200C0080 */
#define SCSI_DMA	0x200D0000

/* Which SCSI device to use by default */
#define DEFAULT_SCSI_ADR	1
#define DEFAULT_SCSI_LUN	0

/* Low level scsi operation codes */
#define DISK_READ	3
#define DISK_WRITE	4

/* The size of a disk block */
#define DBLKSIZE	512

/* Some disk address that will never be used */
#define INSANE_BADDR	0x800000

struct scsi_args {
  long ptr [8];
};

#ifndef NULL
#define NULL		0L
#endif

/*
 * The next macro defines where the "break" area in memory ends for
 * malloc() and friends. The area between edata and this address will
 * then be reserved and should not be used for anything else (or you will
 * no doubt have big problems). Depending on where your program's end-of-data
 * is, you may wish to locate this in such a way as to usurp a minimum
 * amount of memory.
 */
#define BREAK_END_ADDR		((char *)0x400000)	/* to   4MB */

/* Selectivly enable inline functions */
#ifndef NO_INLINE
#define Inline	inline
#else
#define Inline
#endif
 
extern void fatal(), warn();
extern long ulimit(int, long);
extern int brk(char *);
extern char *sbrk(int);

extern int sc_rdwt();

#endif /* _SO_H_INCLUDE */
