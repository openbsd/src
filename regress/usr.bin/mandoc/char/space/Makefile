# $OpenBSD: Makefile,v 1.13 2018/12/15 19:30:19 schwarze Exp $

REGRESS_TARGETS  = leading-mdoc leading-man multiple trailing-mdoc zerowidth
REGRESS_TARGETS += eos eos-man break nobreak
REGRESS_TARGETS += tab tab-man esct-mdoc esct-man
REGRESS_TARGETS += invalid

UTF8_TARGETS	 = zerowidth

HTML_TARGETS	 = zerowidth

LINT_TARGETS	 = trailing-mdoc tab tab-man esct-mdoc esct-man invalid

.include <bsd.regress.mk>
