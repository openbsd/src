#ifndef __types_loaded__
#define __types_loaded__ 1

#include <stddef.h>

/*
 * Miscellaneous VMS types that are not normally defined
 * in any consistent fashion.
 */

/* VMS I/O status block */
struct IOSB
{
  short status, count;
  long devinfo;
};

/* VMS Item List 3 structure */
struct itm$list3
{
  short buflen;
  short itemcode;
  void *buffer;
  size_t *retlen;
};

/* VMS Lock status block with value block */
struct LOCK
{
  short status, reserved;
  long lockid;
  long value[4];
};

/* VMS Exit Handler Control block */
struct EXHCB
{
  struct exhcb *exh$a_link;
  int (*exh$a_routine)();
  long exh$l_argcount;
  long *exh$a_status;
  long exh$l_status;
};

#endif /* __types_loaded__ 1 */
