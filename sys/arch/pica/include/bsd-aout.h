/* bsd-aout.h

   4.4bsd a.out format, for backwards compatibility...  */

#ifndef __MACHINE_BSD_AOUT_H__
#define __MACHINE_BSD_AOUT_H__
#define BSD_OMAGIC  0407            /* old impure format */
#define BSD_NMAGIC  0410            /* read-only text */
#define BSD_ZMAGIC  0413            /* demand load format */

struct bsd_aouthdr {
#if BYTE_ORDER == BIG_ENDIAN
  u_short a_mid;          /* machine ID */
  u_short a_magic;        /* magic number */
#else
  u_short a_magic;        /* magic number */
  u_short a_mid;          /* machine ID */
#endif
 
  u_long  a_text;         /* text segment size */
  u_long  a_data;         /* initialized data size */
  u_long  a_bss;          /* uninitialized data size */
  u_long  a_syms;         /* symbol table size */
  u_long  a_entry;        /* entry point */
  u_long  a_trsize;       /* text relocation size */
  u_long  a_drsize;       /* data relocation size */
};

#ifndef _KERNEL
#define _AOUT_INCLUDE_
#include <nlist.h>
#endif /* _KERNEL */
#endif /* __MACHINE_BSD_AOUT_H__ */
