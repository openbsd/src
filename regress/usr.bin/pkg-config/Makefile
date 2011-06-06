#	$OpenBSD: Makefile,v 1.2 2011/06/06 14:31:15 jasper Exp $

#REGRESS_TARGETS=cmp-vers1 cmp-vers2 cmp-vers3 cmp-vers4 cmp-vers5 cmp-vers6 \
#	sysroot builddir define-variable
REGRESS_TARGETS=	corrupt1 \
			corrupt2 \
			print-provides \
			print-req \
			print-req-priv

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
sysroot:
	# Test PKG_CONFIG_SYSROOT_DIR

builddir:
	# Test PKG_CONFIG_TOP_BUILD_DIR

# Misc. tests
define-variable:
	# Test --define-variable

clean:
	rm -f *.want *.got

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
