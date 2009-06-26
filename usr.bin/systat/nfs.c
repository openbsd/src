/*	$OpenBSD: nfs.c,v 1.4 2009/06/26 17:24:31 canacar Exp $	*/

/*
 * Copyright (c) 2009 Jasper Lievisse Adriaanse <jasper@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "systat.h"

int	check_nfs(void);
int	select_client(void);
int	select_server(void);
int	read_nfs(void);
void	print_client(void);
void	print_server(void);

struct	nfsstats nfsstats;
int	num_client = 0;
int	num_server = 0;

field_def fields_nfs[] = {
	/* Client */
	{"RPC COUNTS", 10, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 12, 14, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"RPC INFO", 14, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 12, 14, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"CACHE INFO", 10, 12, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"", 12, 14, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},

	/* Server */
	{"RPC COUNTS", 10, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 12, 14, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"CACHE STATS", 14, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 12, 14, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"WRITES", 10, 12, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"", 12, 14, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
};

/* _V suffixed fields indicate a value column. */
#define FIELD_ADDR(x) (&fields_nfs[x])

/* Client */
#define	FLD_NFS_C_RPC_COUNTS	FIELD_ADDR(0)
#define	FLD_NFS_C_RPC_COUNTS_V	FIELD_ADDR(1)
#define	FLD_NFS_C_RPC_INFO	FIELD_ADDR(2)
#define	FLD_NFS_C_RPC_INFO_V	FIELD_ADDR(3)
#define	FLD_NFS_C_CACHE_INFO	FIELD_ADDR(4)
#define	FLD_NFS_C_CACHE_V	FIELD_ADDR(5)

/* Server */
#define	FLD_NFS_S_RPC_COUNTS	FIELD_ADDR(6)
#define	FLD_NFS_S_RPC_COUNTS_V	FIELD_ADDR(7)
#define	FLD_NFS_S_CACHE_STATS	FIELD_ADDR(8)
#define	FLD_NFS_S_CACHE_STATS_V	FIELD_ADDR(9)
#define	FLD_NFS_S_WRITES	FIELD_ADDR(10)
#define	FLD_NFS_S_WRITES_V	FIELD_ADDR(11)

/* Define views */
field_def *view_nfs_0[] = {
	FLD_NFS_C_RPC_COUNTS, FLD_NFS_C_RPC_COUNTS_V, FLD_NFS_C_RPC_INFO,
	FLD_NFS_C_RPC_INFO_V, FLD_NFS_C_CACHE_INFO, FLD_NFS_C_CACHE_V ,NULL
};

field_def *view_nfs_1[] = {
	FLD_NFS_S_RPC_COUNTS, FLD_NFS_S_RPC_COUNTS_V, FLD_NFS_S_CACHE_STATS,
	FLD_NFS_S_CACHE_STATS_V, FLD_NFS_S_WRITES, FLD_NFS_S_WRITES_V, NULL
};

/* Define view managers */
struct view_manager nfs_client_mgr = {
	"Client", select_client, read_nfs, NULL, print_header,
	print_client, keyboard_callback, NULL, NULL
};

struct view_manager nfs_server_mgr = {
	"Server", select_server, read_nfs, NULL, print_header,
	print_server, keyboard_callback, NULL, NULL
};

field_view views_nfs[] = {
	{view_nfs_0, "nfsclient", '8', &nfs_client_mgr},
	{view_nfs_1, "nfsserver", '9', &nfs_server_mgr},
	{NULL, NULL, 0, NULL}
};

int
select_client(void)
{
	num_disp = num_client;
	return(0);
}

int
select_server(void)
{
	num_disp = num_server;
	return(0);
}

int
initnfs(void)
{
	field_view *v;

	for (v = views_nfs; v->name != NULL; v++)
		add_view(v);

	read_nfs();

	return(0);
}

/*
 * We get all the information in one go and don't care about
 * server or client fields (those will be '0' if not applicable).
 */
int
read_nfs(void)
{
	struct nfsstats *p = &nfsstats;
	int mib[3];
	size_t len = sizeof(*p);

	mib[0] = CTL_VFS;
	mib[1] = 2; /* NETDEV */
	mib[2] = NFS_NFSSTATS;

	if (sysctl(mib, 3, p, &len, NULL, 0) < 0)
		return(-1);
	else
		return(0);
}


/*
 * As we want a view with multiple columns, mixed with labels and values,
 * we can't use the regular dance and have to use our own (looong) dance
 * to build the layout.
 */
void
print_client(void)
{
	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Getattr");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_GETATTR]);
	print_fld_str(FLD_NFS_C_RPC_INFO, "TimedOut");
	print_fld_ssize(FLD_NFS_C_RPC_INFO_V, nfsstats.rpctimeouts);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "Attr Hits  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.attrcache_hits);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Setattr");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_SETATTR]);
	print_fld_str(FLD_NFS_C_RPC_INFO, "Invalid");
	print_fld_ssize(FLD_NFS_C_RPC_INFO_V, nfsstats.rpcinvalid);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "Attr Misses");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.attrcache_misses);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Lookup");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_LOOKUP]);
	print_fld_str(FLD_NFS_C_RPC_INFO, "X Replies");
	print_fld_ssize(FLD_NFS_C_RPC_INFO_V, nfsstats.rpcunexpected);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "Lkup Hits  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.lookupcache_hits);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Readlink");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_READLINK]);
	print_fld_str(FLD_NFS_C_RPC_INFO, "Retries");
	print_fld_ssize(FLD_NFS_C_RPC_INFO_V, nfsstats.rpcretries);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "Lkup Misses  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.lookupcache_misses);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Read");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_READ]);
	print_fld_str(FLD_NFS_C_RPC_INFO, "Requests");
	print_fld_ssize(FLD_NFS_C_RPC_INFO_V, nfsstats.rpcrequests);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioR Hits  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V,
	  nfsstats.biocache_reads-nfsstats.read_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Write");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V, nfsstats.rpccnt[NFSPROC_WRITE]);
	print_fld_str(FLD_NFS_C_RPC_INFO, "FrcSync");
	print_fld_ssize(FLD_NFS_C_RPC_INFO_V, nfsstats.forcedsync);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioR Misses");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.read_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Create");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_CREATE]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioW Hits  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V,
	  nfsstats.biocache_writes-nfsstats.write_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Remove");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_REMOVE]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioW Misses");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.write_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Rename");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_RENAME]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioRL Hits  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V,
	  nfsstats.biocache_readlinks-nfsstats.readlink_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Link");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V, nfsstats.rpccnt[NFSPROC_LINK]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioRL Misses");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.readlink_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Symlink");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_SYMLINK]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioD Hits  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V,
	  nfsstats.biocache_readdirs-nfsstats.readdir_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Mkdir");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V, nfsstats.rpccnt[NFSPROC_MKDIR]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "BioD Misses");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.readdir_bios);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Rmdir");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V, nfsstats.rpccnt[NFSPROC_RMDIR]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "DirE Hits  ");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.direofcache_hits);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Readdir");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_READDIR]);
	print_fld_str(FLD_NFS_C_CACHE_INFO, "DirE Misses");
	print_fld_ssize(FLD_NFS_C_CACHE_V, nfsstats.direofcache_misses);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "RdirPlus");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_READDIRPLUS]);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Access");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_ACCESS]);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Mknod");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V, nfsstats.rpccnt[NFSPROC_MKNOD]);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Fsstat");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_FSSTAT]);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Fsinfo");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_FSINFO]);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "PathConf");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_PATHCONF]);
	end_line();

	print_fld_str(FLD_NFS_C_RPC_COUNTS, "Commit");
	print_fld_ssize(FLD_NFS_C_RPC_COUNTS_V,
	  nfsstats.rpccnt[NFSPROC_COMMIT]);
	end_line();
}

void
print_server(void)
{
	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Getattr");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_GETATTR]);
	print_fld_str(FLD_NFS_S_CACHE_STATS, "Inprog");
	print_fld_ssize(FLD_NFS_S_CACHE_STATS_V, nfsstats.srvcache_inproghits);
	print_fld_str(FLD_NFS_S_WRITES, "WriteOps");
	print_fld_ssize(FLD_NFS_S_WRITES_V, nfsstats.srvvop_writes);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Setattr");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_SETATTR]);
	print_fld_str(FLD_NFS_S_CACHE_STATS, "Idem");
	print_fld_ssize(FLD_NFS_S_CACHE_STATS_V,
	  nfsstats.srvcache_idemdonehits);
	print_fld_str(FLD_NFS_S_WRITES, "WriteRPC");
	print_fld_ssize(FLD_NFS_S_WRITES_V, nfsstats.srvrpccnt[NFSPROC_WRITE]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Lookup");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_LOOKUP]);
	print_fld_str(FLD_NFS_S_CACHE_STATS, "Non-idem");
	print_fld_ssize(FLD_NFS_S_CACHE_STATS_V,
	  nfsstats.srvcache_nonidemdonehits);
	print_fld_str(FLD_NFS_S_WRITES, "Opsaved");
	print_fld_ssize(FLD_NFS_S_WRITES_V,
	  nfsstats.srvrpccnt[NFSPROC_WRITE] - nfsstats.srvvop_writes);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Readlink");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_READLINK]);
	print_fld_str(FLD_NFS_S_CACHE_STATS, "Misses");
	print_fld_ssize(FLD_NFS_S_CACHE_STATS_V, nfsstats.srvcache_misses);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Read");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_READ]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Write");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_WRITE]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Create");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_CREATE]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Remove");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_REMOVE]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Rename");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_RENAME]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Link");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_LINK]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Symlink");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_SYMLINK]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Mkdir");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_MKDIR]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Rmdir");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_RMDIR]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Readdir");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_READDIR]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "RdirPlus");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_READDIRPLUS]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Access");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_ACCESS]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Mknod");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_MKNOD]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Fsstat");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_FSSTAT]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Fsinfo");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_FSINFO]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "PathConf");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_PATHCONF]);
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Commit");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V,
	  nfsstats.srvrpccnt[NFSPROC_COMMIT]);
	end_line();

	/* This creates an empty space on screen to separate the two blocks */
	print_fld_str(FLD_NFS_S_RPC_COUNTS, "");
	end_line();

	print_fld_str(FLD_NFS_S_RPC_COUNTS, "Ret-Failed");
	print_fld_ssize(FLD_NFS_S_RPC_COUNTS_V, nfsstats.srvrpc_errs);
	print_fld_str(FLD_NFS_S_CACHE_STATS, "Faults");
	print_fld_ssize(FLD_NFS_S_CACHE_STATS_V, nfsstats.srv_errs);
	end_line();
}
