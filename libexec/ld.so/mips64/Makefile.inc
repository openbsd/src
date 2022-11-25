#	$OpenBSD: Makefile.inc,v 1.9 2022/11/25 14:56:56 visa Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

CHECK_LDSO=c() {							\
	! readelf -Wr $$1 |						\
	  egrep -qv '^($$|[ R])| 0+(1203)? R_MIPS_(NONE|REL32) ';	\
	! readelf -Ws $$1 |						\
	  egrep -qv '^($$|S| *N| *[0-9]+:.*(LOCAL|GLOBAL +DEFAULT +[0-9]+ ))'; \
	}; c

