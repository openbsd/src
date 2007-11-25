#	$OpenBSD: Makefile,v 1.1 2007/11/25 18:25:36 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.12 1996/04/23 23:05:22 christos Exp $

all:	rem.S sdiv.S udiv.S urem.S

sdiv.S: divrem.m4
	echo 'building ${.TARGET} from ${.ALLSRC}'
	(echo "define(NAME,\`.div')define(NAME2,\`_C_LABEL(_div)')define(OP,\`div')define(S,\`true')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	chmod 444 ${.TARGET}

udiv.S: divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.udiv')define(NAME2,\`_C_LABEL(_udiv)')define(OP,\`div')define(S,\`false')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	@chmod 444 ${.TARGET}

rem.S:	divrem.m4
	echo 'building ${.TARGET} from ${.ALLSRC}'
	(echo "define(NAME,\`.rem')define(NAME2,\`_C_LABEL(_rem)')define(OP,\`rem')define(S,\`true')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	chmod 444 ${.TARGET}

urem.S: divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.urem')define(NAME2,\`_C_LABEL(_urem)')define(OP,\`rem')define(S,\`false')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	@chmod 444 ${.TARGET}

.include <bsd.own.mk>
