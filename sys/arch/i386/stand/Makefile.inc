#	$OpenBSD: Makefile.inc,v 1.46 2015/09/18 13:30:56 miod Exp $

CFLAGS=${DEBUG} ${COPTS} -Os -Wall -Werror
CFLAGS+=	-fno-stack-protector -DMDRANDOM
CPPFLAGS+=-I${S} -I${SADIR}/libsa -I. -I${.CURDIR}
SACFLAGS=-D_STANDALONE
DEBUGLIBS=no
DEBUGFLAGS=
# DEBUGFLAGS+=-DDEBUG
# DEBUGFLAGS+=-DGIDT_DEBUG
# DEBUGFLAGS+=-DBIOS_DEBUG
# DEBUGFLAGS+=-DEXEC_DEBUG
# DEBUGFLAGS+=-DALLOC_TRACE
# DEBUGFLAGS+=-DUNIX_DEBUG
# DEBUGFLAGS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG
# DEBUGFLAGS+=-DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG
LINKADDR=0x40120
LOADADDR=0x40000
HEAP_LIMIT=0x90000
BOOTREL=0x60000
BOOTMAGIC=0xc001d00d
#ROM_SIZE=32768
CLEANFILES+=	assym.h machine

SACFLAGS+=-nostdinc -fno-builtin -fpack-struct

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ X$(S) = X -o -h machine ] || ln -s $(S)/arch/i386/include machine)

assym.h: ${S}/kern/genassym.sh ${SADIR}/etc/genassym.cf
	sh ${S}/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} < ${SADIR}/etc/genassym.cf > assym.h.tmp && \
	    mv -f assym.h.tmp assym.h
.endif

# NO_NET=no_net
BINDIR=	/usr/mdec

MANSUBDIR=i386
