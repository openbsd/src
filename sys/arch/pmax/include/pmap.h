/*	$NetBSD: pmap.h,v 1.10 1996/03/19 04:39:05 jonathan Exp $	*/

#include <mips/pmap.h>

#define pmax_trunc_seg(a) mips_trunc_seg(a)
#define pmax_round_seg(a) mips_round_seg(a)
