/*	$OpenBSD: svr4_stream.c,v 1.5 1996/05/22 11:45:00 deraadt Exp $	 */
/*	$NetBSD: svr4_stream.c,v 1.14 1996/05/13 16:57:50 christos Exp $	 */
/*
 * Copyright (c) 1994, 1996 Christos Zoulas.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Pretend that we have streams...
 * Yes, this is gross.
 *
 * ToDo: The state machine for getmsg needs re-thinking
 *       We really need I_FDINSERT and it is going to be a pain.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/stat.h>

#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_stropts.h>
#include <compat/svr4/svr4_timod.h>
#include <compat/svr4/svr4_sockmod.h>
#include <compat/svr4/svr4_ioctl.h>
#include <compat/svr4/svr4_socket.h>

/* Utils */
static int clean_pipe __P((struct proc *, const char *));
static void getparm __P((struct socket *, struct svr4_si_sockparms *));

/* Address Conversions */
static void sockaddr_to_netaddr_in __P((struct svr4_strmcmd *,
					const struct sockaddr_in *));
static void sockaddr_to_netaddr_un __P((struct svr4_strmcmd *,
					const struct sockaddr_un *));
static void netaddr_to_sockaddr_in __P((struct sockaddr_in *,
					const struct svr4_strmcmd *));
static void netaddr_to_sockaddr_un __P((struct sockaddr_un *,
					const struct svr4_strmcmd *));

/* stream ioctls */
static int i_nread __P((struct file *, struct proc *, register_t *, int,
			u_long, caddr_t));
static int i_str   __P((struct file *, struct proc *, register_t *, int,
			u_long, caddr_t));

/* i_str sockmod calls */
static int sockmod       __P((struct file *, int, struct svr4_strioctl *,
			      struct proc *));
static int si_listen     __P((struct file *, int, struct svr4_strioctl *,
			      struct proc *));
static int si_ogetudata  __P((struct file *, int, struct svr4_strioctl *,
			      struct proc *));
static int si_sockparams __P((struct file *, int, struct svr4_strioctl *,
			      struct proc *));
static int si_getudata   __P((struct file *, int, struct svr4_strioctl *,
			      struct proc *));

/* i_str timod calls */
static int timod         __P((struct file *, int, struct svr4_strioctl *,
		              struct proc *));
static int ti_getinfo    __P((struct file *, int, struct svr4_strioctl *,
			      struct proc *));
static int ti_bind       __P((struct file *, int, struct svr4_strioctl *,
			      struct proc *));

#ifdef DEBUG_SVR4
static int show_ioc __P((const char *, struct svr4_strioctl *));
static int show_strbuf __P((struct svr4_strbuf *));
static void show_msg __P((const char *, int, struct svr4_strbuf *, 
			  struct svr4_strbuf *, int));
static void show_strmcmd __P((const char *, struct svr4_strmcmd *));

static int
show_ioc(str, ioc)
	const char		*str;
	struct svr4_strioctl	*ioc;
{
	char *ptr = (char *) malloc(ioc->len, M_TEMP, M_WAITOK);
	int error;
	int i;

	printf("%s cmd = %ld, timeout = %d, len = %d, buf = %p { ",
	       str, ioc->cmd, ioc->timeout, ioc->len, ioc->buf);

	if ((error = copyin(ioc->buf, ptr, ioc->len)) != 0) {
		free((char *) ptr, M_TEMP);
		return error;
	}

	for (i = 0; i < ioc->len; i++)
		printf("%x ", (unsigned char) ptr[i]);

	printf("}\n");

	free((char *) ptr, M_TEMP);
	return 0;
}


static int
show_strbuf(str)
	struct svr4_strbuf *str;
{
	int error;
	int i;
	char *ptr = NULL;
	int maxlen = str->maxlen;
	int len = str->len;

	if (maxlen < 0)
		maxlen = 0;

	if (len >= maxlen || len <= 0)
		len = maxlen;

	if (len != 0) {
	    ptr = malloc(len, M_TEMP, M_WAITOK);

	    if ((error = copyin(str->buf, ptr, len)) != 0) {
		    free((char *) ptr, M_TEMP);
		    return error;
	    }
	}

	printf(", { %d, %d, %p=[ ", str->maxlen, str->len, str->buf);

	for (i = 0; i < len; i++)
		printf("%x ", (unsigned char) ptr[i]);

	printf("]}");

	if (ptr)
		free((char *) ptr, M_TEMP);

	return 0;
}


static void
show_msg(str, fd, ctl, dat, flags)
	const char		*str;
	int			 fd;
	struct svr4_strbuf	*ctl;
	struct svr4_strbuf	*dat;
	int			 flags;
{
	struct svr4_strbuf	buf;
	int error;

	printf("%s(%d", str, fd);
	if (ctl != NULL) {
		if ((error = copyin(ctl, &buf, sizeof(buf))) != 0)
			return;
		show_strbuf(&buf);
	}
	else 
		printf(", NULL");

	if (dat != NULL) {
		if ((error = copyin(dat, &buf, sizeof(buf))) != 0)
			return;
		show_strbuf(&buf);
	}
	else 
		printf(", NULL");

	printf(", %x);\n", flags);
}


static void
show_strmcmd(str, cmd)
	const char		*str;
	struct svr4_strmcmd	*cmd;
{
	int i;

	printf("%s cmd = %ld, len = %ld, offs = %ld { ",
	       str, cmd->cmd, cmd->len, cmd->offs);

	for (i = 0; i < sizeof(cmd->pad) / sizeof(cmd->pad[0]); i++)
		printf("%lx ", cmd->pad[i]);

	printf("}\n");
}
#endif /* DEBUG_SVR4 */


/*
 * We are faced with an interesting situation. On svr4 unix sockets
 * are really pipes. But we really have sockets, and we might as
 * well use them. At the point where svr4 calls TI_BIND, it has
 * already created a named pipe for the socket using mknod(2).
 * We need to create a socket with the same name when we bind,
 * so we need to remove the pipe before, otherwise we'll get address
 * already in use. So we *carefully* remove the pipe, to avoid
 * using this as a random file removal tool. We use system calls
 * to avoid code duplication.
 */
static int
clean_pipe(p, path)
	struct proc *p;
	const char *path;
{
	struct sys_lstat_args la;
	struct sys_unlink_args ua;
	register_t retval;
	struct stat st;
	int error;
	caddr_t sg = stackgap_init(p->p_emul);
	size_t l = strlen(path) + 1;

	SCARG(&la, path) = stackgap_alloc(&sg, l);
	SCARG(&la, ub) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = copyout((char *) path, SCARG(&la, path), l)) != 0)
		return error;

	if ((error = sys_lstat(p, &la, &retval)) != 0)
		return 0;

	if ((error = copyin(SCARG(&la, ub), &st, sizeof(st))) != 0)
		return 0;

	/*
	 * Make sure we are dealing with a mode 0 named pipe.
	 */
	if ((st.st_mode & S_IFMT) != S_IFIFO)
		return 0;

	if ((st.st_mode & ALLPERMS) != 0)
		return 0;

	SCARG(&ua, path) = SCARG(&la, path);

	if ((error = sys_unlink(p, &ua, &retval)) != 0) {
		DPRINTF(("clean_pipe: unlink failed %d\n", error));
		return error;
	}

	return 0;
}


static void
sockaddr_to_netaddr_in(sc, sain)
	struct svr4_strmcmd *sc;
	const struct sockaddr_in *sain;
{
	struct svr4_netaddr_in *na;
	na = SVR4_ADDROF(sc);

	na->family = sain->sin_family;
	na->port = sain->sin_port;
	na->addr = sain->sin_addr.s_addr;
	DPRINTF(("sockaddr_in -> netaddr %d %d %lx\n", na->family, na->port,
		 na->addr));
}


static void
sockaddr_to_netaddr_un(sc, saun)
	struct svr4_strmcmd *sc;
	const struct sockaddr_un *saun;
{
	struct svr4_netaddr_un *na;
	char *dst, *edst = ((char *) sc) + sc->offs + sizeof(na->family) + 1  -
	    sizeof(*sc);
	const char *src;

	na = SVR4_ADDROF(sc);
	na->family = saun->sun_family;
	for (src = saun->sun_path, dst = na->path; (*dst++ = *src++) != '\0'; )
		if (dst == edst)
			break;
	DPRINTF(("sockaddr_un -> netaddr %d %s\n", na->family, na->path));
}


static void
netaddr_to_sockaddr_in(sain, sc)
	struct sockaddr_in *sain;
	const struct svr4_strmcmd *sc;
{
	const struct svr4_netaddr_in *na;


	na = SVR4_ADDROF(sc);
	bzero(sain, sizeof(*sain));
	sain->sin_len = sizeof(*sain);
	sain->sin_family = na->family;
	sain->sin_port = na->port;
	sain->sin_addr.s_addr = na->addr;
	DPRINTF(("netaddr -> sockaddr_in %d %d %x\n", sain->sin_family,
		 sain->sin_port, sain->sin_addr.s_addr));
}


static void
netaddr_to_sockaddr_un(saun, sc)
	struct sockaddr_un *saun;
	const struct svr4_strmcmd *sc;
{
	const struct svr4_netaddr_un *na;
	char *dst, *edst = &saun->sun_path[sizeof(saun->sun_path) - 1];
	const char *src;

	na = SVR4_ADDROF(sc);
	bzero(saun, sizeof(*saun));
	saun->sun_family = na->family;
	for (src = na->path, dst = saun->sun_path; (*dst++ = *src++) != '\0'; )
		if (dst == edst)
			break;
	saun->sun_len = dst - saun->sun_path;
	DPRINTF(("netaddr -> sockaddr_un %d %s\n", saun->sun_family,
		 saun->sun_path));
}


static void
getparm(so, pa)
	struct socket *so;
	struct svr4_si_sockparms *pa;
{
	struct svr4_strm *st = (struct svr4_strm *) so->so_internal;
	pa->family = st->s_family;

	switch (so->so_type) {
	case SOCK_DGRAM:
		pa->type = SVR4_SOCK_DGRAM;
		pa->protocol = IPPROTO_UDP;
		return;

	case SOCK_STREAM:
		pa->type = SVR4_SOCK_STREAM;
		pa->protocol = IPPROTO_TCP;
		return;

	case SOCK_RAW:
		pa->type = SVR4_SOCK_RAW;
		pa->protocol = IPPROTO_RAW;
		return;

	default:
		pa->type = 0;
		pa->protocol = 0;
		return;
	}
}


static int
si_ogetudata(fp, fd, ioc, p)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	int error;
	struct svr4_si_oudata ud;
	struct svr4_si_sockparms pa;
	struct socket *so = (struct socket *) fp->f_data;

	if (ioc->len != sizeof(ud) && ioc->len != sizeof(ud) - sizeof(int)) {
		DPRINTF(("SI_OGETUDATA: Wrong size %d != %d\n",
			 sizeof(ud), ioc->len));
		return EINVAL;
	}

	if ((error = copyin(ioc->buf, &ud, sizeof(ud))) != 0)
		return error;

	getparm(so, &pa);

	switch (pa.family) {
	case AF_INET:
	    ud.addrsize = sizeof(struct sockaddr_in);
	    break;

	case AF_UNIX:
	    ud.addrsize = sizeof(struct sockaddr_un);
	    break;

	default:
	    DPRINTF(("SI_OGETUDATA: Unsupported address family %d\n",
		     pa.family));
	    return ENOSYS;
	}

	/* I have no idea what these should be! */
	ud.tidusize = 16384;
	ud.optsize = 128;
	if (ioc->len == sizeof(ud))
	    ud.tsdusize = 128;

	if (pa.type == SVR4_SOCK_STREAM) 
		ud.etsdusize = 1;
	else
		ud.etsdusize = 0;

	ud.servtype = pa.type;

	/* XXX: Fixme */
	ud.so_state = 0;
	ud.so_options = 0;
	return copyout(&ud, ioc->buf, ioc->len);
}


static int
si_sockparams(fp, fd, ioc, p)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	struct socket *so = (struct socket *) fp->f_data;
	struct svr4_si_sockparms pa;

	getparm(so, &pa);
	return copyout(&pa, ioc->buf, sizeof(pa));
}


static int
si_listen(fp, fd, ioc, p)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	int error;
	struct socket *so = (struct socket *) fp->f_data;
	struct svr4_strm *st = (struct svr4_strm *) so->so_internal;
	register_t retval;
#if 0
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	caddr_t sg;
	void *skp, *sup;
	int sasize;
#endif
	struct svr4_strmcmd lst;
	struct sys_listen_args la;

	if ((error = copyin(ioc->buf, &lst, ioc->len)) != 0)
		return error;

#ifdef DEBUG_SVR4
	show_strmcmd(">si_listen", &lst);
#endif

#if 0
	switch (st->s_family) {
	case AF_INET:
		skp = &sain;
		sasize = sizeof(sain);

		if (lst.offs == 0)
			goto reply;

		netaddr_to_sockaddr_in(&sain, &lst);

		DPRINTF(("SI_LISTEN: fam %d, port %d, addr %x\n",
			 sain.sin_family, sain.sin_port,
			 sain.sin_addr.s_addr));
		break;

	case AF_UNIX:
		skp = &saun;
		sasize = sizeof(saun);
		if (lst.offs == 0)
			goto reply;

		netaddr_to_sockaddr_un(&saun, &lst);

		if (saun.sun_path[0] == '\0')
			goto reply;

		DPRINTF(("SI_LISTEN: fam %d, path %s\n",
			 saun.sun_family, saun.sun_path));

		if ((error = clean_pipe(p, saun.sun_path)) != 0)
			return error;
		break;

	default:
		DPRINTF(("SI_LISTEN: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	sg = stackgap_init(p->p_emul);
	sup = stackgap_alloc(&sg, sasize);

	if ((error = copyout(skp, sup, sasize)) != 0)
		return error;
#endif

	SCARG(&la, s) = fd;
	DPRINTF(("SI_LISTEN: fileno %d backlog = %d\n", fd, 5));
	SCARG(&la, backlog) = 5;

	if ((error = sys_listen(p, &la, &retval)) != 0) {
		DPRINTF(("SI_LISTEN: listen failed %d\n", error));
		return error;
	}

	st->s_cmd = SVR4_TI_ACCEPT_REPLY;

	return 0;

#if 0
reply:
	bzero(&lst, sizeof(lst));
	lst.cmd = SVR4_TI_BIND_REPLY;
	lst.len = sasize;
	lst.offs = 0x10;	/* XXX */

	ioc->len = 32;
	if ((error = copyout(&lst, ioc->buf, ioc->len)) != 0)
		return error;

	return 0;
#endif
}


static int
si_getudata(fp, fd, ioc, p)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	int error;
	struct svr4_si_udata ud;
	struct socket *so = (struct socket *) fp->f_data;

	if (sizeof(ud) != ioc->len) {
		DPRINTF(("SI_GETUDATA: Wrong size %d != %d\n",
			 sizeof(ud), ioc->len));
		return EINVAL;
	}

	if ((error = copyin(ioc->buf, &ud, sizeof(ud))) != 0)
		return error;

	getparm(so, &ud.sockparms);

	switch (ud.sockparms.family) {
	case AF_INET:
	    ud.addrsize = sizeof(struct sockaddr_in);
	    break;

	case AF_UNIX:
	    ud.addrsize = sizeof(struct sockaddr_un);
	    break;

	default:
	    DPRINTF(("SI_GETUDATA: Unsupported address family %d\n",
		     ud.sockparms.family));
	    return ENOSYS;
	}

	/* I have no idea what these should be! */
	ud.tidusize = 16384;
	ud.optsize = 128;
	ud.tsdusize = 16384;


	if (ud.sockparms.type == SVR4_SOCK_STREAM) 
		ud.etsdusize = 1;
	else
		ud.etsdusize = 0;

	ud.servtype = ud.sockparms.type;

	/* XXX: Fixme */
	ud.so_state = 0;
	ud.so_options = 0;
	return copyout(&ud, ioc->buf, sizeof(ud));
}


static int
sockmod(fp, fd, ioc, p)
	struct file		*fp;
	int			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	switch (ioc->cmd) {
	case SVR4_SI_OGETUDATA:
		DPRINTF(("SI_OGETUDATA\n"));
		return si_ogetudata(fp, fd, ioc, p);

	case SVR4_SI_SHUTDOWN:
		DPRINTF(("SI_SHUTDOWN\n"));
		return 0;

	case SVR4_SI_LISTEN:
		DPRINTF(("SI_LISTEN\n"));
		return si_listen(fp, fd, ioc, p);

	case SVR4_SI_SETMYNAME:
		DPRINTF(("SI_SETMYNAME\n"));
		return 0;

	case SVR4_SI_SETPEERNAME:
		DPRINTF(("SI_SETPEERNAME\n"));
		return 0;

	case SVR4_SI_GETINTRANSIT:
		DPRINTF(("SI_GETINTRANSIT\n"));
		return 0;

	case SVR4_SI_TCL_LINK:
		DPRINTF(("SI_TCL_LINK\n"));
		return 0;

	case SVR4_SI_TCL_UNLINK:
		DPRINTF(("SI_TCL_UNLINK\n"));
		return 0;

	case SVR4_SI_SOCKPARAMS:
		DPRINTF(("SI_SOCKPARAMS\n"));
		return si_sockparams(fp, fd, ioc, p);

	case SVR4_SI_GETUDATA:
		DPRINTF(("SI_GETUDATA\n"));
		return si_getudata(fp, fd, ioc, p);

	default:
		DPRINTF(("Unknown sockmod ioctl %lx\n", ioc->cmd));
		return 0;

	}
}


static int
ti_getinfo(fp, fd, ioc, p)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	int error;
	struct svr4_infocmd info;

	bzero(&info, sizeof(info));

	if ((error = copyin(ioc->buf, &info, ioc->len)) != 0)
		return error;

	if (info.cmd != SVR4_TI_INFO_REQUEST)
		return EINVAL;

	info.cmd = SVR4_TI_INFO_REPLY;
	info.tsdu = 0;
	info.etsdu = 1;
	info.cdata = -2;
	info.ddata = -2;
	info.addr = 16;
	info.opt = -1;
	info.tidu = 16384;
	info.serv = 2;
	info.current = 0;
	info.provider = 2;

	ioc->len = sizeof(info);
	if ((error = copyout(&info, ioc->buf, ioc->len)) != 0)
		return error;

	return 0;
}


static int
ti_bind(fp, fd, ioc, p)
	struct file		*fp;
	int 			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	int error;
	struct socket *so = (struct socket *) fp->f_data;
	struct svr4_strm *st = (struct svr4_strm *) so->so_internal;
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	register_t retval;
	caddr_t sg;
	void *skp, *sup = NULL;
	int sasize;
	struct svr4_strmcmd bnd;
	struct sys_bind_args ba;

	if ((error = copyin(ioc->buf, &bnd, ioc->len)) != 0)
		return error;

	if (bnd.cmd != SVR4_TI_BIND_REQUEST)
		return EINVAL;

	switch (st->s_family) {
	case AF_INET:
		skp = &sain;
		sasize = sizeof(sain);

		if (bnd.offs == 0)
			goto reply;

		netaddr_to_sockaddr_in(&sain, &bnd);

		DPRINTF(("TI_BIND: fam %d, port %d, addr %x\n",
			 sain.sin_family, sain.sin_port,
			 sain.sin_addr.s_addr));
		break;

	case AF_UNIX:
		skp = &saun;
		sasize = sizeof(saun);
		if (bnd.offs == 0)
			goto reply;

		netaddr_to_sockaddr_un(&saun, &bnd);

		if (saun.sun_path[0] == '\0')
			goto reply;

		DPRINTF(("TI_BIND: fam %d, path %s\n",
			 saun.sun_family, saun.sun_path));

		if ((error = clean_pipe(p, saun.sun_path)) != 0)
			return error;
		break;

	default:
		DPRINTF(("TI_BIND: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	sg = stackgap_init(p->p_emul);
	sup = stackgap_alloc(&sg, sasize);

	if ((error = copyout(skp, sup, sasize)) != 0)
		return error;

	SCARG(&ba, s) = fd;
	DPRINTF(("TI_BIND: fileno %d\n", fd));
	SCARG(&ba, name) = (caddr_t) sup;
	SCARG(&ba, namelen) = sasize;

	if ((error = sys_bind(p, &ba, &retval)) != 0) {
		DPRINTF(("TI_BIND: bind failed %d\n", error));
		return error;
	}

reply:
	if (sup == NULL) {
		bzero(&bnd, sizeof(bnd));
		bnd.len = sasize;
		bnd.offs = 0x10;	/* XXX */
	}

	bnd.cmd = SVR4_TI_BIND_REPLY;

	if ((error = copyout(&bnd, ioc->buf, ioc->len)) != 0)
		return error;

	return 0;
}


static int
timod(fp, fd, ioc, p)
	struct file		*fp;
	int			 fd;
	struct svr4_strioctl	*ioc;
	struct proc		*p;
{
	switch (ioc->cmd) {
	case SVR4_TI_GETINFO:
		DPRINTF(("TI_GETINFO\n"));
		return ti_getinfo(fp, fd, ioc, p);

	case SVR4_TI_OPTMGMT:
		DPRINTF(("TI_OPTMGMT\n"));
		return 0;

	case SVR4_TI_BIND:
		DPRINTF(("TI_BIND\n"));
		return ti_bind(fp, fd, ioc, p);

	case SVR4_TI_UNBIND:
		DPRINTF(("TI_UNBIND\n"));
		return 0;

	default:
		DPRINTF(("Unknown timod ioctl %lx\n", ioc->cmd));
		return 0;
	}
}


int
svr4_stream_ti_ioctl(fp, p, retval, fd, cmd, dat)
	struct file *fp;
	struct proc *p;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	struct svr4_strbuf skb, *sub = (struct svr4_strbuf *) dat;
	struct socket *so = (struct socket *) fp->f_data;
	struct svr4_strm *st = (struct svr4_strm *) so->so_internal;
	int error;
	void *skp, *sup;
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	struct svr4_strmcmd sc;
	int sasize;
	caddr_t sg;
	int *lenp;

	sc.offs = 0x10;
	
	if ((error = copyin(sub, &skb, sizeof(skb))) != 0) {
		DPRINTF(("ti_ioctl: error copying in strbuf\n"));
		return error;
	}

	switch (st->s_family) {
	case AF_INET:
		skp = &sain;
		sasize = sizeof(sain);
		break;

	case AF_UNIX:
		skp = &saun;
		sasize = sizeof(saun);
		break;

	default:
		DPRINTF(("ti_ioctl: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	sg = stackgap_init(p->p_emul);
	sup = stackgap_alloc(&sg, sasize);
	lenp = stackgap_alloc(&sg, sizeof(*lenp));

	if ((error = copyout(&sasize, lenp, sizeof(*lenp))) != 0) {
		DPRINTF(("ti_ioctl: error copying out lenp\n"));
		return error;
	}

	switch (cmd) {
	case SVR4_TI_GETMYNAME:
		DPRINTF(("TI_GETMYNAME\n"));
		{
			struct sys_getsockname_args ap;
			SCARG(&ap, fdes) = fd;
			SCARG(&ap, asa) = sup;
			SCARG(&ap, alen) = lenp;
			if ((error = sys_getsockname(p, &ap, retval)) != 0) {
				DPRINTF(("ti_ioctl: getsockname error\n"));
				return error;
			}
		}
		break;

	case SVR4_TI_GETPEERNAME:
		DPRINTF(("TI_GETPEERNAME\n"));
		{
			struct sys_getpeername_args ap;
			SCARG(&ap, fdes) = fd;
			SCARG(&ap, asa) = sup;
			SCARG(&ap, alen) = lenp;
			if ((error = sys_getpeername(p, &ap, retval)) != 0) {
				DPRINTF(("ti_ioctl: getpeername error\n"));
				return error;
			}
		}
		break;

	case SVR4_TI_SETMYNAME:
		DPRINTF(("TI_SETMYNAME\n"));
		return 0;

	case SVR4_TI_SETPEERNAME:
		DPRINTF(("TI_SETPEERNAME\n"));
		return 0;
	default:
		DPRINTF(("ti_ioctl: Unknown ioctl %lx\n", cmd));
		return ENOSYS;
	}

	if ((error = copyin(sup, skp, sasize)) != 0) {
		DPRINTF(("ti_ioctl: error copying in socket data\n"));
		return error;
	}

	if ((error = copyin(lenp, &sasize, sizeof(*lenp))) != 0) {
		DPRINTF(("ti_ioctl: error copying in socket size\n"));
		return error;
	}

	switch (st->s_family) {
	case AF_INET:
		sockaddr_to_netaddr_in(&sc, &sain);
		break;

	case AF_UNIX:
		sockaddr_to_netaddr_un(&sc, &saun);
		break;

	default:
		return ENOSYS;
	}

	skb.len = sasize;

	if ((error = copyout(SVR4_ADDROF(&sc), skb.buf, sasize)) != 0) {
		DPRINTF(("ti_ioctl: error copying out socket data\n"));
		return error;
	}

	if ((error = copyout(&skb, sub, sizeof(skb))) != 0) {
		DPRINTF(("ti_ioctl: error copying out strbuf\n"));
		return error;
	}

	return error;
}




static int
i_nread(fp, p, retval, fd, cmd, dat)
	struct file *fp;
	struct proc *p;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	int error;
	int nread = 0;	

	/*
	 * We are supposed to return the message length in nread, and the
	 * number of messages in retval. We don't have the notion of number
	 * of stream messages, so we just find out if we have any bytes waiting
	 * for us, and if we do, then we assume that we have at least one
	 * message waiting for us.
	 */
	if ((error = (*fp->f_ops->fo_ioctl)(fp, FIONREAD,
	    (caddr_t) &nread, p)) != 0)
		return error;

	if (nread != 0)
		*retval = 1;
	else
		*retval = 0;

	return copyout(&nread, dat, sizeof(nread));
}


static int
i_str(fp, p, retval, fd, cmd, dat)
	struct file *fp;
	struct proc *p;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	int			 error;
	struct svr4_strioctl	 ioc;

	if ((error = copyin(dat, &ioc, sizeof(ioc))) != 0)
		return error;

#ifdef DEBUG_SVR4
	if ((error = show_ioc(">", &ioc)) != 0)
		return error;
#endif /* DEBUG_SVR4 */

	switch (ioc.cmd & 0xff00) {
	case SVR4_SIMOD:
		if ((error = sockmod(fp, fd, &ioc, p)) != 0)
			return error;
		break;

	case SVR4_TIMOD:
		if ((error = timod(fp, fd, &ioc, p)) != 0)
			return error;
		break;

	default:
		DPRINTF(("Unimplemented module %c %ld\n",
			 (char) (cmd >> 8), cmd & 0xff));
		return 0;
	}

#ifdef DEBUG_SVR4
	if ((error = show_ioc("<", &ioc)) != 0)
		return error;
#endif /* DEBUG_SVR4 */
	return copyout(&ioc, dat, sizeof(ioc));
}


int
svr4_stream_ioctl(fp, p, retval, fd, cmd, dat)
	struct file *fp;
	struct proc *p;
	register_t *retval;
	int fd;
	u_long cmd;
	caddr_t dat;
{
	*retval = 0;

	/*
	 * All the following stuff assumes "sockmod" is pushed...
	 */
	switch (cmd) {
	case SVR4_I_NREAD:
		DPRINTF(("I_NREAD\n"));
		return i_nread(fp, p, retval, fd, cmd, dat);

	case SVR4_I_PUSH:
		DPRINTF(("I_PUSH\n"));
		return 0;

	case SVR4_I_POP:
		DPRINTF(("I_POP\n"));
		return 0;

	case SVR4_I_LOOK:
		DPRINTF(("I_LOOK\n"));
		return 0;

	case SVR4_I_FLUSH:
		DPRINTF(("I_FLUSH\n"));
		return 0;

	case SVR4_I_SRDOPT:
		DPRINTF(("I_SRDOPT\n"));
		return 0;

	case SVR4_I_GRDOPT:
		DPRINTF(("I_GRDOPT\n"));
		return 0;

	case SVR4_I_STR:
		DPRINTF(("I_STR\n"));
		return i_str(fp, p, retval, fd, cmd, dat);

	case SVR4_I_SETSIG:
		DPRINTF(("I_SETSIG\n"));
		return 0;

	case SVR4_I_GETSIG:
		DPRINTF(("I_GETSIG\n"));
		return EINVAL;

	case SVR4_I_FIND:
		DPRINTF(("I_FIND\n"));
		/*
		 * Here we are not pushing modules really, we just
		 * pretend all are present
		 */
		*retval = 1;
		return 0;

	case SVR4_I_LINK:
		DPRINTF(("I_LINK\n"));
		return 0;

	case SVR4_I_UNLINK:
		DPRINTF(("I_UNLINK\n"));
		return 0;

	case SVR4_I_ERECVFD:
		DPRINTF(("I_ERECVFD\n"));
		return 0;

	case SVR4_I_PEEK:
		DPRINTF(("I_PEEK\n"));
		return 0;

	case SVR4_I_FDINSERT:
		DPRINTF(("I_FDINSERT\n"));
		return 0;

	case SVR4_I_SENDFD:
		DPRINTF(("I_SENDFD\n"));
		return 0;

	case SVR4_I_RECVFD:
		DPRINTF(("I_RECVFD\n"));
		return 0;

	case SVR4_I_SWROPT:
		DPRINTF(("I_SWROPT\n"));
		return 0;

	case SVR4_I_GWROPT:
		DPRINTF(("I_GWROPT\n"));
		return 0;

	case SVR4_I_LIST:
		DPRINTF(("I_LIST\n"));
		return 0;

	case SVR4_I_PLINK:
		DPRINTF(("I_PLINK\n"));
		return 0;

	case SVR4_I_PUNLINK:
		DPRINTF(("I_PUNLINK\n"));
		return 0;

	case SVR4_I_SETEV:
		DPRINTF(("I_SETEV\n"));
		return 0;

	case SVR4_I_GETEV:
		DPRINTF(("I_GETEV\n"));
		return 0;

	case SVR4_I_STREV:
		DPRINTF(("I_STREV\n"));
		return 0;

	case SVR4_I_UNSTREV:
		DPRINTF(("I_UNSTREV\n"));
		return 0;

	case SVR4_I_FLUSHBAND:
		DPRINTF(("I_FLUSHBAND\n"));
		return 0;

	case SVR4_I_CKBAND:
		DPRINTF(("I_CKBAND\n"));
		return 0;

	case SVR4_I_GETBAND:
		DPRINTF(("I_GETBANK\n"));
		return 0;

	case SVR4_I_ATMARK:
		DPRINTF(("I_ATMARK\n"));
		return 0;

	case SVR4_I_SETCLTIME:
		DPRINTF(("I_SETCLTIME\n"));
		return 0;

	case SVR4_I_GETCLTIME:
		DPRINTF(("I_GETCLTIME\n"));
		return 0;

	case SVR4_I_CANPUT:
		DPRINTF(("I_CANPUT\n"));
		return 0;

	default:
		DPRINTF(("unimpl cmd = %lx\n", cmd));
		break;
	}

	return 0;
}




int
svr4_sys_putmsg(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_putmsg_args *uap = v;
	struct filedesc	*fdp = p->p_fd;
	struct file	*fp;
	struct svr4_strbuf dat, ctl;
	struct svr4_strmcmd sc;
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	void *skp, *sup;
	int sasize;
	struct socket *so;
	struct svr4_strm *st;
	int error;
	caddr_t sg;

	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return EBADF;

#ifdef DEBUG_SVR4
	show_msg(">putmsg", SCARG(uap, fd), SCARG(uap, ctl),
		 SCARG(uap, dat), SCARG(uap, flags));
#endif /* DEBUG_SVR4 */

	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return EBADF;

	if (SCARG(uap, ctl) != NULL) {
		if ((error = copyin(SCARG(uap, ctl), &ctl, sizeof(ctl))) != 0)
			return error;
	}
	else
		ctl.len = -1;

	if (SCARG(uap, dat) != NULL) {
	    	if ((error = copyin(SCARG(uap, dat), &dat, sizeof(dat))) != 0)
			return error;
	}
	else
		dat.len = -1;

	/*
	 * Only for sockets for now.
	 */
	if (fp == NULL || fp->f_type != DTYPE_SOCKET) {
		DPRINTF(("putmsg: bad file type\n"));
		return EINVAL;
	}

	so = (struct socket *)  fp->f_data;
	st = (struct svr4_strm *) so->so_internal;


	if (ctl.len > sizeof(sc)) {
		DPRINTF(("putmsg: Bad control size %d != %d\n", ctl.len,
			 sizeof(struct svr4_strmcmd)));
		return EINVAL;
	}

	if ((error = copyin(ctl.buf, &sc, ctl.len)) != 0)
		return error;

	switch (st->s_family) {
	case AF_INET:
		if (sc.len != sizeof(sain)) {
			DPRINTF(("putmsg: Invalid inet length %ld\n", sc.len));
			return ENOSYS;
		}
		netaddr_to_sockaddr_in(&sain, &sc);
		skp = &sain;
		sasize = sizeof(sain);
		error = sain.sin_family != st->s_family;
		break;

	case AF_UNIX:
		{
			/* We've been given a device/inode pair */
			dev_t *dev = SVR4_ADDROF(&sc);
			ino_t *ino = (ino_t *) &dev[1];
			if ((skp = svr4_find_socket(p, fp, *dev, *ino)) == NULL)
				return ENOENT;
			sasize = sizeof(saun);
		}
		break;

	default:
		DPRINTF(("putmsg: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	sg = stackgap_init(p->p_emul);
	sup = stackgap_alloc(&sg, sasize);

	if ((error = copyout(skp, sup, sasize)) != 0)
		return error;

	switch (st->s_cmd = sc.cmd) {
	case SVR4_TI_CONNECT_REQUEST:	/* connect 	*/
		{
			struct sys_connect_args co;

			co.s = SCARG(uap, fd);
			co.name = (caddr_t) sup;
			co.namelen = (int) sasize;
			return sys_connect(p, &co, retval);
		}

	case SVR4_TI_SENDTO_REQUEST:	/* sendto 	*/
		{
			struct msghdr msg;
			struct iovec aiov;
			msg.msg_name = (caddr_t) sup;
			msg.msg_namelen = sasize;
			msg.msg_iov = &aiov;
			msg.msg_iovlen = 1;
			msg.msg_control = 0;
			msg.msg_flags = 0;
			aiov.iov_base = dat.buf;
			aiov.iov_len = dat.len;
			error = sendit(p, SCARG(uap, fd), &msg,
				       SCARG(uap, flags), retval);

			*retval = 0;
			return error;
		}
	default:
		DPRINTF(("putmsg: Unimplemented command %lx\n", sc.cmd));
		return ENOSYS;
	}
}


int
svr4_sys_getmsg(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_getmsg_args *uap = v;
	struct filedesc	*fdp = p->p_fd;
	struct file	*fp;
	struct sys_getpeername_args ga;
	struct sys_accept_args aa;
	struct svr4_strbuf dat, ctl;
	struct svr4_strmcmd sc;
	int error;
	struct msghdr msg;
	struct iovec aiov;
	struct sockaddr_in sain;
	struct sockaddr_un saun;
	void *skp, *sup;
	int sasize;
	struct socket *so;
	struct svr4_strm *st;
	int *flen;
	int fl;
	caddr_t sg;

	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return EBADF;

	bzero(&sc, sizeof(sc));

#ifdef DEBUG_SVR4
	show_msg(">getmsg", SCARG(uap, fd), SCARG(uap, ctl),
		 SCARG(uap, dat), 0);
#endif /* DEBUG_SVR4 */
			
	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return EBADF;

	if (SCARG(uap, ctl) != NULL) {
		if ((error = copyin(SCARG(uap, ctl), &ctl, sizeof(ctl))) != 0)
			return error;
	}
	else {
		ctl.len = -1;
		ctl.maxlen = 0;
	}

	if (SCARG(uap, dat) != NULL) {
	    	if ((error = copyin(SCARG(uap, dat), &dat, sizeof(dat))) != 0)
			return error;
	}
	else {
		dat.len = -1;
		dat.maxlen = 0;
	}

	/*
	 * Only for sockets for now.
	 */
	if (fp == NULL || fp->f_type != DTYPE_SOCKET) {
		DPRINTF(("getmsg: bad file type\n"));
		return EINVAL;
	}

	so = (struct socket *)  fp->f_data;
	st = (struct svr4_strm *) so->so_internal;

	if (ctl.maxlen == -1 || dat.maxlen == -1) {
		DPRINTF(("getmsg: Cannot handle -1 maxlen (yet)\n"));
		return ENOSYS;
	}

	switch (st->s_family) {
	case AF_INET:
		skp = &sain;
		sasize = sizeof(sain);
		break;

	case AF_UNIX:
		skp = &saun;
		sasize = sizeof(saun);
		break;

	default:
		DPRINTF(("getmsg: Unsupported address family %d\n",
			 st->s_family));
		return ENOSYS;
	}

	sg = stackgap_init(p->p_emul);
	sup = stackgap_alloc(&sg, sasize);
	flen = (int *) stackgap_alloc(&sg, sizeof(*flen));

	fl = sasize;
	if ((error = copyout(&fl, flen, sizeof(fl))) != 0)
		return error;

	switch (st->s_cmd) {
	case SVR4_TI_CONNECT_REQUEST:
		/*
		 * We do the connect in one step, so the putmsg should
		 * have gotten the error.
		 */
		sc.cmd = SVR4_TI_OK_REPLY;
		sc.len = 0;

		ctl.len = 8;
		dat.len = -1;
		fl = 1;
		break;

	case SVR4_TI_OK_REPLY:
		/*
		 * We are immediately after a connect reply, so we send
		 * an connect verification.
		 */
		SCARG(&ga, fdes) = SCARG(uap, fd);
		SCARG(&ga, asa) = (caddr_t) sup;
		SCARG(&ga, alen) = flen;
		
		if ((error = sys_getpeername(p, &ga, retval)) != 0) {
			DPRINTF(("getmsg: getpeername failed %d\n", error));
			return error;
		}

		if ((error = copyin(sup, skp, sasize)) != 0)
			return error;
		
		sc.cmd = SVR4_TI_CONNECT_REPLY;
		sc.len = sasize;
		sc.offs = 0x18;
		sc.pad[0] = 0x4;
		sc.pad[1] = 0x14;
		sc.pad[2] = 0x04000402;

		switch (st->s_family) {
		case AF_INET:
			sockaddr_to_netaddr_in(&sc, &sain);
			break;

		case AF_UNIX:
			sockaddr_to_netaddr_un(&sc, &saun);
			break;

		default:
			return ENOSYS;
		}

		ctl.len = 40;
		dat.len = -1;
		fl = 0;
		break;

	case SVR4_TI_ACCEPT_REPLY:
		/*
		 * We are after a listen, so we try to accept...
		 */
		SCARG(&aa, s) = SCARG(uap, fd);
		SCARG(&aa, name) = (caddr_t) sup;
		SCARG(&aa, anamelen) = flen;
		
		if ((error = sys_accept(p, &aa, retval)) != 0) {
			DPRINTF(("getmsg: getpeername failed %d\n", error));
			return error;
		}

		if ((error = copyin(sup, skp, sasize)) != 0)
			return error;
		
		sc.cmd = SVR4_TI_ACCEPT_REPLY;
		sc.len = sasize;
		sc.offs = 0x18;
		sc.pad[0] = 0x0;
		sc.pad[1] = 0x28;
		sc.pad[2] = 0x3;

		switch (st->s_family) {
		case AF_INET:
			sockaddr_to_netaddr_in(&sc, &sain);
			break;

		case AF_UNIX:
			sockaddr_to_netaddr_un(&sc, &saun);
			break;

		default:
			return ENOSYS;
		}

		ctl.len = 40;
		dat.len = -1;
		fl = 0;
		break;

	case SVR4_TI_SENDTO_REQUEST:
		if (ctl.maxlen > 36 && ctl.len < 36)
		    ctl.len = 36;

		if ((error = copyin(ctl.buf, &sc, ctl.len)) != 0)
			return error;

		switch (st->s_family) {
		case AF_INET:
			sockaddr_to_netaddr_in(&sc, &sain);
			break;

		case AF_UNIX:
			sockaddr_to_netaddr_un(&sc, &saun);
			break;

		default:
			return ENOSYS;
		}

		msg.msg_name = (caddr_t) sup;
		msg.msg_namelen = sasize;
		msg.msg_iov = &aiov;
		msg.msg_iovlen = 1;
		msg.msg_control = 0;
		aiov.iov_base = dat.buf;
		aiov.iov_len = dat.maxlen;
		msg.msg_flags = 0;

		error = recvit(p, SCARG(uap, fd), &msg, (caddr_t) flen, retval);

		if (error) {
			DPRINTF(("getmsg: recvit failed %d\n", error));
			return error;
		}

		if ((error = copyin(msg.msg_name, skp, sasize)) != 0)
			return error;

		sc.cmd = SVR4_TI_RECVFROM_REPLY;
		sc.len = sasize;

		switch (st->s_family) {
		case AF_INET:
			sockaddr_to_netaddr_in(&sc, &sain);
			break;

		case AF_UNIX:
			sockaddr_to_netaddr_un(&sc, &saun);
			break;

		default:
			return ENOSYS;
		}

		dat.len = *retval;
		fl = 0;
		break;

	default:
		DPRINTF(("getmsg: Unknown state %x\n", st->s_cmd));
		return EINVAL;
	}

	st->s_cmd = sc.cmd;
	if (SCARG(uap, ctl)) {
		if (ctl.len != -1)
			if ((error = copyout(&sc, ctl.buf, ctl.len)) != 0)
				return error;

		if ((error = copyout(&ctl, SCARG(uap, ctl), sizeof(ctl))) != 0)
			return error;
	}

	if (SCARG(uap, dat)) {
		if ((error = copyout(&dat, SCARG(uap, dat), sizeof(dat))) != 0)
			return error;
	}

	if (SCARG(uap, flags)) { /* XXX: Need translation */
		if ((error = copyout(&fl, SCARG(uap, flags), sizeof(fl))) != 0)
			return error;
	}

	*retval = 0;

#ifdef DEBUG_SVR4
	show_msg("<getmsg", SCARG(uap, fd), SCARG(uap, ctl),
		 SCARG(uap, dat), fl);
#endif /* DEBUG_SVR4 */
	return error;
}
