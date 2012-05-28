# $OpenBSD: Makefile,v 1.2 2012/05/28 17:08:48 schwarze Exp $

REGRESS_TARGETS=h z

# Postprocessing to remove "character backspace" sequences
# unless they are foolowed by the same character again.
# This removes underlining as well, so we mustn't use it.
# Cannot use /g because matches progress backwards.

z.out_ascii: z.in
	${NROFF} ${.ALLSRC} | \
		perl -pe 'while (s/(.)\010(?!\1)//) {}' \
		> ${.TARGET}

.include <bsd.regress.mk>
