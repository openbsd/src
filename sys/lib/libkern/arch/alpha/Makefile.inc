#	$OpenBSD: Makefile.inc,v 1.4 1996/05/01 15:08:15 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.5 1996/04/17 22:46:34 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.c bzero.S ffs.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c \
	strncpy.c scanc.c skpc.c locc.c htonl.S htons.S ntohl.S ntohs.S \
	random.c strncasecmp.c

# `source' files built from m4 source
SRCS+=	__divqu.S __divq.S __divlu.S __divl.S
SRCS+=	__remqu.S __remq.S __remlu.S __reml.S
CLEANFILES+=	__divqu.S __divq.S __divlu.S __divl.S
CLEANFILES+=	__remqu.S __remq.S __remlu.S __reml.S

__divqu.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divqu')define(OP,\`div')define(S,\`false')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__divq.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divq')define(OP,\`div')define(S,\`true')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__divlu.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divlu')define(OP,\`div')define(S,\`false')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__divl.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__divl')define(OP,\`div')define(S,\`true')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__remqu.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__remqu')define(OP,\`rem')define(S,\`false')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__remq.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__remq')define(OP,\`rem')define(S,\`true')"; \
	 echo "define(WORDSIZE,64)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__remlu.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__remlu')define(OP,\`rem')define(S,\`false')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}

__reml.S: ${DIR}/arch/alpha/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`__reml')define(OP,\`rem')define(S,\`true')"; \
	 echo "define(WORDSIZE,32)"; cat ${.ALLSRC}) | m4 > ${.TARGET}
