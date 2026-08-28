# Copyright (c) 2019 William Pitcock <nenolod@dereferenced.org>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# This software is provided 'as is' and without any warranty, express or
# implied.  In no event shall the authors be liable for any damages arising
# from the use of this software.

# pkgconf-lite is a staticly-linked version of pkgconf that does not include
# all features, notably it does not include cross-compile support and MSVC
# support.  It does not include the libpkgconf library.

LIBPKGCONF_SRCS_ORIG = \
	libpkgconf/argvsplit.c		\
	libpkgconf/audit.c		\
	libpkgconf/bsdstubs.c		\
	libpkgconf/buffer.c		\
	libpkgconf/bufferset.c		\
	libpkgconf/bytecode.c		\
	libpkgconf/cache.c		\
	libpkgconf/client.c		\
	libpkgconf/dependency.c		\
	libpkgconf/fileio.c		\
	libpkgconf/fragment.c		\
	libpkgconf/license.c		\
	libpkgconf/output.c		\
	libpkgconf/parser.c		\
	libpkgconf/path.c		\
	libpkgconf/personality.c	\
	libpkgconf/pkg.c		\
	libpkgconf/queue.c		\
	libpkgconf/tuple.c		\
	libpkgconf/variable.c		\
	libpkgconf/version.c		\

CLI_CORE_SRCS_ORIG = \
	cli/getopt_long.c		\
	cli/core.c

CLI_MAIN_SRCS_ORIG = \
	cli/main.c

LIBPKGCONF_SRCS =	${LIBPKGCONF_SRCS_ORIG:S,^libpkgconf/,,}
CLI_CORE_SRCS =		${CLI_CORE_SRCS_ORIG:S,^cli/,,}
CLI_MAIN_SRCS =		${CLI_MAIN_SRCS_ORIG:S,^cli/,,}

PKGCONF_SRCS = ${LIBPKGCONF_SRCS} ${CLI_CORE_SRCS} ${CLI_MAIN_SRCS}
PKGCONF_OBJS = ${PKGCONF_SRCS:.c=.o}

TEST_RUNNER_MAIN_SRCS_ORIG =	\
	tests/test-runner.c

TEST_RUNNER_MAIN_SRCS =	${TEST_RUNNER_MAIN_SRCS_ORIG:S,^tests/,,}

TEST_RUNNER_SRCS = ${LIBPKGCONF_SRCS} ${CLI_CORE_SRCS} ${TEST_RUNNER_MAIN_SRCS}
TEST_RUNNER_OBJS = ${TEST_RUNNER_SRCS:.c=.o}

CFLAGS += -Wno-unused-variable -Wno-unused-parameter
CPPFLAGS = -D_DEFAULT_SOURCE -DPKGCONF_LITE -I. -Ilibpkgconf -Icli -DSYSTEM_LIBDIR=\"${SYSTEM_LIBDIR}\" -DSYSTEM_INCLUDEDIR=\"${SYSTEM_INCLUDEDIR}\" -DPKG_DEFAULT_PATH=\"${PKG_DEFAULT_PATH}\"
STRIP = strip

#all: check

libpkgconf/config.h:
	@echo '#define PACKAGE_NAME "pkgconf-lite"' >> $@
	@echo '#define PACKAGE_BUGREPORT "https://git.dereferenced.org/pkgconf/pkgconf/issues"' >> $@
	@echo '#define PACKAGE_VERSION "3.0.6"' >> $@
	@echo '#define PACKAGE PACKAGE_NAME " " PACKAGE_VERSION' >> $@


run-regress-test-runner-lite: test-runner-lite
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/basic
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/link-abi
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/ordering
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/parser
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/personality
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/sbom
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/solver
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/sysroot
	./test-runner-lite --test-fixtures ${.CURDIR}/tests ${.CURDIR}/t/tuple

SYSTEM_LIBDIR=/usr/lib
SYSTEM_INCLUDEDIR=/usr/include
SYSPATH=/usr/lib/pkgconfig
PKGPATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig
X11PATH=/usr/X11R6/lib/pkgconfig:/usr/X11R6/share/pkgconfig
PKG_DEFAULT_PATH=${SYSPATH}:${PKGPATH}:${X11PATH}

CFLAGS += -I ${.CURDIR}/../../../usr.bin/pkgconf
CFLAGS += -I ${.CURDIR}

PROGS =	test-runner-lite

SRCS_pkgconf = ${PKGCONF_SRCS}
SRCS_test-runner-lite = ${LIBPKGCONF_SRCS} ${CLI_CORE_SRCS} ${TEST_RUNNER_MAIN_SRCS}

.PATH: ${.CURDIR}/tests
.PATH: ${.CURDIR}/../../../usr.bin/pkgconf
.PATH: ${.CURDIR}/../../../usr.bin/pkgconf/libpkgconf
.PATH: ${.CURDIR}/../../../usr.bin/pkgconf/cli

.include <bsd.regress.mk>
