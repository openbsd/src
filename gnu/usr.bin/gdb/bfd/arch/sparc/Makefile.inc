# $Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:39:55 deraadt Exp $

SRCS+=		netbsd_sparc.c cpu-sparc.c

CFLAGS+=	-DNETBSD_CORE

VECTORS=	-DDEFAULT_VECTOR=netbsd_sparc_vec \
		-DSELECT_ARCHITECTURES=bfd_sparc_arch \
		-DSELECT_VECS='&netbsd_sparc_vec'


