#	$OpenBSD: Makefile,v 1.13 2010/01/10 03:37:50 guenther Exp $
#	$NetBSD: Makefile,v 1.11 1996/03/18 22:33:07 thorpej Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	kvm
WANTLINT=

# Try most specific name first.
.if	exists(kvm_${MACHINE}.c)
SRCS=	kvm_${MACHINE}.c
.else
# Less specific name (i.e. m68k)
CFLAGS+=-D${MACHINE}
SRCS=	kvm_${MACHINE_ARCH}.c
.endif

CFLAGS+= -D_LIBKVM
SRCS+=	kvm.c kvm_file.c kvm_file2.c kvm_getloadavg.c kvm_proc.c kvm_proc2.c \
	kvm_cd9660.c kvm_udf.c kvm_ntfs.c

MAN=	kvm.3 kvm_dump.3 kvm_geterr.3 kvm_getfiles.3 kvm_getloadavg.3 \
	kvm_getprocs.3 kvm_nlist.3 kvm_open.3 kvm_read.3

MLINKS+=kvm_getprocs.3 kvm_getargv.3 kvm_getprocs.3 kvm_getenvv.3
MLINKS+=kvm_getprocs.3 kvm_getproc2.3
MLINKS+=kvm_getprocs.3 kvm_getargv2.3 kvm_getprocs.3 kvm_getenvv2.3
MLINKS+=kvm_getfiles.3 kvm_getfile2.3
MLINKS+=kvm_open.3 kvm_openfiles.3 kvm_open.3 kvm_close.3
MLINKS+=kvm_read.3 kvm_write.3
MLINKS+=kvm_dump.3 kvm_dump_mkheader.3 kvm_dump.3 kvm_dump_wrtheader.3
MLINKS+=kvm_dump.3 kvm_dump_inval.3

.include <bsd.lib.mk>
