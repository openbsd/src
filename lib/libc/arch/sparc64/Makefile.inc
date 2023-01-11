#	$OpenBSD: Makefile.inc,v 1.5 2023/01/11 17:00:21 deraadt Exp $

LDADD+= -Wl,--no-execute-only

# `source' files built from m4 source
# the name `div.o' is taken for the ANSI C `div' function, hence sdiv here
SRCS+=	rem.S sdiv.S udiv.S urem.S
CLEANFILES+=rem.S sdiv.S udiv.S urem.S

sdiv.S: ${LIBCSRCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.div')define(OP,\`div')define(S,\`true')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	@chmod 444 ${.TARGET}

udiv.S: ${LIBCSRCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.udiv')define(OP,\`div')define(S,\`false')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	@chmod 444 ${.TARGET}

rem.S: ${LIBCSRCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.rem')define(OP,\`rem')define(S,\`true')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	@chmod 444 ${.TARGET}

urem.S: ${LIBCSRCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.urem')define(OP,\`rem')define(S,\`false')"; \
	 cat ${.ALLSRC}) | m4 > ${.TARGET}
	@chmod 444 ${.TARGET}

.include "${.CURDIR}/arch/sparc64/fpu/Makefile.inc"
