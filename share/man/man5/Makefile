#	$OpenBSD: Makefile,v 1.29 2002/10/13 21:21:21 miod Exp $
#	$NetBSD: Makefile,v 1.14 1995/05/11 23:13:15 cgd Exp $

# missing: dump.5 plot.5

MAN=	a.out.5 acct.5 bsd.port.mk.5 bsd.regress.mk.5 core.5 dir.5 disktab.5 \
	ethers.5 fbtab.5 files.conf.5 forward.5 fs.5 fstab.5 genassym.cf.5 \
	group.5 hostname.if.5 hosts.equiv.5 hosts.5 intro.5 link.5 \
	login.conf.5 moduli.5 motd.5 netgroup.5 networks.5 passwd.5 \
	passwd.conf.5 pf.conf.5 phones.5 printcap.5 protocols.5 remote.5 \
	resolv.conf.5 rpc.5 services.5 shells.5 stab.5 sysctl.conf.5 types.5 \
	utmp.5 wsconsctl.conf.5
MLINKS= dir.5 dirent.5 fs.5 inode.5 utmp.5 wtmp.5 utmp.5 lastlog.5
MLINKS+= hosts.equiv.5 .rhosts.5
MLINKS+= resolv.conf.5 resolver.5
MLINKS+= hostname.if.5 bridgename.if.5
MLINKS+= passwd.5 master.passwd.5

.include <bsd.prog.mk>
