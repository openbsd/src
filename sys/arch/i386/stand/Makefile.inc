#	$OpenBSD: Makefile.inc,v 1.21 1998/04/20 13:25:00 deraadt Exp $

CFLAGS=${DEBUG} -O2 -Wall -Werror
CPPFLAGS+=-I${S} -I${SADIR}/libsa -I. -I${.CURDIR}
SACFLAGS=-nostdinc -fno-builtin -fpack-struct
SACFLAGS+=-D_STANDALONE
# CPPFLAGS+=-DSAVE_MEMORY
# DEBUGFLAGS=-DDEBUG
# DEBUGFLAGS+=-DGIDT_DEBUG
# DEBUGFLAGS+=-DBIOS_DEBUG
# DEBUGFLAGS+=-DEXEC_DEBUG
# DEBUGFLAGS+=-DALLOC_TRACE
# DEBUGFLAGS+=-D_TEST
# DEBUGFLAGS+=-DUNIX_DEBUG
# DEBUGFLAGS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG
# DEBUGFLAGS+=-DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG
LINKADDR=0x10000
LOADADDR=0x10000
HEAP_LIMIT=0x60000
BOOTREL=0x60000
BOOTMAGIC=0xdeadbeef
#ROM_SIZE=32768

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
