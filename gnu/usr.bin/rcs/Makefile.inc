#	$Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:40:59 deraadt Exp $

LIBRCS!=cd $(.CURDIR)/../lib; \
	printf "xxx:\n\techo \$${.OBJDIR}/librcs.a\n" | ${MAKE} -r -s -f - xxx
