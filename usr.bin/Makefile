#	$OpenBSD: Makefile,v 1.28 1996/10/04 19:16:51 deraadt Exp $
#	$NetBSD: Makefile,v 1.62 1996/03/10 05:45:43 thorpej Exp $
#	from: @(#)Makefile	5.8.1.1 (Berkeley) 5/8/91

.include <bsd.own.mk>

SUBDIR=	apply apropos arch asa at awk banner basename bdes biff cal calendar \
	cap_mkdb cdio checknr chflags chpass cksum cmp col colcrt colrm \
	column comm compress cpp crontab ctags cut dirname du encrypt \
	env error expand false file file2c find finger fmt fold from \
	fsplit fstat ftp gencat getconf getopt head hexdump id indent \
	info_mkdb ipcrm ipcs join jot kdump ktrace lam last lastcomm leave \
	less lex lndir locate lock logger login logname look lorder m4 machine \
	mail make man mesg mkdep mkfifo mkstr modstat msgs nc \
	netstat newsyslog \
	nfsstat nice nohup oldrdist pagesize passwd paste patch pr printenv \
	printf quota rdist rdistd renice rev rlogin rpcgen rpcinfo rs \
	rsh rup ruptime rusers rwall rwho \
	script sed shar showmount skey skeyinit soelim split \
	su sup systat tail talk tcopy tee telnet tftp time \
	tip tn3270 touch tput tr true tset tsort tty ul uname unexpand \
	unifdef uniq units unvis users uudecode uuencode \
	vacation vgrind vi vim vis vmstat w wall wc what whatis whereis \
	which who whois window write \
	xargs xinstall xlint xstr yacc yes \
	gprof

.if defined(YP)
SUBDIR+=ypcat ypmatch ypwhich
.endif

# Don't use the standard tool chain on the alpha or mips.
.if (${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "mips")
SUBDIR+= ar nm ranlib size strip strings
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= pctr elf2ecoff
.elif ${MACHINE_ARCH} == "i386"
SUBDIR+= pctr
.elif ${MACHINE_ARCH} == "mips"
SUBDIR+= elf2ecoff
.endif

.include <bsd.subdir.mk>
