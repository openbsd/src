#	$OpenBSD: Makefile.inc,v 1.8 2019/11/10 22:18:14 guenther Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables

CHECK_LDSO=c() {							\
	! readelf -Wr $$1 |						\
	  egrep -qv '^($$|[ R])| 0+(1203)? R_MIPS_(NONE|REL32) ';	\
	! readelf -Ws $$1 |						\
	  egrep -qv '^($$|S| *N| *[0-9]+:.*(LOCAL|GLOBAL +DEFAULT +[0-9]+ ))'; \
	}; c

