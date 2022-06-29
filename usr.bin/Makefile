#	$OpenBSD: Makefile,v 1.168 2022/06/29 08:39:49 job Exp $

.include <bsd.own.mk>

SUBDIR= apply arch at aucat audioctl awk banner \
	basename bc bgplg \
	biff cal calendar cap_mkdb cdio chpass cmp col colrm \
	column comm compress cpp crontab csplit ctags ctfconv ctfdump \
	cu cut cvs \
	dc deroff diff diff3 dig dirname doas du encrypt env expand false file \
	find fgen finger fmt fold from fstat ftp gencat getcap \
	getconf getent getopt gprof grep head hexdump htpasswd id indent \
	infocmp ipcrm ipcs \
	join jot kdump keynote kstat ktrace \
	lam last lastcomm ldap leave less lex \
	libtool lndir \
	locale locate lock logger login logname look lorder \
	m4 mail make mandoc mesg mg \
	midicat mixerctl mkdep mklocale mktemp nc netstat \
	newsyslog \
	nfsstat nice nm nl nohup openssl pagesize passwd paste patch pctr \
	pkg-config pkill \
	pr printenv printf quota radioctl rcs rdist rdistd \
	readlink realpath renice rev rpcgen rpcinfo rs rsync rup rusers rwall \
	sdiff seq script sed sendbug shar showmount signify skey \
	skeyaudit skeyinfo skeyinit sndioctl sndiod snmp \
	sort spell split ssh stat su systat \
	tail talk tcpbench tee telnet tftp tic time timeout \
	tmux top touch tput tr true ts tset tsort tty usbhidaction usbhidctl \
	ul uname unexpand unifdef uniq units \
	unvis users uudecode uuencode vacation vi vis vmstat w wall wc \
	what which who whois write x99token xargs xinstall \
	yacc yes

.if (${YP:L} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

.include <bsd.subdir.mk>
