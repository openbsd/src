#	$OpenBSD: Makefile,v 1.79 2002/07/24 01:08:56 art Exp $

.include <bsd.own.mk>

SUBDIR= apply apropos arch asa at aucat audioctl awk banner basename bdes \
	biff cal calendar cap_mkdb cdio checknr chpass cksum cmp col \
	colcrt colrm column comm compile_et compress cpp crontab ctags cut \
	deroff dirname du elf2olf encrypt env error expand false file file2c \
	find fgen finger fmt fold from fsplit fstat ftp gencat getconf getopt \
	gprof head hexdump id indent infocmp ipcrm ipcs join jot kdump keynote \
	ktrace lam last lastcomm leave less lex lndir locate lock logger \
	login logname look lorder m4 mail make man mesg mg midiplay mixerctl \
	mkdep mkstr mktemp modstat msgs nc netstat newsyslog nfsstat nice \
	nohup oldrdist pagesize passwd paste patch pctr pr printenv printf \
	quota radioctl rdist rdistd readlink renice rev rpcgen rpcinfo \
	rs rsh \
	rup ruptime rusers rwall rwho script sectok sed shar showmount skey \
	skeyaudit skeyinfo skeyinit sort spell split ssh su sup systat sudo \
	tail talk tcopy tee telnet tftp tic time tip tn3270 top touch tput tr \
	true tset tsort tty usbhidaction usbhidctl ul uname unexpand unifdef \
	uniq units \
	unvis users uudecode uuencode vacation vgrind vi vis vmstat w wall wc \
	what whatis which who whois window write x99token xargs xinstall xlint \
	xstr yacc yes

.if (${YP:L} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

.if (${ELF_TOOLCHAIN} != "yes")
SUBDIR+= ar nm ranlib size strip strings
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= elf2ecoff elf2aout
.endif

.include <bsd.subdir.mk>
