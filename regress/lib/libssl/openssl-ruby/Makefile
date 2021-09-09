#	$OpenBSD: Makefile,v 1.5 2021/09/09 15:02:33 tb Exp $

OPENSSL_RUBY_TESTS =	/usr/local/share/openssl-ruby-tests
RUBY_BINREV =		30
RUBY =			ruby${RUBY_BINREV}

# We work in a subdirectory of obj/ since extconf.rb generates a Makefile whose
# name can't be customized in $PWD. An obj/Makefile in turn confuses either make
# or bsd.*.mk. This hurts when things are in an unexpected state after a signal.
BUILDDIR =		build

.if !exists(${OPENSSL_RUBY_TESTS})
regress:
	@echo package openssl-ruby-tests is required for this regress
	@echo SKIPPED
.else

REGRESS_TARGETS +=		openssl-ruby-test client-ca-test
REGRESS_EXPECTED_FAILURES +=	client-ca-test

openssl-ruby-test:	retest

_BUILDDIR_COOKIE =	.builddir
_BUILD_COOKIE =		.build
_TEST_COOKIE =		.test

${_BUILDDIR_COOKIE}:
	mkdir -p ${BUILDDIR}
	touch $@

${_BUILD_COOKIE}: ${_BUILDDIR_COOKIE}
	cd ${BUILDDIR} && \
	    ${RUBY} ${OPENSSL_RUBY_TESTS}/ext/openssl/extconf.rb && \
	    make;
	touch $@

OPENSSL_RUBY_TESTSRC =	${OPENSSL_RUBY_TESTS}/test/openssl/test_*.rb
${_TEST_COOKIE}: ${_BUILD_COOKIE} ${_BUILDDIR_COOKIE}
	cd ${BUILDDIR} && \
	    env SKIP_CLIENT_CA_TEST=true ${RUBY} -I. \
		-I${OPENSSL_RUBY_TESTS}/test/openssl \
		-I${OPENSSL_RUBY_TESTS}/lib \
		-e 'Dir["${OPENSSL_RUBY_TESTSRC}"].each{|f| require f}' \
		-- --no-use-color --no-show-detail-immediately
	touch $@

build: ${_BUILD_COOKIE}
test: ${_TEST_COOKIE}

_MAKE =			cd ${.CURDIR} && exec ${.MAKE}

rebuild:
	rm -f ${_BUILD_COOKIE}
	${_MAKE} build

retest:
	rm -f ${_TEST_COOKIE}
	${_MAKE} test

client-ca-test: ${_BUILD_COOKIE}
	cd ${BUILDDIR} && \
	    ${RUBY} -I. -I${OPENSSL_RUBY_TESTS}/test/openssl \
		-I${OPENSSL_RUBY_TESTS}/lib \
		${OPENSSL_RUBY_TESTS}/test/openssl/test_ssl.rb \
		-n test_client_ca

CLEANFILES +=		${_BUILD_COOKIE} ${_TEST_COOKIE} ${_BUILDDIR_COOKIE}

. if make(clean) || make(cleandir)
.  if exists(${BUILDDIR})
.BEGIN:
	rm -r ${BUILDDIR}
.  endif
. endif

.PHONY: build rebuild test retest

.endif

.include <bsd.regress.mk>
