#	$OpenBSD: Makefile,v 1.91 2005/02/19 22:15:41 millert Exp $

.include <bsd.own.mk>

SUBDIR= apply apropos arch asa asn1_compile at aucat audioctl awk banner \
	basename bc bdes \
	biff cal calendar cap_mkdb cdio checknr chpass cmp col \
	colcrt colrm column comm compile_et compress cpp crontab ctags cut \
	dc deroff diff diff3 dirname du elf2olf encrypt env expand false file \
	file2c find fgen finger fmt fold from fsplit fstat ftp gencat getcap \
	getconf getopt gprof grep head hexdump id indent infocmp ipcrm ipcs \
	join jot kdump keynote ktrace lam last lastcomm leave less lex lndir \
	locate lock logger login logname look lorder m4 mail make man mesg mg \
	midiplay mixerctl mkdep mkstr mktemp modstat msgs nc netstat newsyslog \
	nfsstat nice nm nohup oldrdist pagesize passwd paste patch pctr pkill \
	pmdb pr printenv printf quota radioctl rdist rdistd readlink renice \
	rev rpcgen rpcinfo rs rsh \
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

.if (${ELF_TOOLCHAIN:L} == "no")
SUBDIR+= ar ranlib strip strings
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= elf2ecoff elf2aout
.endif

.include <bsd.subdir.mk>
