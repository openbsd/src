# $Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:39:54 deraadt Exp $

SRCS+=		netbsd386.c cpu-i386.c

CFLAGS+=	-DNETBSD_CORE

VECTORS=	-DDEFAULT_VECTOR=netbsd_386_vec \
		-DSELECT_ARCHITECTURES=bfd_i386_arch \
		-DSELECT_VECS='&netbsd_386_vec'


