#	$OpenBSD: Makefile,v 1.1 2022/11/30 10:47:05 tb Exp $

# The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
#
# This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
# in April-May 1998
#
# Copyright (C) 1998, 1999 by Angelos D. Keromytis.
#
# Permission to use, copy, and modify this software with or without fee
# is hereby granted, provided that this entire notice is included in
# all copies of any software which is or includes a copy or
# modification of this software.
#
# THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
# REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
# MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
# PURPOSE.

REGRESS_TARGETS += test-libkeynote

KEYNOTEDIR = ${.CURDIR}/../../../lib/libkeynote

test-libkeynote:
	cd ${KEYNOTEDIR} && \
		keynote verify -e testsuite/test-env \
		-r false,maybe,probably,true \
		-k testsuite/auth1 \
		-k testsuite/auth2 \
		-k testsuite/auth3 \
		-k testsuite/auth4 \
		-l testsuite/test-assertion1 \
		-l testsuite/test-assertion2 \
		-l testsuite/test-assertion3 \
		-l testsuite/test-assertion4 \
		-l testsuite/test-assertion5 \
		-l testsuite/test-assertion6 \
		-l testsuite/test-assertion7 \
		| grep 'Query result = true'
	@echo SUCCESS

.include <bsd.regress.mk>
