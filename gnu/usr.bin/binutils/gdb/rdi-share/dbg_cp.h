/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * ARM symbolic debugger toolbox: dbg_cp.h
 */

/*
 * RCS $Revision: 1.3 $
 * Checkin $Date: 2004/12/27 14:00:54 $
 */

#ifndef Dbg_CP__h

#define Dbg_CP__h

#define Dbg_Access_Readable  1
#define Dbg_Access_Writable  2
#define Dbg_Access_CPDT      4 /* else CPRT */

typedef struct {
    unsigned short rmin, rmax;
    /* a single description can be used for a range of registers with
       the same properties *accessed via CPDT instructions*
     */
    unsigned char nbytes;   /* size of register */
    unsigned char access;   /* see above (Access_xxx) */
    union {
       struct { /* CPDT instructions do not allow the coprocessor much freedom:
                   only bit 22 ('N') and 12-15 ('CRd') are free for the
                   coprocessor to use as it sees fit.
                 */
                unsigned char nbit;
                unsigned char rdbits;
              } cpdt;
       struct { /* CPRT instructions have much more latitude.  The bits fixed
                   by the ARM are  24..31 (condition mask & opcode)
                                   20 (direction)
                                   8..15 (cpnum, arm register)
                                   4 (CPRT not CPDO)
                   leaving 14 bits free to the coprocessor (fortunately
                   falling within two bytes).
                 */
                unsigned char read_b0, read_b1,
                              write_b0, write_b1;
              } cprt;
    } accessinst;
} Dbg_CoProRegDesc;

struct Dbg_CoProDesc {
    int entries;
    Dbg_CoProRegDesc regdesc[1/* really nentries */];
};

#define Dbg_CoProDesc_Size(n) (sizeof(struct Dbg_CoProDesc) + ((n)-1)*sizeof(Dbg_CoProRegDesc))

#endif
