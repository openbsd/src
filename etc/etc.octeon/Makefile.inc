#	$OpenBSD: Makefile.inc,v 1.5 2019/07/28 15:45:30 visa Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	# None yet.

MDEXT=	boot bsd.rd miniroot${OSrev}.fs
