#	$OpenBSD: Makefile,v 1.1.1.1 2022/10/20 07:33:14 tb Exp $

RUST_OPENSSL_TESTS =	/usr/local/share/rust-openssl-tests
CARGO =			/usr/local/bin/cargo

.if !exists(${RUST_OPENSSL_TESTS}) || !exists(${CARGO})
regress:
	@echo packages rust-openssl-tests and rust are required for this regress
	@echo SKIPPED
.else

REGRESS_TARGETS +=	rust-openssl-test

WORKSPACE_LINKS = openssl openssl-errors openssl-macros openssl-sys systest

_WORKSPACE_COOKIE =	.workspace

${_WORKSPACE_COOKIE}:
. if ${.CURDIR} != ${.OBJDIR}
	cp ${.CURDIR}/Cargo.toml ${.OBJDIR}/
. endif
	mkdir -p .cargo
	cp ${.CURDIR}/config .cargo/
	cd ${.OBJDIR} && ln -sf ${WORKSPACE_LINKS:S,^,${RUST_OPENSSL_TESTS}/,} .
	touch $@

CLEANFILES += Cargo.lock

. if ${.CURDIR} != ${.OBJDIR}
CLEANFILES += Cargo.toml
. endif

rust-openssl-test: ${_WORKSPACE_COOKIE}
	cd ${.OBJDIR} && cargo test --offline --color=never -- --color=never

CLEANFILES +=	${_WORKSPACE_COOKIE} ${WORKSPACE_LINKS}

. if make(clean) || make(cleandir)
.  if exists(.cargo) || exists(target)
.BEGIN:
	rm -rf .cargo
	rm -rf target
.  endif
. endif

.endif

.include <bsd.regress.mk>
