/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

/* returns 0 if no characters ready to read */
int
mvmeprom_instat()
{
	u_short ret;

	MVMEPROM_CALL(MVMEPROM_INSTAT);
	asm volatile ("movew ccr,%0": "=d" (ret));
	return (!(ret & 0x4));
}
