/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

#include <vldb.h>
#include <volumeserver.h>

#include <aafs/aafs_cell.h>
#include <aafs/aafs_volume.h>
#include <aafs/aafs_vldb.h>

#include <roken.h>

int
main(int argc, char **argv)
{
    struct aafs_cell *c;
    struct aafs_volume *v;
    struct aafs_volume_info *i;
    struct nvldbentry nvldb;
    int ret;

    setprogname(argv[0]);

    if ((ret = aafs_init(NULL)) != 0)
	errx(1, "aafs_init");
    
    if ((ret = aafs_cell_create("e.kth.se", AAFS_SEC_NULL, &c)) != 0)
	errx(1, "aafs_cell_create");

    if ((ret = aafs_volume_create(c, "root.cell", 0, &v)) != 0)
	errx(1, "aafs_volume_create");
	
    if ((ret = aafs_volume_examine_nvldb(v, 0, &nvldb)) != 0)
	errx(1, "aafs_volume_examine_nvldb");

    aafs_volume_print_nvldb(v, stdout, 0);

    printf("\n");

    if ((ret = aafs_volume_examine_info(v, VOL_EXA_VOLINFO_ALL, &i)) != 0)
	errx(1, "aafs_volume_examine_info");
	
    {
	    struct aafs_volume_info_ctx *ctx;
	    struct aafs_volume_info_entry *se;
	    struct aafs_site *site;

	    for (se = aafs_volume_info_first(i, &ctx);
		 se != NULL;
		 se = aafs_volume_info_next(ctx))
	    {
		    char sn[1024];

		    site = aafs_volume_info_get_site(se);
		    printf("%s\n", aafs_site_print(site, sn, sizeof(sn)));
		    aafs_object_unref(site, "site");

		    aafs_volume_status_print(stdout, 0, se);
		    printf("\n");
	    }
	    aafs_volume_info_destroy_ctx(ctx);
    }

    /* list vldb */
    {
	struct aafs_vldb_query_attrs *attrs;
	struct aafs_vldb_list *ql;
	struct aafs_vldb_ctx *vq;	
	char vn[VLDB_MAXNAMELEN];
	
	if ((ret = aafs_vldb_query_attr_create(&attrs)) != 0)
	    errx(1, "aafs_vldb_query_attr_create");

	if ((ret = aafs_vldb_query(c, attrs, &ql)) != 0)
	    errx(1, "aafs_vldb_query");

	for (v = aafs_vldb_iterate_first(ql, &vq);
	     v != NULL;
	     v = aafs_vldb_iterate_next(vq))
	{
	    aafs_volume_print_nvldb(v, stdout, 0);
	}

	aafs_vldb_query_free(ql);

    }

    return 0;
}
