#	$Id: Makefile.inc,v 1.2 1995/12/14 03:43:17 deraadt Exp $

LIBRCS!=cd $(.CURDIR)/../lib; \
	printf "xxx:\n\techo \$${.OBJDIR}/librcs.a\n" | \
	${MAKE} -r -s -f - xxx | grep librcs
