#	from: @(#)Makefile	5.8.1.1 (Berkeley) 5/8/91
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:44:52 deraadt Exp $

SUBDIR=	apply apropos asa at banner basename bdes biff cal calendar cap_mkdb \
	checknr chflags chpass cksum cmp col colcrt colrm column comm \
	compress crontab ctags cut dirname du \
	env error expand false file find finger fmt fold fpr from \
	fsplit fstat ftp gencat getconf getopt head hexdump id indent \
	ipcrm ipcs join jot kdump ktrace lam last lastcomm leave lex locate \
	lock logger login logname look lorder m4 machine mail make man mesg \
	mkdep mkfifo mkstr modstat more msgs netstat newsyslog nfsstat nice \
	nohup pagesize passwd paste patch pr printenv printf quota \
	rdist renice rev rlogin rpcgen rpcinfo rs \
	rsh rup ruptime rusers rwall rwho \
	script sed shar showmount skey skeyinit soelim split strings \
	su systat tail talk tcopy tee telnet tftp time \
	tip tn3270 touch tput tr true tset tsort tty ul uname unexpand \
	unifdef uniq units unvis users uudecode uuencode \
	vacation vgrind vi vis vmstat w wall wc what whatis whereis \
	which who whois window write \
	xargs xinstall xlint xstr yacc yes ypcat ypmatch ypwhich

# Don't use the standard tool chain on the alpha.
.if (${MACHINE_ARCH} != "alpha")
SUBDIR+= ar gprof nm ranlib size strip
.endif
.if (${MACHINE_ARCH} == "sparc")
SUBDIR+= fdeject
.endif

.include <bsd.subdir.mk>
