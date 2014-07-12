#	$OpenBSD: Makefile,v 1.1 2014/07/12 23:16:23 jsg Exp $

AWK=	awk

radeon_devlist.h: ../drm_pciids.h devlist.awk
	/bin/rm -f radeon_devlist.h
	${AWK} -f devlist.awk ../drm_pciids.h > radeon_devlist.h
