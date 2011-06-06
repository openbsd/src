#	$OpenBSD: Makefile,v 1.5 2011/06/06 17:49:23 jasper Exp $

REGRESS_TARGETS=	corrupt1 \
			corrupt2 \
			print-provides \
			print-req \
			print-req-priv \
			logfile \
			sysroot-cflags \
			sysroot-libs

# Checks for corrupt pkg-config files.
corrupt1:
	# Test for missing variables/fields
	@echo "Package 'corrupt1' has no Version: field" > ${.OBJDIR}/$@.want
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--exists $@ 2> ${.OBJDIR}/$@.got; then false; fi
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got

corrupt2:
	# Test for missing variables/fields
	@echo "Package 'corrupt2' has no Name: field" > ${.OBJDIR}/$@.want
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--exists $@ 2> ${.OBJDIR}/$@.got; then false; fi
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got

# Checks for various printing features
print-provides:
	# Test --print-provides
	@echo "print-provides = 0.0.0" > ${.OBJDIR}/$@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-provides $@ > ${.OBJDIR}/$@.got
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got

print-req:
	# Test --print-requires
	@echo "print-req2>=0.0.1" > ${.OBJDIR}/$@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-requires $@ > $@.got
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got
	
print-req-priv:
	# Test --print-requires-private
	@echo "print-req-priv2>=0.0.1" > ${.OBJDIR}/$@.want
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-requires-private $@ > ${.OBJDIR}/$@.got; then false; fi
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got

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

logfile:
	# Test PKG_CONFIG_LOG
	@echo "[/usr/bin/pkg-config] [--exists] [sysroot >= 0.0.0]" > ${.OBJDIR}/$@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_LOG=${.OBJDIR}/$@.got \
		pkg-config --exists "sysroot >= 0.0.0"
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got

sysroot-cflags:
	# Test PKG_CONFIG_SYSROOT_DIR (cflags)
	@echo "-I/altroot/tmp/include -I/altroot/tmp/include/foo" > ${.OBJDIR}/$@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		pkg-config --cflags sysroot > ${.OBJDIR}/$@.got
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got

sysroot-libs:
	# Test PKG_CONFIG_SYSROOT_DIR (libs)
	@echo "-L/altroot/tmp/lib -L/altroot/tmp/lib/foo -lc" > ${.OBJDIR}/$@.want
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		pkg-config --libs sysroot > ${.OBJDIR}/$@.got
	@diff -u ${.OBJDIR}/$@.want ${.OBJDIR}/$@.got

# Misc. tests
define-variable:
	# Test --define-variable

clean:
	rm -f *.want *.got

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
