#	$OpenBSD: Makefile,v 1.64 2022/10/28 15:50:55 miod Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>

.if make(obj)
SUBDIR+=	cc clang gcc
.else
.  if ${BUILD_GCC3:L} == "yes"
SUBDIR+=	gcc
.  endif
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

maninstall:
	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		gcc/protoize.1 ${DESTDIR}${MANDIR}1/protoize.1
	@rm -f ${DESTDIR}${MANDIR}1/unprotoize.1
	ln ${DESTDIR}${MANDIR}1/protoize.1 ${DESTDIR}${MANDIR}1/unprotoize.1

.include <bsd.subdir.mk>
