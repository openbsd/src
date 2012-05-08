#	$OpenBSD: Makefile.inc,v 1.3 2012/05/08 13:15:11 yasuoka Exp $
# $Id: Makefile.inc,v 1.3 2012/05/08 13:15:11 yasuoka Exp $

.include <bsd.own.mk>

.if exists(${.CURDIR}/../../Makefile.inc)
.include "../../Makefile.inc"
.endif
