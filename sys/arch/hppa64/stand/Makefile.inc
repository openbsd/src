#	$OpenBSD: Makefile.inc,v 1.2 2005/12/30 02:03:48 millert Exp $

CFLAGS=${DEBUG} -Os -Wall -Werror
CPPFLAGS+=-I${S} -I. -I${.CURDIR}
SACFLAGS=-nostdinc -fno-builtin -D_STANDALONE -I${STANDIR}/libsa
SACFLAGS+=-mdisable-fpregs -fno-stack-protector
#DEBUGFLAGS=-DDEBUG
#DEBUGFLAGS+=-DPDCDEBUG
#DEBUGFLAGS+=-DLIFDEBUG
#DEBUGFLAGS+=-DEXEC_DEBUG
#DEBUGFLAGS+=-DALLOC_TRACE
LINKADDR=0x780000
LOADADDR=0x780000
HEAP_LIMIT=0x7f8000
CLEANFILES+=	machine
DEBUGLIBS=	no

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj) && !(defined(PROG) && ${PROG} == "mkboot")
.BEGIN:
	@([ -h machine ] || ln -s $(S)/arch/hppa64/include machine)
.endif

.if exists(${STANDIR}/libsa/libsa.a)
LIBSA=${STANDIR}/libsa/libsa.a
.else
LIBSA=${STANDIR}/libsa/${__objdir}/libsa.a
.endif
.if exists(${STANDIR}/libkern/libkern.a)
LIBKERN=${STANDIR}/libkern/libkern.a
.else
LIBKERN=${STANDIR}/libkern/${__objdir}/libkern.a
.endif
.if exists(${STANDIR}/libz/libz.a)
LIBZ=${STANDIR}/libz/libz.a
.else
LIBZ=${STANDIR}/libz/${__objdir}/libz.a
.endif
.if exists(${STANDIR}/mkboot/mkboot)
MKBOOT=${STANDIR}/mkboot/mkboot
.else
MKBOOT=${STANDIR}/mkboot/${__objdir}/mkboot
.endif

BINDIR=	/usr/mdec
