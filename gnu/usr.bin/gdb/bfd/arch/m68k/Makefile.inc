# $Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:39:54 deraadt Exp $

SRCS+=		netbsdm68k.c netbsdm68k4k.c cpu-m68k.c trad-core.c

# Note: DEFAULT_VECTOR is actually ignored when SELECT_VECS is set.
VECTORS=	-DDEFAULT_VECTOR=netbsd_m68k_vec \
		-DSELECT_ARCHITECTURES=bfd_m68k_arch \
		-DSELECT_VECS=' &netbsd_m68k_vec, &netbsd_m68k4k_vec, \
				&netbsd_core_vec, &trad_core_vec '

