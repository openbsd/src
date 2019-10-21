#	$OpenBSD: Makefile.inc,v 1.7 2019/10/21 02:36:43 guenther Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
