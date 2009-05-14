#	$OpenBSD: Makefile32.inc,v 1.1 2009/05/14 18:57:41 miod Exp $

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
AS=		as -32
LD=		ld -m elf32btsmip
LIBSA_CPPFLAGS=
.endif

### Figure out what to use for libsa and libz
LIBSADIR=               ${.CURDIR}/../libsa32
LIBZDIR=                ${.CURDIR}/../libz32

.endif
