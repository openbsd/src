#	$OpenBSD: Makefile.inc,v 1.8 1997/11/04 19:08:01 chuck Exp $
#	$NetBSD: Makefile.inc,v 1.9 1996/08/27 00:44:24 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c \
	bcmp.c bzero.S ffs.S strcat.c strcmp.c strcpy.c strlen.c strncmp.c \
	strncpy.c scanc.c skpc.c htonl.S htons.S ntohl.S ntohs.S \
	random.c strncasecmp.c

# `source' files built from m4 source
SRCS+=	__divqu.S __divq.S __divlu.S __divl.S
SRCS+=	__remqu.S __remq.S __remlu.S __reml.S
CLEANFILES+=	__divqu.S __divq.S __divlu.S __divl.S
CLEANFILES+=	__remqu.S __remq.S __remlu.S __reml.S

__divqu.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divqu')define(OP,\`div')define(S,\`false')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__divq.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divq')define(OP,\`div')define(S,\`true')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__divlu.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divlu')define(OP,\`div')define(S,\`false')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__divl.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divl')define(OP,\`div')define(S,\`true')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__remqu.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__remqu')define(OP,\`rem')define(S,\`false')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__remq.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__remq')define(OP,\`rem')define(S,\`true')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__remlu.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__remlu')define(OP,\`rem')define(S,\`false')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__reml.S: ${M}/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__reml')define(OP,\`rem')define(S,\`true')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}
