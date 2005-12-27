#	$OpenBSD: Makefile,v 1.99 2005/12/27 18:44:02 deraadt Exp $

.include <bsd.own.mk>

SUBDIR= apply apropos ar arch asa asn1_compile at aucat audioctl awk banner \
	basename bc bdes \
	biff cal calendar cap_mkdb cdio checknr chpass cmp col \
	colcrt colrm column comm compile_et compress cpp crontab ctags cut \
	dc deroff diff diff3 dirname du elf2olf encrypt env expand false file \
	file2c find fgen finger fmt fold from fsplit fstat ftp gencat getcap \
	getconf getent getopt gprof grep gzsig head hexdump id indent \
	infocmp ipcrm ipcs \
	join jot kdump keynote ktrace lam last lastcomm leave less lex lndir \
	locate lock logger login logname look lorder m4 mail make man mesg mg \
	midiplay mixerctl mkdep mklocale mkstr mktemp modstat msgs nc netstat \
	newsyslog \
	nfsstat nice nm nohup oldrdist pagesize passwd paste patch pctr pkill \
	pmdb pr printenv printf quota radioctl ranlib rdist rdistd readlink \
	renice rev rpcgen rpcinfo rs rsh rup ruptime rusers rwall rwho \
	sdiff script sectok sed shar showmount skey \
	skeyaudit skeyinfo skeyinit sort spell split ssh stat su sup systat \
	sudo tail talk tcopy tee telnet tftp tic time tip tn3270 top touch \
	tput tr true tset tsort tty usbhidaction usbhidctl ul uname \
	unexpand unifdef uniq units \
	unvis users uudecode uuencode vacation vgrind vi vis vmstat w wall wc \
	what whatis which who whois window write x99token xargs xinstall xlint \
	xstr yacc yes

.if (${YP:L} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

.if (${ELF_TOOLCHAIN:L} == "no")
SUBDIR+= strip strings
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= elf2ecoff elf2aout
.endif

.include <bsd.subdir.mk>
