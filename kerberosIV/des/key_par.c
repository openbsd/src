/*	$Id: key_par.c,v 1.1.1.1 1995/12/14 06:52:44 tholo Exp $	*/

#include "des_locl.h"

/* MIT Link and source compatibility */

#ifdef des_fixup_key_parity
#undef des_fixup_key_parity
#endif /* des_fixup_key_parity */

void
des_fixup_key_parity(des_cblock *key)
{
  des_set_odd_parity(key);
}
