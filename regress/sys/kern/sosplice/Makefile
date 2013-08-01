#	$OpenBSD: Makefile,v 1.3 2013/08/01 21:26:30 kettenis Exp $

SUBDIR +=	perf

.if defined(REGRESS_FULL)
SUBDIR +=	error
SUBDIR +=	tcp
SUBDIR +=	udp
.endif

.include <bsd.subdir.mk>
