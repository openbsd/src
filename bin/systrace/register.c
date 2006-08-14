/*	$OpenBSD: register.c,v 1.22 2006/08/14 07:24:55 ray Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
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
 *      This product includes software developed by Niels Provos.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/tree.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>

#include "intercept.h"
#include "systrace.h"
#include "linux-translate.h"

#define X(x)	if ((x) == -1) \
	err(1, "%s:%d: intercept failed", __func__, __LINE__)

void
systrace_initcb(void)
{
	struct systrace_alias *alias;
	struct intercept_translate *tl;

	X(intercept_init());

	X(intercept_register_gencb(gen_cb, NULL));
	X(intercept_register_sccb("native", "open", trans_cb, NULL));
	tl = intercept_register_transfn("native", "open", 0);
	intercept_register_translation("native", "open", 1, &ic_oflags);
	alias = systrace_new_alias("native", "open", "native", "fswrite");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("native", "sendmsg", trans_cb, NULL));
	intercept_register_translation("native", "sendmsg", 1,
	    &ic_translate_sendmsg);
	X(intercept_register_sccb("native", "connect", trans_cb, NULL));
	intercept_register_translation("native", "connect", 1,
	    &ic_translate_connect);
	X(intercept_register_sccb("native", "sendto", trans_cb, NULL));
	intercept_register_translation("native", "sendto", 4,
	    &ic_translate_connect);
	X(intercept_register_sccb("native", "bind", trans_cb, NULL));
	intercept_register_translation("native", "bind", 1,
	    &ic_translate_connect);
	X(intercept_register_sccb("native", "execve", trans_cb, NULL));
	intercept_register_transfn("native", "execve", 0);
	intercept_register_translation("native", "execve", 1, &ic_trargv);
	X(intercept_register_sccb("native", "stat", trans_cb, NULL));
	tl = intercept_register_transfn("native", "stat", 0);
	alias = systrace_new_alias("native", "stat", "native", "fsread");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("native", "lstat", trans_cb, NULL));
	tl = intercept_register_translation("native", "lstat", 0,
	    &ic_translate_unlinkname);
	alias = systrace_new_alias("native", "lstat", "native", "fsread");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("native", "unlink", trans_cb, NULL));
	tl = intercept_register_translation("native", "unlink", 0,
	    &ic_translate_unlinkname);
	alias = systrace_new_alias("native", "unlink", "native", "fswrite");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("native", "truncate", trans_cb, NULL));
	tl = intercept_register_transfn("native", "truncate", 0);
	alias = systrace_new_alias("native", "truncate", "native", "fswrite");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("native", "mkfifo", trans_cb, NULL));
	tl = intercept_register_transfn("native", "mkfifo", 0);
	intercept_register_translation("native", "mkfifo", 1, &ic_modeflags);
	alias = systrace_new_alias("native", "mkfifo", "native", "fswrite");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("native", "mknod", trans_cb, NULL));
	intercept_register_transfn("native", "mknod", 0);
	intercept_register_translation("native", "mknod", 1, &ic_modeflags);

	X(intercept_register_sccb("native", "chown", trans_cb, NULL));
	intercept_register_transfn("native", "chown", 0);
	intercept_register_translation("native", "chown", 1, &ic_uidt);
	intercept_register_translation("native", "chown", 2, &ic_gidt);
	X(intercept_register_sccb("native", "fchown", trans_cb, NULL));
	intercept_register_translation("native", "fchown", 0, &ic_fdt);
	intercept_register_translation("native", "fchown", 1, &ic_uidt);
	intercept_register_translation("native", "fchown", 2, &ic_gidt);
	X(intercept_register_sccb("native", "lchown", trans_cb, NULL));
	intercept_register_translation("native", "lchown", 0,
	    &ic_translate_unlinkname);
	intercept_register_translation("native", "lchown", 1, &ic_uidt);
	intercept_register_translation("native", "lchown", 2, &ic_gidt);
	X(intercept_register_sccb("native", "chmod", trans_cb, NULL));
	intercept_register_transfn("native", "chmod", 0);
	intercept_register_translation("native", "chmod", 1, &ic_modeflags);
	X(intercept_register_sccb("native", "fchmod", trans_cb, NULL));
	intercept_register_translation("native", "fchmod", 0, &ic_fdt);
	intercept_register_translation("native", "fchmod", 1, &ic_modeflags);
	X(intercept_register_sccb("native", "chflags", trans_cb, NULL));
	intercept_register_transfn("native", "chflags", 0);
	intercept_register_translation("native", "chflags", 1, &ic_fileflags);
	X(intercept_register_sccb("native", "readlink", trans_cb, NULL));
	tl = intercept_register_translation("native", "readlink", 0,
	    &ic_translate_unlinkname);
	alias = systrace_new_alias("native", "readlink", "native", "fsread");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("native", "chdir", trans_cb, NULL));
	intercept_register_transfn("native", "chdir", 0);
	X(intercept_register_sccb("native", "chroot", trans_cb, NULL));
	intercept_register_transfn("native", "chroot", 0);
	X(intercept_register_sccb("native", "access", trans_cb, NULL));
	tl = intercept_register_transfn("native", "access", 0);
	alias = systrace_new_alias("native", "access", "native", "fsread");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("native", "mkdir", trans_cb, NULL));
	tl = intercept_register_translation("native", "mkdir", 0,
	    &ic_translate_unlinkname);
	alias = systrace_new_alias("native", "mkdir", "native", "fswrite");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("native", "rmdir", trans_cb, NULL));
	tl = intercept_register_transfn("native", "rmdir", 0);
	alias = systrace_new_alias("native", "rmdir", "native", "fswrite");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("native", "rename", trans_cb, NULL));
	intercept_register_translation("native", "rename", 0,
	    &ic_translate_unlinkname);
	intercept_register_translation("native", "rename", 1,
	    &ic_translate_unlinkname);
	X(intercept_register_sccb("native", "symlink", trans_cb, NULL));
	intercept_register_transstring("native", "symlink", 0);
	intercept_register_translation("native", "symlink", 1,
	    &ic_translate_unlinkname);
	X(intercept_register_sccb("native", "link", trans_cb, NULL));
	intercept_register_transfn("native", "link", 0);
	intercept_register_transfn("native", "link", 1);

	X(intercept_register_sccb("native", "setuid", trans_cb, NULL));
	intercept_register_translation("native", "setuid", 0, &ic_uidt);
	intercept_register_translation("native", "setuid", 0, &ic_uname);
	X(intercept_register_sccb("native", "seteuid", trans_cb, NULL));
	intercept_register_translation("native", "seteuid", 0, &ic_uidt);
	intercept_register_translation("native", "seteuid", 0, &ic_uname);
	X(intercept_register_sccb("native", "setgid", trans_cb, NULL));
	intercept_register_translation("native", "setgid", 0, &ic_gidt);
	X(intercept_register_sccb("native", "setegid", trans_cb, NULL));
	intercept_register_translation("native", "setegid", 0, &ic_gidt);

	X(intercept_register_sccb("native", "socket", trans_cb, NULL));
	intercept_register_translation("native", "socket", 0, &ic_sockdom);
	intercept_register_translation("native", "socket", 1, &ic_socktype);
	X(intercept_register_sccb("native", "kill", trans_cb, NULL));
	intercept_register_translation("native", "kill", 0, &ic_pidname);
	intercept_register_translation("native", "kill", 1, &ic_signame);
	X(intercept_register_sccb("native", "fcntl", trans_cb, NULL));
	intercept_register_translation("native", "fcntl", 1, &ic_fcntlcmd);

	X(intercept_register_sccb("native", "mmap", trans_cb, NULL));
	intercept_register_translation("native", "mmap", 2, &ic_memprot);
	X(intercept_register_sccb("native", "mprotect", trans_cb, NULL));
	intercept_register_translation("native", "mprotect", 2, &ic_memprot);

	X(intercept_register_sccb("linux", "open", trans_cb, NULL));
	tl = intercept_register_translink("linux", "open", 0);
	intercept_register_translation("linux", "open", 1, &ic_linux_oflags);
	alias = systrace_new_alias("linux", "open", "linux", "fswrite");
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("linux", "stat", trans_cb, NULL));
	tl = intercept_register_translink("linux", "stat", 0);
	alias = systrace_new_alias("linux", "stat", "linux", "fsread");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("linux", "lstat", trans_cb, NULL));
	tl = intercept_register_translink("linux", "lstat", 0);
	alias = systrace_new_alias("linux", "lstat", "linux", "fsread");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("linux", "execve", trans_cb, NULL));
	intercept_register_translink("linux", "execve", 0);
	X(intercept_register_sccb("linux", "access", trans_cb, NULL));
	tl = intercept_register_translink("linux", "access", 0);
	alias = systrace_new_alias("linux", "access", "linux", "fsread");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("linux", "symlink", trans_cb, NULL));
	intercept_register_transstring("linux", "symlink", 0);
	intercept_register_translink("linux", "symlink", 1);
	X(intercept_register_sccb("linux", "link", trans_cb, NULL));
	intercept_register_translink("linux", "link", 0);
	intercept_register_translink("linux", "link", 1);
	X(intercept_register_sccb("linux", "readlink", trans_cb, NULL));
	tl = intercept_register_translink("linux", "readlink", 0);
	alias = systrace_new_alias("linux", "readlink", "linux", "fsread");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("linux", "rename", trans_cb, NULL));
	intercept_register_translink("linux", "rename", 0);
	intercept_register_translink("linux", "rename", 1);
	X(intercept_register_sccb("linux", "mkdir", trans_cb, NULL));
	tl = intercept_register_translink("linux", "mkdir", 0);
	alias = systrace_new_alias("linux", "mkdir", "linux", "fswrite");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("linux", "rmdir", trans_cb, NULL));
	tl = intercept_register_translink("linux", "rmdir", 0);
	alias = systrace_new_alias("linux", "rmdir", "linux", "fswrite");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("linux", "unlink", trans_cb, NULL));
	tl = intercept_register_translink("linux", "unlink", 0);
	alias = systrace_new_alias("linux", "unlink", "linux", "fswrite");
	systrace_alias_add_trans(alias, tl);
	X(intercept_register_sccb("linux", "chmod", trans_cb, NULL));
	intercept_register_translink("linux", "chmod", 0);
	intercept_register_translation("linux", "chmod", 1, &ic_modeflags);

	X(intercept_register_sccb("linux", "socketcall", trans_cb, NULL));
	alias = systrace_new_alias("linux", "socketcall", "linux", "_socketcall");
	tl = intercept_register_translation("linux", "socketcall", 1, &ic_linux_socket_sockdom);
	systrace_alias_add_trans(alias, tl);
	tl = intercept_register_translation("linux", "socketcall", 1, &ic_linux_socket_socktype);
	systrace_alias_add_trans(alias, tl);
	tl = intercept_register_translation("linux", "socketcall", 1, &ic_linux_connect_sockaddr);
	systrace_alias_add_trans(alias, tl);
	tl = intercept_register_translation("linux", "socketcall", 1, &ic_linux_bind_sockaddr);
	systrace_alias_add_trans(alias, tl);
	tl = intercept_register_translation("linux", "socketcall", 0, &ic_linux_socketcall_catchall);
	systrace_alias_add_trans(alias, tl);

	X(intercept_register_sccb("linux", "kill", trans_cb, NULL));
	intercept_register_translation("linux", "kill", 0, &ic_pidname);
	intercept_register_translation("linux", "kill", 1, &ic_signame);

	X(intercept_register_execcb(execres_cb, NULL));
	X(intercept_register_pfreecb(policyfree_cb, NULL));
}
