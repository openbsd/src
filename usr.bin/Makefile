#	$OpenBSD: Makefile,v 1.9 1996/06/17 02:14:15 downsj Exp $
#	$NetBSD: Makefile,v 1.62 1996/03/10 05:45:43 thorpej Exp $
#	from: @(#)Makefile	5.8.1.1 (Berkeley) 5/8/91

SUBDIR=	apply apropos asa at banner basename bdes biff cal calendar cap_mkdb \
	checknr chflags chpass cksum cmp col colcrt colrm column comm \
	compress cpp crontab ctags cut dirname du \
	env error expand false file find finger fmt fold fpr from \
	fsplit fstat ftp gencat getconf getopt head hexdump id indent \
	ipcrm ipcs join jot kdump ktrace lam last lastcomm leave less lex \
	locate lock logger login logname look lorder m4 machine mail make man \
	mesg mkdep mkfifo mkstr modstat msgs netstat newsyslog nfsstat nice \
	nohup oldrdist pagesize passwd paste patch pr printenv printf quota \
	rdist rdistd renice rev rlogin rpcgen rpcinfo rs \
	rsh rup ruptime rusers rwall rwho \
	script sed shar showmount skey skeyinit soelim split strings \
	su sup systat tail talk tcopy tee telnet tftp time \
	tip tn3270 touch tput tr true tset tsort tty ul uname unexpand \
	unifdef uniq units unvis users uudecode uuencode \
	vacation vgrind vi vis vmstat w wall wc what whatis whereis \
	which who whois window write \
	xargs xinstall xlint xstr yacc yes ypcat ypmatch ypwhich

# Don't use the standard tool chain on the alpha or mips.
.if (${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "mips")
SUBDIR+= ar gprof nm ranlib size strip
.endif

.include <bsd.subdir.mk>
