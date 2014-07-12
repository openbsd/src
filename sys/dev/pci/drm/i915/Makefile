#	$OpenBSD: Makefile,v 1.1 2014/07/12 23:16:23 jsg Exp $

AWK=	awk

i915_devlist.h: i915_drv.c devlist.awk
	/bin/rm -f i915_devlist.h
	${AWK} -f devlist.awk i915_drv.c > i915_devlist.h
