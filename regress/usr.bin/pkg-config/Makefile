#	$OpenBSD: Makefile,v 1.1 2011/06/06 13:58:03 jasper Exp $

#REGRESS_TARGETS=cmp-vers1 cmp-vers2 cmp-vers3 cmp-vers4 cmp-vers5 cmp-vers6 \
#	sysroot builddir define-variable print-provides print-req \
#	print-req-priv corrupt1 corrupt2
REGRESS_TARGETS=corrupt1 corrupt2 print-req-priv

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

sysroot:
	# Test PKG_CONFIG_SYSROOT_DIR

builddir:
	# Test PKG_CONFIG_TOP_BUILD_DIR

define-variable:
	# Test --define-variable

print-provides:
	# Test --print-provides

print-req:
	# Test --print-requires
	
print-req-priv:
	# Test --print-requires-private
	@echo "print-req-priv2>=0.0.1" > $@.want
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-requires-private $@ > $@.got; then false; fi
	@diff -u $@.want $@.got

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

clean:
	rm -f *.want *.got

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
