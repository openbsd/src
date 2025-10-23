#	$OpenBSD: Makefile,v 1.67 2025/10/23 20:42:52 miod Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>

.if make(obj)
SUBDIR+=	cc clang
.else
.  if ${BUILD_GCC4:L} == "yes"
SUBDIR+=	cc
.  endif
.  if ${BUILD_CLANG:L} == "yes"
SUBDIR+=	clang
.  endif
.endif

SUBDIR+=	cvs
# binutils-2.17 needs to build gdb in binutils at the moment
.if make(obj)
SUBDIR+=	binutils
.elif ${MACHINE_CPU} != "aarch64" && ${MACHINE_CPU} != "powerpc64" && \
      ${MACHINE_CPU} != "riscv64"
SUBDIR+=	binutils
.endif
SUBDIR+=	binutils-2.17

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
