/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

/* returns 0: success, nonzero: error */
int
mvmeprom_diskwr(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	asm volatile ("movel %0, sp@-"::"d" (arg));
	MVMEPROM_CALL(MVMEPROM_DSKWR);
	asm volatile ("movew ccr,%0": "=d" (ret));
	return (!(ret & 0x4));
}
