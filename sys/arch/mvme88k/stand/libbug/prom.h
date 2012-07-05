/*	$OpenBSD: prom.h,v 1.4 2012/07/05 04:23:33 guenther Exp $	*/

#define MVMEPROM_CALL(x) \
	__asm__ __volatile__ ("or r9,r0," __STRING(x)); \
	__asm__ __volatile__ ("tb0 0,r0,496")
