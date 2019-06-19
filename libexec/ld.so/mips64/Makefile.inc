#	$OpenBSD: Makefile.inc,v 1.5 2019/04/19 08:59:40 visa Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
