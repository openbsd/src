#	$OpenBSD: Makefile.inc,v 1.10 2002/12/08 17:08:32 mickey Exp $

CFLAGS=${DEBUG} -Os -Wall -Werror
CPPFLAGS+=-I${S} -I. -I${.CURDIR}
SACFLAGS=-nostdinc -fno-builtin -D_STANDALONE -I${SADIR}/libsa
SACFLAGS+=-mdisable-fpregs -mfast-indirect-calls -mpa-risc-1-0
SACFLAGS+=-fno-stack-protector
#DEBUGFLAGS=-DDEBUG
#DEBUGFLAGS+=-DPDCDEBUG
#DEBUGFLAGS+=-DLIFDEBUG
#DEBUGFLAGS+=-DEXEC_DEBUG
#DEBUGFLAGS+=-DALLOC_TRACE
LINKADDR=0x700000
LOADADDR=0x700000
HEAP_LIMIT=0x7c0000
CLEANFILES+=	machine
DEBUGLIBS=	no

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj) && !(defined(PROG) && ${PROG} == "mkboot")
.BEGIN:
	@([ -h machine ] || ln -s $(S)/arch/hppa/include machine)
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

BINDIR=	/usr/mdec
