#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fp.c,v 1.1 1997/01/02 02:28:58 rahnds Exp $";
#endif /* LIBC_SCCS and not lint */
 
#include <ieeefp.h>
 
fp_except
fpgetmask()
{
	return 0; /* stub --- XXX */ 
}

fp_rnd
fpgetround()
{
	return 0; /* stub --- XXX */ 
}

fp_except
fpsetmask(mask)
	fp_except mask;
{
	return 0; /* stub --- XXX */ 
}

fp_rnd
fpsetround(rnd_dir)
	fp_rnd rnd_dir;
{
	return 0; /* stub --- XXX */ 
}

fp_except
fpsetsticky(sticky)
	fp_except sticky;
{
	return 0; /* stub --- XXX */ 
}

fp_except
fpgetsticky()
{
	return 0; /* stub --- XXX */ 
}
