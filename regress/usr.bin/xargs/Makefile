#	$OpenBSD: Makefile,v 1.1 2010/03/25 01:43:47 schwarze Exp $

regress: showargs
	@sh ${.CURDIR}/xargs-L.sh

clean:
	rm -f showargs

.include <bsd.regress.mk>
