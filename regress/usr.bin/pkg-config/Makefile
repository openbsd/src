#	$OpenBSD: Makefile,v 1.7 2011/06/06 18:59:44 jasper Exp $

REGRESS_TARGETS=cmp-vers1-1 \
		cmp-vers1-2 \
		cmp-vers1-3 \
		cmp-vers2-1 \
		cmp-vers2-2 \
		cmp-vers2-3 \
		cmp-vers2-4 \
		cmp-vers3 \
		corrupt1 \
		corrupt2 \
		print-provides \
		print-req \
		print-req-priv \
		logfile \
		sysroot-cflags \
		sysroot-libs

WANT=		${.OBJDIR}/$@.want
GOT=		${.OBJDIR}/$@.got

# Tests for corrupt pkg-config files.
corrupt1:
	# Test for missing variables/fields
	@echo "Package 'corrupt1' has no Version: field" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--exists $@ 2> ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

corrupt2:
	# Test for missing variables/fields
	@echo "Package 'corrupt2' has no Name: field" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--exists $@ 2> ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

# Tests for various printing features
print-provides:
	# Test --print-provides
	@echo "print-provides = 0.0.0" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-provides $@ > ${GOT}
	@diff -u ${WANT} ${GOT}

print-req:
	# Test --print-requires
	@echo "print-req2>=0.0.1" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-requires $@ > $@.got
	@diff -u ${WANT} ${GOT}
	
print-req-priv:
	# Test --print-requires-private
	@echo "print-req-priv2>=0.0.1" > ${WANT}
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config \
		--print-requires-private $@ > ${GOT}; then false; fi
	@diff -u ${WANT} ${GOT}

# Tests for version comparison
cmp-vers1-1:
	# Test regular versions (a <= b)
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists \
		"vers1 <= 0.0.2"; then false; fi

cmp-vers1-2:
	# Test regular versions (a <= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists "vers1 <= 0.1.0"

cmp-vers1-3:
	# Test regular versions (a < b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists "vers1 < 0.2"

cmp-vers2-1:
	# Test regular versions (a >= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists "vers2 >= 0.0.1"

cmp-vers2-2:
	# Test regular versions (a >= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists "vers2 >= 0.0.0"

cmp-vers2-3:
	# Test regular versions (a >= b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists "vers2 >= 0.1.0"

cmp-vers2-4:
	# Test regular versions (a >= b)
	@if PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists \
		"vers2 > 0.2.0.0"; then false; fi

cmp-vers3:
	# Test regular versions (a = b)
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ pkg-config --exists "vers2 = 0.2.0"

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
	@echo "[/usr/bin/pkg-config] [--exists] [sysroot >= 0.0.0]" > ${WANT}
	@rm -f ${GOT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_LOG=${GOT} \
		pkg-config --exists "sysroot >= 0.0.0"
	@diff -u ${WANT} ${GOT}

sysroot-cflags:
	# Test PKG_CONFIG_SYSROOT_DIR (cflags)
	@echo "-I/altroot/tmp/include -I/altroot/tmp/include/foo" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		pkg-config --cflags sysroot > ${GOT}
	@diff -u ${WANT} ${GOT}

sysroot-libs:
	# Test PKG_CONFIG_SYSROOT_DIR (libs)
	@echo "-L/altroot/tmp/lib -L/altroot/tmp/lib/foo -lc" > ${WANT}
	@PKG_CONFIG_PATH=${.CURDIR}/pcdir/ PKG_CONFIG_SYSROOT_DIR=/altroot \
		pkg-config --libs sysroot > ${GOT}
	@diff -u ${WANT} ${GOT}

# Misc. tests
define-variable:
	# Test --define-variable

clean:
	rm -f *.want *.got

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
