#	$OpenBSD: Makefile,v 1.48 1998/04/26 21:58:16 provos Exp $

.include <bsd.own.mk>

SUBDIR= apply apropos arch asa at aucat audioctl awk banner basename bdes biff \
	cat calendar cap_mkdb cdio checknr chflags chpass cksum cmp col colcrt \
	colrm column comm compile_et compress cpp crontab ctags cut dirname \
	du elf2olf encrypt env error expand false file file2c find finger fmt \
	fold from fsplit fstat ftp gencat getconf getopt gprof head hexdump \
	id indent \
	info_mkdb ipcrm ipcs join jot kdump ktrace lam last lastcomm leave \
	less lex lndir locate lock logger login logname look lorder m4 \
	machine mail make man mesg mixerctl mk_cmds mkdep mkfifo mkstr mktemp \
	modstat msgs nc netstat newsyslog \
	nfsstat nice nohup oldrdist pagesize passwd paste patch pr printenv \
	printf quota rdist rdistd readlink renice rev rlogin rpcgen rpcinfo rs \
	rsh rup ruptime rusers rwall rwho \
	script sed shar showmount skey skeyaudit skeyinfo skeyinit soelim sort \
	split su sup systat tail talk tcopy tee telnet tftp time \
	tip tn3270 top touch tput tr true tset tsort tty ul uname unexpand \
	unifdef uniq units unvis users uudecode uuencode \
	vacation vgrind vi vis vmstat w wall wc what whatis \
	which who whois window write \
	xargs xinstall xlint xstr yacc yes

.if (${YP} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

# Don't use the standard tool chain on the alpha or mips.
.if (${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "mips" && ${MACHINE_ARCH} != "powerpc")
SUBDIR+= ar nm ranlib size strip strings
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= pctr elf2ecoff elf2aout
.elif ${MACHINE_ARCH} == "i386"
SUBDIR+= pctr
.elif ${MACHINE_ARCH} == "mips"
SUBDIR+= elf2ecoff elf2aout
.endif

.include <bsd.subdir.mk>
