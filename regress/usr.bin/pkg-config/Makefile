#	$OpenBSD: Makefile,v 1.48 2014/11/02 01:09:54 jca Exp $

REGRESS_TARGETS=cmp-vers1-1 \
		cmp-vers1-2 \
		cmp-vers1-3 \
		cmp-vers2-1 \
		cmp-vers2-2 \
		cmp-vers2-3 \
		cmp-vers2-4 \
		cmp-vers2-5 \
		cmp-vers2-6 \
		cmp-vers3 \
		cmp-vers4-1 \
		cmp-vers4-2 \
		cmp-vers4-3 \
		cmp-vers4-4 \
		cmp-vers4-5 \
		cmp-vers4-6 \
		cmp-vers4-7 \
		cmp-vers4-8 \
		cmp-vers4-9 \
		cmp-vers5-1 \
		cmp-vers5-2 \
		cmp-vers5-3 \
		cmp-vers5-4 \
		cmp-vers5-5 \
		cmp-vers5-6 \
		cmp-vers5-7 \
		cmp-vers6-1 \
		cmp-vers6-2 \
		cmp-vers7-1 \
		cmp-vers7-2 \
		cmp-vers7-3 \
		cmp-vers7-4 \
		corrupt1 \
		corrupt2 \
		corrupt3 \
		print-provides \
		print-req \
		print-req-priv \
		logfile \
		sysroot-cflags \
		sysroot-libs \
		static-cflags1 \
		static-cflags2 \
		static-libs1 \
		static-libs2 \
		static-libs3 \
		static-libs4 \
		static-order \
		missing-req-1 \
		missing-req-2 \
		missing-req-3 \
		missing-req-4 \
		whitespace-cflags \
		whitespace-libs \
		whitespace-linebreak \
		whitespace-name \
		quoting \
		broken-require-1 \
		broken-require-2 \
		min-version-1 \
		min-version-2 \
		exact-version-1 \
		exact-version-2 \
		max-version-1 \
		max-version-2 \
		find-config-1 \
		find-config-2 \
		variable-get-1 \
		variable-set \
		variables-1 \
		variables-2 \
		variables-3 \
		variables-4 \
		variables-5

PKG_CONFIG?=	pkg-config
PCONFIG =	PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}
VPCONFIG =	${PCONFIG} --errors-to-stdout --print-errors > ${GOT}
WANT=		${.OBJDIR}/$@.want
GOT=		${.OBJDIR}/$@.got

# Tests for corrupt pkg-config files.
corrupt1:
	# Test for missing variables/fields
	@echo "Package '$@' has no Version: field" > ${WANT}
	@if ${VPCONFIG} --exists $@; then false; fi
	@diff -u ${WANT} ${GOT}

corrupt2:
	# Test for missing variables/fields
	@echo "Package '$@' has no Name: field" > ${WANT}
	@if ${VPCONFIG} --exists $@; then false; fi
	@diff -u ${WANT} ${GOT}

corrupt3:
	# Test for empty file
	@echo "Package file '${.CURDIR}/pcdir//$@.pc' appears to be empty" > ${WANT}
	@echo "Package '$@' has no Name: field" >> ${WANT}
	@if ${VPCONFIG} --libs $@; then false; fi
	@diff -u ${WANT} ${GOT}

# Tests for various printing features
print-provides:
	# Test --print-provides
	@echo "print-provides = 0.0.0" > ${WANT}
	@${PCONFIG} --print-provides $@ > ${GOT}
	@diff -u ${WANT} ${GOT}

print-req:
	# Test --print-requires
	@echo "print-req2>=0.0.1" > ${WANT}
	@${PCONFIG} --print-requires $@ > ${GOT}
	@diff -u ${WANT} ${GOT}

print-req-priv:
	# Test --print-requires-private
	@echo "print-req-priv2>=0.0.0" > ${WANT}
	@${PCONFIG} --print-requires-private $@ > ${GOT}
	@diff -u ${WANT} ${GOT}

# Tests for version comparison
cmp-vers1-1:
	# Test regular versions (a <= b)
	@if ${PCONFIG} --exists "vers1 <= 0.0.2"; then false; fi

cmp-vers1-2:
	# Test regular versions (a <= b)
	@${PCONFIG} --exists "vers1 <= 0.1.0"

cmp-vers1-3:
	# Test regular versions (a < b)
	@${PCONFIG} --exists "vers1 < 0.2"

cmp-vers2-1:
	# Test regular versions (a >= b)
	@${PCONFIG} --exists "vers2 >= 0.0.1"

cmp-vers2-2:
	# Test regular versions (a >= b)
	@${PCONFIG} --exists "vers2 >= 0.0.0"

cmp-vers2-3:
	# Test regular versions (a >= b)
	@${PCONFIG} --exists "vers2 >= 0.1.0"

cmp-vers2-4:
	# Test regular versions (a >= b)
	@if ${PCONFIG} --exists "vers2 > 0.2.0.0"; then false; fi

cmp-vers2-5:
	# Test regular versions (a != b)
	@${PCONFIG} --exists "vers2 != 0.1.0"

cmp-vers2-6:
	# Test regular versions (a != b)
	@if ${PCONFIG} --exists "vers2 != 0.2.0"; then false; fi

cmp-vers3:
	# Test regular versions (a = b)
	@${PCONFIG} --exists "vers2 = 0.2.0"

cmp-vers4-1:
	# Test suffixed versions (alpha >)
	@touch ${WANT}
	@${VPCONFIG} "alpha > 0.1.0"
	@diff -u ${WANT} ${GOT}

cmp-vers4-2:
	# Test suffixed versions (alpha >)
	@touch ${WANT}
	@${VPCONFIG} "alpha > 0.1.0alpha1"
	@diff -u ${WANT} ${GOT}

cmp-vers4-3:
	# Test suffixed versions (alpha >=)
	@touch ${WANT}
	@${VPCONFIG}  "alpha >= 0.1.0"
	@diff -u ${WANT} ${GOT}

cmp-vers4-4:
	# Test suffixed versions (alpha >=)
	@touch ${WANT}
	@${VPCONFIG}  "alpha >= 0.1.0alpha2"
	@diff -u ${WANT} ${GOT}

cmp-vers4-5:
	# Test suffixed versions (alpha <)
	@touch ${WANT}
	@${VPCONFIG} "alpha < 0.1.1"
	@diff -u ${WANT} ${GOT}

cmp-vers4-6:
	# Test suffixed versions (alpha <)
	@touch ${WANT}
	@${VPCONFIG} "alpha < 0.1.0alpha3"
	@diff -u ${WANT} ${GOT}

cmp-vers4-7:
	# Test suffixed versions (alpha <=)
	@touch ${WANT}
	@${VPCONFIG}  "alpha < 0.1.1"
	@diff -u ${WANT} ${GOT}

cmp-vers4-8:
	# Test suffixed versions (alpha <=)
	@touch ${WANT}
	@${VPCONFIG} "alpha <= 0.1.0alpha2"
	@diff -u ${WANT} ${GOT}

cmp-vers4-9:
	# Test suffixed versions (alpha =)
	@touch ${WANT}
	@${VPCONFIG} "alpha = 0.1.0alpha2"
	@diff -u ${WANT} ${GOT}

cmp-vers5-1:
	# Test suffixed versions (beta > alpha)
	@touch ${WANT}
	@${VPCONFIG} "beta > 0.1.0alpha1"
	@diff -u ${WANT} ${GOT}

cmp-vers5-2:
	# Test suffixed versions (beta >= alpha)
	@touch ${WANT}
	@${VPCONFIG} "beta >= 0.1.0beta1"
	@diff -u ${WANT} ${GOT}

cmp-vers5-3:
	# Test suffixed versions (alpha < rc)
	@touch ${WANT}
	@${VPCONFIG}  "alpha < 0.1.0rc2"
	@diff -u ${WANT} ${GOT}

cmp-vers5-4:
	# Test suffixed versions (beta < rc)
	@touch ${WANT}
	@${VPCONFIG} "beta < 0.1.0rc2"
	@diff -u ${WANT} ${GOT}

cmp-vers5-5:
	# Test suffixed versions (alpha < beta)
	@touch ${WANT}
	@${VPCONFIG}  "alpha < 0.1.0beta2"
	@diff -u ${WANT} ${GOT}

cmp-vers5-6:
	# Test suffixed versions (rc > alpha)
	@touch ${WANT}
	@${VPCONFIG} "rc > 0.1.0alpha2"
	@diff -u ${WANT} ${GOT}

cmp-vers5-7:
	# Test suffixed versions (rc > beta)
	@touch ${WANT}
	@${VPCONFIG} "rc > 0.1.0beta2"
	@diff -u ${WANT} ${GOT}

# These tests also fail with fd.o pkg-config, so keep them for later
# to decide how our pkg-config should behave.
cmp-vers5-8:
	# Test suffixed versions (' ' > alpha)
	@touch ${WANT}
	@${VPCONFIG} "vers1 > 0.1.0alpha2"
	@diff -u ${WANT} ${GOT}

cmp-vers5-9:
	# Test suffixed versions (' ' > beta)
	@touch ${WANT}
	@${VPCONFIG} "beta <= 0.1.0beta2"
	@diff -u ${WANT} ${GOT}

cmp-vers5-10:
	# Test suffixed versions (' ' > rc)
	@touch ${WANT}
	@${VPCONFIG} "beta <= 0.1.0rc2"
	@diff -u ${WANT} ${GOT}

cmp-vers6-1:
	# Test suffixed versions in Requires
	@echo " -lalpha2" > ${WANT}
	@${VPCONFIG} --libs requires-test2
	@diff -u ${WANT} ${GOT}

cmp-vers6-2:
	# Test suffixed versions in Requires.private
	@echo "Requested 'alpha >= 0.1.0alpha3' but version of alpha suffix test is 0.1.0alpha2" > ${WANT}
	@if ${VPCONFIG} --libs --static requires-test2; then false; fi
	@diff -u ${WANT} ${GOT}

cmp-vers7-1:
	# Test suffixed version (vers3 > 1.0.1g)
	@touch ${WANT}
	@${VPCONFIG} "vers3 > 1.0.1g"
	@diff -u ${WANT} ${GOT}

cmp-vers7-2:
	# Test suffixed version (vers3 < 1.0.1i)
	@touch ${WANT}
	@${VPCONFIG} "vers3 < 1.0.1i"
	@diff -u ${WANT} ${GOT}

cmp-vers7-3:
	# Test suffixed version (vers3 < 1.0.1i)
	@touch ${WANT}
	@${VPCONFIG} "vers3 < 1.0.1i"
	@diff -u ${WANT} ${GOT}

cmp-vers7-4:
	# Test suffixed version (vers3 > 1.0.1)
	@touch ${WANT}
	@${VPCONFIG} "vers3 > 1.0.1"
	@diff -u ${WANT} ${GOT}

# Tests for various environment variables
builddir:
	# Test PKG_CONFIG_TOP_BUILD_DIR

logfile:
	# Test PKG_CONFIG_LOG
	@echo "[/usr/bin/pkg-config] [--exists] [sysroot >= 0.0.0]" > ${WANT}
	@rm -f ${GOT}
	@PKG_CONFIG_LOG=${GOT} ${PCONFIG} --exists "sysroot >= 0.0.0"
	@diff -u ${WANT} ${GOT}

sysroot-cflags:
	# Test PKG_CONFIG_SYSROOT_DIR (cflags)
	@echo "-I/altroot/tmp/include -I/altroot/tmp/include/foo" > ${WANT}
	@PKG_CONFIG_SYSROOT_DIR=/altroot ${PCONFIG} --cflags sysroot > ${GOT}
	@diff -u ${WANT} ${GOT}

sysroot-libs:
	# Test PKG_CONFIG_SYSROOT_DIR (libs)
	@echo "-L/altroot/tmp/lib -L/altroot/tmp/lib/foo -lc" > ${WANT}
	@PKG_CONFIG_SYSROOT_DIR=/altroot ${PCONFIG} --libs sysroot > ${GOT}
	@diff -u ${WANT} ${GOT}

# Tests for static linking
static-cflags1:
	# Test grabbing Cflags (without Requires.private)
	@echo "-I/usr/local/include/foo" > ${WANT}
	@${VPCONFIG} --cflags --static static
	@diff -u ${WANT} ${GOT}

static-cflags2:
	# Test grabbing Cflags (with Requires.private)
	@echo "-I/usr/local/include/foo -I/usr/local/include" > ${WANT}
	@${VPCONFIG} --cflags --static static2
	@diff -u ${WANT} ${GOT}

static-libs1:
	# Test grabbing Libs.private from this package
	@echo "-L/usr/local/lib -lc -lm -ll" > ${WANT}
	@${VPCONFIG} --libs --static static
	@diff -u ${WANT} ${GOT}

static-libs2:
	# Test grabbing Libs.private from Requires in order
	@echo "-L/usr/local/lib -lc -lm -ll -lutil -lz" > ${WANT}
	@${VPCONFIG} --libs --static static2
	@diff -u ${WANT} ${GOT}

static-libs3:
	# Test grabbing Libs.private from Requires.private in order
	@echo "-L/tmp/lib -L/tmp/lib/foo -L/usr/local/lib -lbaz\ quux -lc -lm -ll -lutil -lz" > ${WANT}
	@${VPCONFIG} --libs --static static3
	@diff -u ${WANT} ${GOT}

static-libs4:
	# Test Requires.private
	@echo "-L/public-dep/lib -L/private-dep/lib -L/requires-test/lib -lpublic-dep -lprivate-dep -lrequires-test" > ${WANT}
	@${VPCONFIG} --libs --static requires-test
	@diff -u ${WANT} ${GOT}

static-order:
	# Test Requires.private order
	@echo "-L/usr/X11R6/lib -lstatic-order1 -lstatic-order2 -lstatic-order3" > ${WANT}
	@${VPCONFIG} --static --libs static-order1
	@diff -u ${WANT} ${GOT}

# This fails as we read and check Requires as well as Requires.private where
# only Requires needs to be checked here as we ask for Libs and not Cflags.
missing-req-1:
	# Test for missing packages in Requires (libs)
	@echo "Package nonexisting was not found in the pkg-config search path" > ${WANT}
	@if ${VPCONFIG} --libs missing-req ; then false; fi
	@diff -u ${WANT} ${GOT}

missing-req-2:
	# Test for missing packages in Requires (cflags)
	@echo "Package nonexisting was not found in the pkg-config search path" > ${WANT}
	@if ${VPCONFIG} --cflags missing-req; then false; fi
	@diff -u ${WANT} ${GOT}

missing-req-3:
	# Test for missing packages in Requires (version)
	@echo "0.0.0" > ${WANT}
	@${VPCONFIG} --modversion missing-req
	@diff -u ${WANT} ${GOT}

missing-req-4:
	# Test for missing Requires.private with --exists
	@echo "Package nonexisting was not found in the pkg-config search path" > ${WANT}
	@if ${VPCONFIG} --exists missing-req2 ; then false; fi
	@diff -u ${WANT} ${GOT}

whitespace-cflags:
	# Test whitespace on various positions in Cflags
	@echo "-I/tmp/include -I/tmp/include/include\ dir" > ${WANT}
	@${VPCONFIG} --cflags whitespace
	@diff -u ${WANT} ${GOT}

whitespace-libs:
	# Test whitespace on various positions in Libs
	@echo "-L/tmp/lib -L/tmp/lib/foo -lbaz\ quux" > ${WANT}
	@${VPCONFIG} --libs whitespace
	@diff -u ${WANT} ${GOT}

whitespace-linebreak:
	# Test linebreak in Description field
	@echo " -lc" > ${WANT}
	@${VPCONFIG} --libs linebreak
	@diff -u ${WANT} ${GOT}

whitespace-name:
	# Test whitespace in Name
	@echo "Requested 'whitespace > 0.2.0' but version of Whitespace test is 0.0.0" > ${WANT}
	@if ${VPCONFIG} "whitespace > 0.2.0"; then false; fi
	@diff -u ${WANT} ${GOT}

quoting:
	# Test for stripping out quotes from variables
	@echo "-I/usr/X11R6/include/freetype2" > ${WANT}
	@${VPCONFIG} --cflags quotes
	@diff -u ${WANT} ${GOT}

broken-require-1:
	# Test expanded variables with special parsing requirements
	@${PCONFIG} requires-test3

broken-require-2:
	# Test expanded variables with special parsing requirements
	@${PCONFIG} requires-test4

min-version-1:
	# Test --atleast-version (matches)
	@${PCONFIG} --atleast-version=0.0.9 vers1

min-version-2:
	# Test --atleast-version (doesn't match)
	@if ${PCONFIG} --atleast-version=0.1.1 vers1 ; then false; fi

exact-version-1:
	# Test --exact-version (matches)
	@${PCONFIG} --exact-version=0.1.0 vers1

exact-version-2:
	# Test --exact-version (doesn't match)
	@if ${PCONFIG} --exact-version=0.1.1 vers1 ; then false; fi

max-version-1:
	# Test --max-version (matches)
	@${PCONFIG} --max-version=0.1.0 vers1

max-version-2:
	# Test --max-version (doesn't match)
	@if ${PCONFIG} --max-version=0.0.9 vers1 ; then false; fi

find-config-1:
	# Test a regular module name being resolved
	@echo "1.0.0" > ${WANT}
	@${VPCONFIG} --modversion public-dep
	@diff -u ${WANT} ${GOT}

# Need to use ${PKG_CONFIG} instead of ${VPCONFIG} as the latter sets PKG_CONFIG_PATH,
# which is exactly what we don't want for this test.
find-config-2:
	# Test a full path to a .pc file
	@echo "1.0.0" > ${WANT}
	@${PKG_CONFIG} --errors-to-stdout --print-errors --modversion \
		${.CURDIR}/pcdir/public-dep.pc > ${GOT}
	@diff -u ${WANT} ${GOT}

variable-get-1:
	# Test --variable
	@echo "/tmp" > ${WANT}
	@${VPCONFIG} --variable prefix sysroot
	@diff -u ${WANT} ${GOT}

variable-set:
	# Test --define-variable
	@echo "-I/usr/local2/include -I/usr/local2/include/foo" > ${WANT}
	@${VPCONFIG} --define-variable=prefix=/usr/local2 \
		--cflags sysroot
	@diff -u ${WANT} ${GOT}

variables-1:
	# Test --variable without any options
	@echo "/usr/local/lib/pkgconfig-plugins.0" > ${WANT}
	@${VPCONFIG} --variable=plugindir variables
	@diff -u ${WANT} ${GOT}

variables-2:
	# Test --variable + --define-variable
	@echo "/chroot/lib/pkgconfig-plugins.0" > ${WANT}
	@${VPCONFIG} --define-variable=libdir='/chroot/lib' --variable=plugindir variables
	@diff -u ${WANT} ${GOT}

variables-3:
	# Test --variable + --define-variable
	@echo '$${foodir}/pkgconfig-plugins.0' > ${WANT}
	@${VPCONFIG} --define-variable=libdir='$${foodir}' --variable=plugindir variables
	@diff -u ${WANT} ${GOT}

variables-4:
	# Test --variable + --define-variable
	@echo '$${libdir}/pkgconfig-plugins.0' > ${WANT}
	@${VPCONFIG} --define-variable=libdir='$${libdir}' --variable=plugindir variables
	@diff -u ${WANT} ${GOT}

variables-5:
	# Test --variable
	@echo ' -lfoo-1' > ${WANT}
	@${VPCONFIG} --libs variables
	@diff -u ${WANT} ${GOT}

clean:
	rm -f *.want *.got

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
