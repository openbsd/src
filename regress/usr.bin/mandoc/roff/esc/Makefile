# $OpenBSD: Makefile,v 1.5 2014/07/06 19:08:57 schwarze Exp $

REGRESS_TARGETS = one two multi B c f h w z ignore
LINT_TARGETS	= B h w

# Postprocessing to remove "character backspace" sequences
# unless they are foolowed by the same character again.
# This removes underlining as well, so we mustn't use it.
# Cannot use /g because matches progress backwards.

z.out_ascii: z.in
	${NROFF} ${.ALLSRC} | \
		perl -pe 'while (s/(.)\010(?!\1)//) {}' \
		> ${.TARGET}

.include <bsd.regress.mk>
