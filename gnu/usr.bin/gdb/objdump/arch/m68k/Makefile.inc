# $Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:40:14 deraadt Exp $

SRCS+=		m68k-pinsn.c

CFLAGS+=	-DSELECT_ARCHITECTURES=bfd_arch_m68k \
		-DPRINT_INSN=print_insn_m68k


