#	$OpenBSD: Makefile,v 1.4 2013/08/01 21:26:30 kettenis Exp $

SUBDIR+= mtree

.if defined(REGRESS_FULL)
SUBDIR += pkg_add relayd
.endif

.include <bsd.subdir.mk>
