#	$OpenBSD: Makefile.inc,v 1.4 2019/07/01 15:58:38 kettenis Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
CFLAGS += -fPIC -msoft-float
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
