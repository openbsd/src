#	$OpenBSD: Makefile.inc,v 1.10 1997/06/30 21:11:18 kstailey Exp $

#CPPFLAGS+=-nostdinc -I/sys
CPPFLAGS+=-Wall -Werror -I. -Imachine
SACFLAGS+=-Wa,-R -fpack-struct -fno-builtin -fomit-frame-pointer
CPPFLAGS+=-D_STANDALONE
# CPPFLAGS+=-DSAVE_MEMORY
# CPPFLAGS+=-DDEBUG
# CPPFLAGS+=-DGIDT_DEBUG
# CPPFLAGS+=-DBIOS_DEBUG
# CPPFLAGS+=-DEXEC_DEBUG
# CPPFLAGS+=-DALLOC_TRACE
# CPPFLAFS+=-D_TEST
# CPPFLAFS+=-DUNIX_DEBUG
# CPPFLAFS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG
# CPPFLAGS+=-DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG
START=0x10000
HEAP_LIMIT=0x50000
BOOTREL=0x3000
BOOTMAGIC=0xdeadbeef

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

