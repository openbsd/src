/*	$OpenBSD: tcfs_mount.h,v 1.4 2000/06/18 16:23:10 provos Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _TCFS_MOUNT_H_
#define _TCFS_MOUNT_H_

#include <miscfs/tcfs/tcfs_keytab.h>
#include <miscfs/tcfs/tcfs_version.h>

#define MaxCipherNameLen	8
struct tcfs_status {
	int	status;
	int 	n_ukey;
	int	n_gkey;
	int	tcfs_version;
	char	cipher_desc[MaxCipherNameLen];
	int	cipher_keysize;
	int	cipher_version;
};

struct tcfs_args {
	char		*target;	/* Target of loopback   */
	u_char 		tcfs_key[KEYSIZE];
	int		cipher_num;
	int		cmd;		/* direttiva		*/
	uid_t		user;		/* utente		*/
	pid_t		proc;		/* processo		*/
	gid_t		group;		/* gruppo		*/
	int		treshold;	/* soglia grpkey	*/
	struct tcfs_status	st;	
};

struct tcfs_mount {
	struct mount	*tcfsm_vfs;
	struct vnode	*tcfsm_rootvp;	/* Reference to root tcfs_node */
	void *ks;
	tcfs_keytab *tcfs_uid_kt;
	tcfs_keytab *tcfs_gid_kt;
	int tcfs_cipher_num;
};

#endif /* _TCFS_MOUNT_H_ */
