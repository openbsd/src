#	$OpenBSD: htons.S,v 1.4 1997/11/07 15:57:20 niklas Exp $

#include "DEFS.h"


/*
 * netorder = htons(hostorder)
 * hostorder = ntohs(netorder)
 */
LEAF(htons)
ALEAF(ntohs)
	.set	noreorder
	srl	v0, a0, 8
	and	v0, v0, 0xff
	sll	v1, a0, 8
	and	v1, v1, 0xff00
	j	ra
	 or	v0, v0, v1
END(htons)
