#	$Id: Makefile,v 1.2 1995/12/14 03:31:36 deraadt Exp $

.if make(clean) || make(cleandir) || make(depend) || make(obj) || make(lint)
SUBDIR += tools
.endif
SUBDIR += tn3270 mset

.include <bsd.subdir.mk>
