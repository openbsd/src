# $Id: Makefile,v 1.1.1.1 1995/10/18 08:40:13 deraadt Exp $

# Paths to libraries
#
LIBBFD!=cd $(.CURDIR)/../bfd; \
	printf "xxx:\n\techo \$${.OBJDIR}/libbfd.a\n" | make -r -s -f - xxx

LIBIBERTY!=cd $(.CURDIR)/../libiberty; \
	printf "xxx:\n\techo \$${.OBJDIR}/libiberty.a\n" | make -r -s -f - xxx


PROG=		objdump

CFLAGS+=	-I$(.CURDIR)/arch/$(MACHINE_ARCH) -I$(.CURDIR)/../bfd \
		-I$(.CURDIR)/../include \
		-I$(.CURDIR)/../bfd/arch/$(MACHINE_ARCH) \
		-I$(.CURDIR)

CFLAGS+=	-DVERSION=\"2.0\"

.PATH:		$(.CURDIR)/arch/$(MACHINE_ARCH) \
		$(.CURDIR)/../gdb/arch/$(MACHINE_ARCH)

SRCS=		objdump.c bucomm.c filemode.c version.c

LDADD=		$(LIBBFD) $(LIBIBERTY)
DPADD=		$(LIBBFD) $(LIBIBERTY)

.include "$(.CURDIR)/arch/$(MACHINE_ARCH)/Makefile.inc"
.include "../../Makefile.inc"
.include <bsd.prog.mk>

