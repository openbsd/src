#	$Id: Makefile,v 1.2 1996/01/04 01:50:33 deraadt Exp $

NOMAN=

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cpp.sh ${DESTDIR}${BINDIR}/cpp
	if [ `cc -print-libgcc-file-name` != /usr/lib/libgcc.a ]; then \
		(cd ${DESTDIR}/usr/libexec; rm -f cpp; \
		ln -s `cc -print-libgcc-file-name | sed -e 's/libgcc\.a/cpp/'` cpp); \
	fi

.include <bsd.prog.mk>
