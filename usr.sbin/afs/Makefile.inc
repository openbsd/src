OS_VER	!= uname -r
BINDIR	?=/usr/sbin
CFLAGS	+= -DHAVE_CONFIG_H -DSYSCONFDIR=\"/etc\" \
	   -DARLACACHEDIR=\"/var/spool/afs\"\
	   -DARLACONFFILE=\"/etc/arla.conf\" -DKERBEROS \
	   -DARLAVENDOR=\"unknown\" -DARLACPU=\"${MACHINE_ARCH}\" \
	   -DARLAOS=\"openbsd${OS_VER}\" \

.if (${MACHINE_ARCH} == "sparc")
CFLAGS	+= -DBROKEN_MMAP
.endif

