#	$OpenBSD: Makefile32.inc,v 1.3 2009/10/31 20:36:14 miod Exp $

.ifndef __INCLUDED_STAND_MAKEFILE32_INC
__INCLUDED_STAND_MAKEFILE32_INC=

.if ${MACHINE} == "sgi"
# Silence warnings
CFLAGS+=	-fno-builtin-snprintf
CFLAGS+=	-fno-builtin-memcpy
CFLAGS+=	-fno-builtin-memcmp
CFLAGS+=	-fno-builtin-memset
CFLAGS+=	-fno-builtin-strncpy
CFLAGS+=	-fno-builtin-strncmp
# Override toolchain options to force 32 bit binaries
SAABI=		-mabi=32 -mips2 -Wa,-G0
AS?=		as
AS+=		-32
LD?=		ld
LD+=		-m elf32btsmip
LIBSA_CPPFLAGS=
.endif

### Figure out what to use for libsa and libz
LIBSADIR=               ${.CURDIR}/../libsa32
LIBZDIR=                ${.CURDIR}/../libz32

.endif
