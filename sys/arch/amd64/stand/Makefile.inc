#	$OpenBSD: Makefile.inc,v 1.10 2013/12/28 02:51:06 deraadt Exp $

CFLAGS=${DEBUG} ${COPTS} -Os -Wall -Werror
CFLAGS+=	-fno-stack-protector -DMDRANDOM
CDIAGFLAGS=
CPPFLAGS+=-I${S} -I${SADIR}/libsa -I. -I${.CURDIR}
SACFLAGS=-D_STANDALONE
DEBUGLIBS=no
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
LINKADDR=0x40120
LOADADDR=0x40000
HEAP_LIMIT=0x90000
BOOTREL=0x60000
BOOTMAGIC=0xc001d00d
#ROM_SIZE=32768
CLEANFILES+=	assym.h machine

.if empty(DEBUGFLAGS:M-D_TEST)
SACFLAGS+=-nostdinc -fno-builtin -fpack-struct
.endif

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ X$(S) = X -o -h machine ] || ln -s $(S)/arch/amd64/include machine)

assym.h: ${S}/kern/genassym.sh ${SADIR}/etc/genassym.cf
	sh ${S}/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} < ${SADIR}/etc/genassym.cf > assym.h.tmp && \
	    mv -f assym.h.tmp assym.h
.endif

# NO_NET=no_net
BINDIR=	/usr/mdec

MANSUBDIR=amd64
