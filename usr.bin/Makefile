#	$OpenBSD: Makefile,v 1.128 2013/03/10 17:55:54 nicm Exp $

.include <bsd.own.mk>

SUBDIR= apply apropos ar arch asa asn1_compile at aucat audioctl awk banner \
	basename bc bdes bgplg \
	biff cal calendar cap_mkdb cdio chpass cmp col colrm \
	column comm compile_et compress cpp crontab csplit ctags cu cut \
	dc deroff diff diff3 dirname du encrypt env expand false file \
	file2c find fgen finger fmt fold from fsplit fstat ftp gencat getcap \
	getconf getent getopt gprof grep gzsig head hexdump id indent \
	infocmp ipcrm ipcs \
	join jot kdump keynote ktrace lam last lastcomm leave less lex \
	libtool lndir \
	locate lock logger login logname look lorder \
	m4 mail make man mandoc mesg mg \
	midiplay mixerctl mkdep mklocale mkstr mktemp modstat nc netstat \
	newsyslog \
	nfsstat nice nm nohup oldrdist pagesize passwd paste patch pctr \
	pkg-config pkill \
	pr printenv printf quota radioctl ranlib rcs rdist rdistd \
	readlink renice rev rpcgen rpcinfo rs rsh rup ruptime rusers rwall \
	rwho sdiff script sed sendbug shar showmount skey \
	skeyaudit skeyinfo skeyinit sndiod \
	sort spell split sqlite3 ssh stat su systat \
	sudo tail talk tcopy tcpbench tee telnet tftp tic tip time \
	tmux top touch tput tr true tset tsort tty usbhidaction usbhidctl \
	ul uname unexpand unifdef uniq units \
	unvis users uudecode uuencode vacation vi vis vmstat w wall wc \
	what whatis which who whois write x99token xargs xinstall \
	xstr yacc yes

.if (${YP:L} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

.if (${ELF_TOOLCHAIN:L} == "no")
SUBDIR+= strip strings
.endif

.include <bsd.subdir.mk>
