#	$OpenBSD: Makefile.inc,v 1.10 2002/08/11 12:21:33 art Exp $
#	$NetBSD: Makefile.inc,v 1.12 1996/04/23 23:05:22 christos Exp $

SRCS+=  __main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c memcmp.c memset.c \
	bcmp.c bzero.S strcat.c strcmp.c strcpy.c strlcat.c strlcpy.c \
	strlen.S strncmp.c \
	strncpy.c htonl.S htons.S ntohl.S ntohs.S scanc.c skpc.c \
	strncasecmp.c

SRCS+=	rem.S sdiv.S udiv.S urem.S
SRCS+=	saveregs.S

# `source' files built from m4 source
# the name `div.o' is taken for the ANSI C `div' function, hence sdiv here
SRCS+=	rem.S sdiv.S udiv.S urem.S
CLEANFILES+=rem.S sdiv.S udiv.S urem.S

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

