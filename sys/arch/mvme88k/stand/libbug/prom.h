/*	$OpenBSD: prom.h,v 1.2 2001/07/04 08:09:28 niklas Exp $	*/

#define MVMEPROM_CALL(x)	\
	asm volatile ( __CONCAT("or r9,r0," __STRING(x)) ); \
	asm volatile ("tb0 0,r0,496");
