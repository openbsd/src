/*	$NetBSD: swab.S,v 1.4 1995/11/17 20:21:28 thorpej Exp $	*/

#include "DEFS.h"

ENTRY(swab)
	movl	sp@(4),a0		| source
	movl	sp@(8),a1		| destination
	movl	sp@(12),d0		| count
	lsrl	#1,d0			| count is in bytes; we need words
	beq	swdone

swloop:
	movw	a0@+,d1
	rorw	#8,d1
	movw	d1,a1@+
	subql	#1,d0
	bne	swloop

swdone:
	rts
