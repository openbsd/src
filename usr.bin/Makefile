#	$OpenBSD: Makefile,v 1.159 2018/11/30 07:08:24 ratchov Exp $

.include <bsd.own.mk>

SUBDIR= apply arch at aucat audioctl awk banner \
	basename bc bgplg \
	biff cal calendar cap_mkdb cdio chpass cmp col colrm \
	column comm compress cpp crontab csplit ctags ctfconv ctfdump \
	cu cut cvs \
	dc deroff diff diff3 dirname doas du encrypt env expand false file \
	find fgen finger fmt fold from fstat ftp gencat getcap \
	getconf getent getopt gprof grep head hexdump htpasswd id indent \
	infocmp ipcrm ipcs \
	join jot kdump keynote ktrace lam last lastcomm ldap leave less lex \
	libtool lndir \
	locale locate lock logger login logname look lorder \
	m4 mail make mandoc mesg mg \
	midicat mixerctl mkdep mklocale mktemp nc netstat \
	newsyslog \
	nfsstat nice nm nl nohup openssl pagesize passwd paste patch pctr \
	pkg-config pkill \
	pr printenv printf quota radioctl rcs rdist rdistd \
	readlink renice rev rpcgen rpcinfo rs rup rusers rwall \
	sdiff script sed sendbug shar showmount signify skey \
	skeyaudit skeyinfo skeyinit sndiod \
	sort spell split ssh stat su systat \
	tail talk tcpbench tee telnet tftp tic time \
	tmux top touch tput tr true tset tsort tty usbhidaction usbhidctl \
	ul uname unexpand unifdef uniq units \
	unvis users uudecode uuencode vacation vi vis vmstat w wall wc \
	what which who whois write x99token xargs xinstall \
	yacc yes

.if (${YP:L} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

.include <bsd.subdir.mk>
