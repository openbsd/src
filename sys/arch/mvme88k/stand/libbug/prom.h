/*	$OpenBSD: prom.h,v 1.3 2003/09/07 21:35:35 miod Exp $	*/

#define MVMEPROM_CALL(x) \
	__asm__ __volatile__ (__CONCAT("or r9,r0,", __STRING(x))); \
	__asm__ __volatile__ ("tb0 0,r0,496")
