#	$OpenBSD: Makefile,v 1.3 2011/06/06 17:35:33 jasper Exp $

#REGRESS_TARGETS=cmp-vers1 cmp-vers2 cmp-vers3 cmp-vers4 cmp-vers5 cmp-vers6 \
#	builddir define-variable
REGRESS_TARGETS=	corrupt1 \
			corrupt2 \
			print-provides \
			print-req \
			print-req-priv \
			sysroot-cflags \
			sysroot-libs

# Checks for corrupt pkg-config files.
corrupt1:
	# Test for missing variables/fields
	@echo "Package 'corrupt1' has no Version: field" > $@.want
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--exists $@ 2> $@.got; then false; fi
	@diff -u $@.want $@.got

corrupt2:
	# Test for missing variables/fields
	@echo "Package 'corrupt2' has no Name: field" > $@.want
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--exists $@ 2> $@.got; then false; fi
	@diff -u $@.want $@.got

# Checks for various printing features
print-provides:
	# Test --print-provides
	@echo "print-provides = 0.0.0" > $@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-provides $@ > $@.got
	@diff -u $@.want $@.got

print-req:
	# Test --print-requires
	@echo "print-req2>=0.0.1" > $@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-requires $@ > $@.got
	@diff -u $@.want $@.got
	
print-req-priv:
	# Test --print-requires-private
	@echo "print-req-priv2>=0.0.1" > $@.want
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-requires-private $@ > $@.got; then false; fi
	@diff -u $@.want $@.got

# Tests for version comparison
cmp-vers1:
	# Test regular versions (a < b)

cmp-vers2:
	# Test regular versions (a > b)

cmp-vers3:
	# Test regular versions (a = b)

cmp-vers4:
	# Test suffixed versions (alpha)

cmp-vers5:
	# Test suffixed versions (beta)

cmp-vers6:
	# Test suffixed versions (rc)

# Tests for various environment variables
builddir:
	# Test PKG_CONFIG_TOP_BUILD_DIR

sysroot-cflags:
	# Test PKG_CONFIG_SYSROOT_DIR (cflags)
	@echo "-I/altroot/tmp/include -I/altroot/tmp/include/foo" > $@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		pkg-config --cflags sysroot > $@.got
	@diff -u $@.want $@.got

sysroot-libs:
	# Test PKG_CONFIG_SYSROOT_DIR (libs)
	@echo "-L/altroot/tmp/lib -L/altroot/tmp/lib/foo -lc" > $@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		pkg-config --libs sysroot > $@.got
	@diff -u $@.want $@.got

# Misc. tests
define-variable:
	# Test --define-variable

clean:
	rm -f *.want *.got

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
