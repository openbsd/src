# $Id: Makefile,v 1.2 1995/12/14 03:42:41 deraadt Exp $

# Paths to libraries
#
LIBBFD!=cd $(.CURDIR)/../bfd; \
	printf "xxx:\n\techo \$${.OBJDIR}/libbfd.a\n" | \
	${MAKE} -r -s -f - xxx | grep libbfd

LIBIBERTY!=cd $(.CURDIR)/../libiberty; \
	printf "xxx:\n\techo \$${.OBJDIR}/libiberty.a\n" | \
	${MAKE} -r -s -f - xxx | grep libiberty


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

