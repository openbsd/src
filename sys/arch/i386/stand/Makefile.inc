#	$OpenBSD: Makefile.inc,v 1.8 1997/04/18 03:17:29 mickey Exp $

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

.if exists(${SADIR}/libsa/obj)
LIBSA=${SADIR}/libsa/obj/libsa.a
.else
LIBSA=${SADIR}/libsa/libsa.a
.endif
.if exists(${SADIR}/libz/obj)
LIBZ=${SADIR}/libz/obj/libz.a
.else
LIBZ=${SADIR}/libz/libz.a
.endif

# NO_NET=no_net
BINDIR=	/usr/mdec

