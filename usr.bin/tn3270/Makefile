#	$OpenBSD: Makefile,v 1.3 1996/06/26 05:40:58 deraadt Exp $

.if make(clean) || make(cleandir) || make(depend) || make(obj) || make(lint)
SUBDIR += tools
.endif
SUBDIR += tn3270 mset

.include <bsd.subdir.mk>
