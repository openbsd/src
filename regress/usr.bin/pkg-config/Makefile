#	$OpenBSD: Makefile,v 1.15 2011/06/12 18:16:39 jasper Exp $

REGRESS_TARGETS=cmp-vers1-1 \
		cmp-vers1-2 \
		cmp-vers1-3 \
		cmp-vers2-1 \
		cmp-vers2-2 \
		cmp-vers2-3 \
		cmp-vers2-4 \
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
		corrupt1 \
		corrupt2 \
		corrupt3 \
		print-provides \
		print-req \
		print-req-priv \
		logfile \
		sysroot-cflags \
		sysroot-libs \
		missing-req-1 \
		missing-req-2 \
		missing-req-3 \
		whitespace-cflags \
		whitespace-libs \
		whitespace-name

PKG_CONFIG=	pkg-config
WANT=		${.OBJDIR}/$@.want
GOT=		${.OBJDIR}/$@.got

# Tests for corrupt pkg-config files.
corrupt1:
	# Test for missing variables/fields
	@echo "Package '$@' has no Version: field" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} \
                --errors-to-stdout --print-errors --exists $@ > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

corrupt2:
	# Test for missing variables/fields
	@echo "Package '$@' has no Name: field" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} \
		--errors-to-stdout --print-errors --exists $@ > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

corrupt3:
	# Test for empty file
	@echo "Package file '$@.pc' appears to be empty" > ${WANT}
	@echo "Package '$@' has no Name: field" >> ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} \
		--errors-to-stdout --print-errors --libs $@ > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

# Tests for various printing features
print-provides:
	# Test --print-provides
	@echo "print-provides = 0.0.0" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} \
		--print-provides $@ > ${GOT}
	@diff -u ${WANT} ${GOT}

print-req:
	# Test --print-requires
	@echo "print-req2>=0.0.1" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} \
		--print-requires $@ > $@.got
	@diff -u ${WANT} ${GOT}

print-req-priv:
	# Test --print-requires-private
	@echo "print-req-priv2>=0.0.1" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} \
		--print-requires-private $@ > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

# Tests for version comparison
cmp-vers1-1:
	# Test regular versions (a <= b)
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists \
		"vers1 <= 0.0.2"; then false; fi

cmp-vers1-2:
	# Test regular versions (a <= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists "vers1 <= 0.1.0"

cmp-vers1-3:
	# Test regular versions (a < b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists "vers1 < 0.2"

cmp-vers2-1:
	# Test regular versions (a >= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists "vers2 >= 0.0.1"

cmp-vers2-2:
	# Test regular versions (a >= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists "vers2 >= 0.0.0"

cmp-vers2-3:
	# Test regular versions (a >= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists "vers2 >= 0.1.0"

cmp-vers2-4:
	# Test regular versions (a >= b)
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists \
		"vers2 > 0.2.0.0"; then false; fi

cmp-vers3:
	# Test regular versions (a = b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --exists "vers2 = 0.2.0"

cmp-vers4-1:
	# Test suffixed versions (alpha >)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha > 0.1.0" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-2:
	# Test suffixed versions (alpha >)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha > 0.1.0alpha1" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-3:
	# Test suffixed versions (alpha >=)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha >= 0.1.0" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-4:
	# Test suffixed versions (alpha >=)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha >= 0.1.0alpha2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-5:
	# Test suffixed versions (alpha <)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha < 0.1.1" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-6:
	# Test suffixed versions (alpha <)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha < 0.1.0alpha3" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-7:
	# Test suffixed versions (alpha <=)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha < 0.1.1" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-8:
	# Test suffixed versions (alpha <=)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha <= 0.1.0alpha2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers4-9:
	# Test suffixed versions (alpha =)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha = 0.1.0alpha2" > ${GOT}
	@diff -u ${WANT} ${GOT}


cmp-vers5-1:
	# Test suffixed versions (beta > alpha)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "beta > 0.1.0alpha1" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-2:
	# Test suffixed versions (beta >= alpha)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "beta >= 0.1.0beta1" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-3:
	# Test suffixed versions (alpha < rc)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha < 0.1.0rc2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-4:
	# Test suffixed versions (beta < rc)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "beta < 0.1.0rc2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-5:
	# Test suffixed versions (alpha < beta)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "alpha < 0.1.0beta2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-6:
	# Test suffixed versions (rc > alpha)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "rc > 0.1.0alpha2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-7:
	# Test suffixed versions (rc > beta)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "rc > 0.1.0beta2" > ${GOT}
	@diff -u ${WANT} ${GOT}

# These tests also fail with fd.o pkg-config, so keep them for later
# to decide how our pkg-config should behave.
cmp-vers5-8:
	# Test suffixed versions (' ' > alpha)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "vers1 > 0.1.0alpha2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-9:
	# Test suffixed versions (' ' > beta)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "beta <= 0.1.0beta2" > ${GOT}
	@diff -u ${WANT} ${GOT}

cmp-vers5-10:
	# Test suffixed versions (' ' > rc)
	@touch ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG}  \
		--errors-to-stdout --print-errors "beta <= 0.1.0rc2" > ${GOT}
	@diff -u ${WANT} ${GOT}

# Tests for various environment variables
builddir:
	# Test PKG_CONFIG_TOP_BUILD_DIR

logfile:
	# Test PKG_CONFIG_LOG
	@echo "[/usr/bin/pkg-config] [--exists] [sysroot >= 0.0.0]" > ${WANT}
	@rm -f ${GOT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_LOG=${GOT} \
		${PKG_CONFIG} --exists "sysroot >= 0.0.0"
	@diff -u ${WANT} ${GOT}

sysroot-cflags:
	# Test PKG_CONFIG_SYSROOT_DIR (cflags)
	@echo "-I/altroot/tmp/include -I/altroot/tmp/include/foo" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		${PKG_CONFIG} --cflags sysroot > ${GOT}
	@diff -u ${WANT} ${GOT}

sysroot-libs:
	# Test PKG_CONFIG_SYSROOT_DIR (libs)
	@echo "-L/altroot/tmp/lib -L/altroot/tmp/lib/foo -lc" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		${PKG_CONFIG} --libs sysroot > ${GOT}
	@diff -u ${WANT} ${GOT}

# Tests for static linking
static-libs1:
	# Test grabbing Libs.private from this package

static-libs2:
	# Test grabbing Libs.private from other packages in order

# Misc. tests
define-variable:
	# Test --define-variable

# This fails as we read and check Requires as well as Requires.private where
# only Requires needs to be checked here as we ask for Libs and not Cflags.
# XXX: Test Requires.private too.
missing-req-1:
	# Test for missing packages in Requires (libs)
	@echo "Package nonexisting was not found in the pkg-config search path" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --libs \
		--errors-to-stdout --print-errors missing-req > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

missing-req-2:
	# Test for missing packages in Requires (cflags)
	@echo "Package nonexisting was not found in the pkg-config search path" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --cflags \
		--errors-to-stdout --print-errors missing-req > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

missing-req-3:
	# Test for missing packages in Requires (version)
	@echo "0.0.0" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --modversion \
		--errors-to-stdout --print-errors missing-req > ${GOT}
	@diff -u ${WANT} ${GOT}

whitespace-cflags:
	# Test whitespace on various positions in Cflags
	@echo "-I/tmp/include -I/tmp/include/include\ dir" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --cflags \
		--errors-to-stdout --print-errors whitespace > ${GOT}
	@diff -u ${WANT} ${GOT}

whitespace-libs:
	# Test whitespace on various positions in Libs
	@echo "-L/tmp/lib -L/tmp/lib/foo -lbaz\ quux" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --libs \
		--errors-to-stdout --print-errors whitespace > ${GOT}
	@diff -u ${WANT} ${GOT}

whitespace-name:
	# Test whitespace in Name
	@echo "Requested 'whitespace > 0.2.0' but version of Whitespace test is 0.0.0" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ ${PKG_CONFIG} --errors-to-stdout \
		--print-errors "whitespace > 0.2.0" > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

clean:
	rm -f *.want *.got

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
