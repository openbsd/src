# $Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:40:14 deraadt Exp $

SRCS+=		i386-pinsn.c

CFLAGS+=	-DSELECT_ARCHITECTURES=bfd_arch_i386 \
		-DPRINT_INSN=print_insn_i386


