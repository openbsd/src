#ifndef _TCFS_KEYTAB_H_
#include "tcfs_keytab.h"
#endif
#define _TCFS_MOUNT_H_

#ifndef _TCFS_VERSION_H_
#include "tcfs_version.h"
#endif

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
	char 		*tcfs_key;	/* chiave 		*/
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
	void* ks;
	tcfs_keytab *tcfs_uid_kt;
	tcfs_keytab *tcfs_gid_kt;
	int tcfs_cipher_num;
};
