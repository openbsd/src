#	$OpenBSD: Makefile.inc,v 1.27 2000/02/08 17:48:53 mickey Exp $

CFLAGS=${DEBUG} -Os -Wall -Werror
CPPFLAGS+=-I${S} -I${SADIR}/libsa -I. -I${.CURDIR}
SACFLAGS=-D_STANDALONE
DEBUGFLAGS=
# DEBUGFLAGS+=-DDEBUG
# DEBUGFLAGS+=-DGIDT_DEBUG
# DEBUGFLAGS+=-DBIOS_DEBUG
# DEBUGFLAGS+=-DEXEC_DEBUG
# DEBUGFLAGS+=-DALLOC_TRACE
# DEBUGFLAGS+=-g -D_TEST
# DEBUGFLAGS+=-DUNIX_DEBUG
# DEBUGFLAGS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG
# DEBUGFLAGS+=-DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG
LINKADDR=0x40000
LOADADDR=0x40000
HEAP_LIMIT=0x90000
BOOTREL=0x60000
BOOTMAGIC=0xc001d00d
#ROM_SIZE=32768
CLEANFILES+=	machine

.if empty(DEBUGFLAGS:M-D_TEST)
SACFLAGS+=-nostdinc -fno-builtin -fpack-struct
.endif

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s $(S)/arch/i386/include machine)
.endif


.if exists(${SADIR}/etc/assym.h)
CPPFLAGS+=-I${SADIR}/etc
.else
CPPFLAGS+=-I${SADIR}/etc/${__objdir}
.endif
.if exists(${SADIR}/libsa/libsa.a)
LIBSA=${SADIR}/libsa/libsa.a
.else
LIBSA=${SADIR}/libsa/${__objdir}/libsa.a
.endif
.if exists(${SADIR}/libz/libz.a)
LIBZ=${SADIR}/libz/libz.a
.else
LIBZ=${SADIR}/libz/${__objdir}/libz.a
.endif

# NO_NET=no_net
BINDIR=	/usr/mdec
