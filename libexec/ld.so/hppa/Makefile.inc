#	$OpenBSD: Makefile.inc,v 1.4 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fpic
AFLAGS += -fpic

# extract components from libgcc.a
MDOBJ=	_dyncall.o _remU.o _remI.o _divI.o _divU.o _div_const.o
${MDOBJ}:
	ar x `$(CC) -print-libgcc-file-name` ${MDOBJ}
OBJS+=${MDOBJ}

# We have to deal with six styles of relocation:
# 000112ec  00000001 R_PARISC_DIR32                               00014000
# 00013f00  00000041 R_PARISC_PLABEL32                            00011262
# 00013f08  00000f41 R_PARISC_PLABEL32 00001ed8   _dl_debug_state + 0
# 00014120  00000201 R_PARISC_DIR32    0000ebd0   .rodata + 8ec
# 000112b0  00000081 R_PARISC_IPLT                                00009a28
# 00011268  00000c81 R_PARISC_IPLT     00000430   dlerror + 0

CHECK_LDSO=c() {							\
	! readelf -Wr $$1 |						\
	  egrep -qv -e '^($$|[ R])'					\
	     -e ' R_PARISC_DIR32 +[0-9a-f]+($| +\.[a-z])'		\
	     -e ' R_PARISC_(PLABEL32|IPLT) +[0-9a-f]+($| +_?dl)';	\
	}; c
