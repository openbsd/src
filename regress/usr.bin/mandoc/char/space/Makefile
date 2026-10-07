# $OpenBSD: Makefile,v 1.15 2026/10/07 12:44:06 schwarze Exp $

REGRESS_TARGETS  = leading-mdoc leading-man multiple trailing-mdoc zerowidth
REGRESS_TARGETS += eos eos-man break nobreak
REGRESS_TARGETS += tab tab-filled tab-man esct-mdoc esct-man
REGRESS_TARGETS += invalid

UTF8_TARGETS	 = zerowidth break

HTML_TARGETS	 = zerowidth

LINT_TARGETS	 = trailing-mdoc tab tab-man esct-mdoc esct-man invalid

.include <bsd.regress.mk>
