# $Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:39:55 deraadt Exp $

SRCS+=		netbsdvax.c cpu-vax.c

CFLAGS+=	-DNETBSD_CORE

VECTORS=	-DDEFAULT_VECTOR=netbsd_vax_vec \
		-DSELECT_ARCHITECTURES=bfd_vax_arch \
		-DSELECT_VECS='&netbsd_vax_vec'


