# $Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:40:14 deraadt Exp $

SRCS+=		sparc-pinsn.c sparc-opc.c

CFLAGS+=	-DSELECT_ARCHITECTURES=bfd_arch_sparc \
		-DPRINT_INSN=print_insn_sparc


