#	$OpenBSD: Makefile.inc,v 1.2 1998/06/27 02:59:20 mickey Exp $

CFLAGS=${DEBUG} -O2 -Wall -Werror
CPPFLAGS+=-I${S} -I. -I${.CURDIR}
SACFLAGS=-nostdinc -fno-builtin
SACFLAGS+=-mdisable-fpregs -mfast-indirect-calls -mpa-risc-1-0
SACFLAGS+=-D_STANDALONE -I${SADIR}/libsa
DEBUGFLAGS=-DDEBUG
# DEBUGFLAGS+=-DEXEC_DEBUG
# DEBUGFLAGS+=-DALLOC_TRACE
# DEBUGFLAGS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG
# DEBUGFLAGS+=-DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG
LINKADDR=0x500000
LOADADDR=0x500000
HEAP_LIMIT=0x600000
#ROM_SIZE=32768
CLEANFILES+=	machine

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj) && !(defined(PROG) && ${PROG} == "mkboot")
.BEGIN:
	@([ -h machine ] || ln -s $(S)/arch/hppa/include machine)
.endif

.if 0
.if $(CC) != $(HOSTCC)
.S.o .s.o:
	@echo "${CPP} ${CPPFLAGS} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	${AS} -o ${.TARGET}"
	@${CPP} ${CPPFLAGS} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	${AS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.c.o:
	${CC} -S ${CFLAGS} ${CPPFLAGS} ${CFLAGS:M-[ID]*} ${.IMPSRC}
	${AS} $*.s -o ${.TARGET}.o
	@rm $*.s
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
.endif
.endif
.if exists(${SADIR}/libsa/libsa.a)
LIBSA=${SADIR}/libsa/libsa.a
.else
LIBSA=${SADIR}/libsa/${__objdir}/libsa.a
.endif
.if exists(${SADIR}/libkern/libkern.a)
LIBKERN=${SADIR}/libkern/libkern.a
.else
LIBKERN=${SADIR}/libkern/${__objdir}/libkern.a
.endif
.if exists(${SADIR}/libz/libz.a)
LIBZ=${SADIR}/libz/libz.a
.else
LIBZ=${SADIR}/libz/${__objdir}/libz.a
.endif
.if exists(${SADIR}/mkboot/mkboot)
MKBOOT=${SADIR}/mkboot/mkboot
.else
MKBOOT=${SADIR}/mkboot/${__objdir}/mkboot
.endif

# NO_NET=no_net
BINDIR=	/usr/mdec
