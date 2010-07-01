# $OpenBSD: Makefile.inc,v 1.2 2010/07/01 03:38:17 yasuoka Exp $
# $Id: Makefile.inc,v 1.2 2010/07/01 03:38:17 yasuoka Exp $

.include <bsd.own.mk>

.if exists(${.CURDIR}/../../Makefile.inc)
.include "../../Makefile.inc"
.endif
