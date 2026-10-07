# $OpenBSD: Makefile,v 1.16 2026/10/07 17:02:55 schwarze Exp $

REGRESS_TARGETS  = leading-mdoc leading-man multiple trailing-mdoc zerowidth
REGRESS_TARGETS += eos eos-man break nobreak
REGRESS_TARGETS += tab tab-filled tab-man esct-mdoc esct-man
REGRESS_TARGETS += invalid

UTF8_TARGETS	 = zerowidth break

HTML_TARGETS	 = zerowidth break

LINT_TARGETS	 = trailing-mdoc tab tab-man esct-mdoc esct-man invalid

.include <bsd.regress.mk>
