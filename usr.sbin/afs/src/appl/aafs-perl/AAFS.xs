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

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <vldb.h>
#include <volumeserver.h>

#include <aafs/aafs_cell.h>
#include <aafs/aafs_volume.h>
#include <aafs/aafs_vldb.h>

#include <ko.h>

typedef struct aafs_cell aafs_cell;
typedef struct aafs_volume aafs_volume;

static void
hv_store_val(HV *h, char *k, int32_t val)
{
    hv_store(h, k, strlen(k), newSViv(val), 0);
}

static void
hv_store_str(HV *h, char *k, const char *str)
{
    hv_store(h, k, strlen(k), newSVpvn(str, strlen(str)), 0);
}


MODULE = AAFS  PACKAGE = AAFS

aafs_cell *
aafs_cell_create(cell, sec_type = 0)
	char *cell
	int sec_type
   PREINIT:
	aafs_cell *c;
   CODE:
	aafs_init(NULL); /* XXX */
	if (aafs_cell_create(cell, sec_type, &c) != 0)
		XSRETURN_UNDEF;
	RETVAL = c;
    OUTPUT:
	RETVAL


aafs_volume *
aafs_volume_create(cell, name, flags = 0)
	aafs_cell *cell
	char *name
	int flags
    PREINIT:
	struct aafs_volume *v;
	int error;
    CODE:
	error = aafs_volume_create(cell, name, flags, &v);
	if (error)
	    XSRETURN_UNDEF;
	RETVAL = v;
    OUTPUT:
	RETVAL

HV *
aafs_volume_examine_nvldb(volume)
	aafs_volume *volume
   PREINIT:
	HV *h, *sh;
	AV *s;
	int error, i;
	struct nvldbentry vldb;
   CODE:
	error = aafs_volume_examine_nvldb(volume, 0, &vldb);
	if (error)
	    XSRETURN_UNDEF;

	h = (HV *)sv_2mortal((SV *)newHV());

	hv_store_str(h, "name", vldb.name);
	hv_store_val(h, "nServers", vldb.nServers);
	s = (AV *)sv_2mortal((SV *)newAV());
	for (i = 0; i < vldb.nServers; i++) {
	    sh = (HV *)sv_2mortal((SV *)newHV());
	    hv_store_val(sh, "number", vldb.serverNumber[i]);
	    hv_store_val(sh, "partition", vldb.serverPartition[i]);
	    hv_store_val(sh, "serverFlags", vldb.serverFlags[i]);
	    av_push(s, newRV((SV *)sh));
	}
	hv_store(h, "Servers", 7, newRV((SV *)s), 0);
	if (vldb.volumeId[RWVOL])
	    hv_store_val(h, "RW-ID", vldb.volumeId[RWVOL]);
	if (vldb.volumeId[ROVOL])
	    hv_store_val(h, "RO-ID", vldb.volumeId[ROVOL]);
	if (vldb.volumeId[BACKVOL])
	    hv_store_val(h, "BU-ID", vldb.volumeId[BACKVOL]);
	if (vldb.cloneId)
	    hv_store_val(h, "Clone-ID", vldb.cloneId);
    	hv_store_val(h, "flags", vldb.flags & VLOP_ALLOPERS);
    	hv_store_val(h, "matchindex", vldb.matchindex);

	RETVAL = h;
    OUTPUT:
	RETVAL


AV *
aafs_volume_examine_info(volume, flags = VOL_EXA_VOLINFO_ALL)
	aafs_volume *volume
	int flags	
   PREINIT:
	struct aafs_volume_info *info;
	struct aafs_volume_info_ctx *ctx;
	struct aafs_volume_info_entry *se;
	struct aafs_server *server;
	aafs_partition partition;
	struct aafs_site *site;
	xvolintInfo v;
	int error, i;
	HV *sh;
	AV *s, *es;
   CODE:
	error = aafs_volume_examine_info(volume, flags, &info);
	if (error)
	    XSRETURN_UNDEF; 

	s = (AV *)sv_2mortal((SV *)newAV());

	for (se = aafs_volume_info_first(info, &ctx);
		se != NULL;
		se = aafs_volume_info_next(ctx))
	{
		char sn[1024], pn[30];

		sh = (HV *)sv_2mortal((SV *)newHV());

		av_push(s, newRV((SV *)sh));

		site = aafs_volume_info_get_site(se);

		server = aafs_site_server(site);
		partition = aafs_site_partition(site);

		aafs_server_get_name(server, sn, sizeof(sn));
		aafs_partition_name(partition, pn, sizeof(pn));

		hv_store_str(sh, "Server", sn);
		hv_store_str(sh, "Partition", pn);

		aafs_object_unref(server, "server");
		aafs_object_unref(site, "site");

		aafs_volume_info_get_volinfo(se, &v);

		hv_store_str(sh, "Status", 
		    v.status == VOK ? "On-line" : "Busy");
		hv_store_str(sh, "Type", 
		    volumetype_from_volsertype(v.type));
		hv_store_val(sh, "Usage", v.size);

		if (v.status != VOK)
		    continue;

		hv_store_val(sh, "MaxQuota", v.maxquota);
		hv_store_val(sh, "CreationDate", v.creationDate);
		hv_store_val(sh, "LastUpdate", v.updateDate);
		hv_store_val(sh, "NumAccess", v.dayUse);
		hv_store_val(sh, "FileCount", v.filecount);

		hv_store_val(sh, "FileCount", v.filecount);

		es = (AV *)sv_2mortal((SV *)newAV());
		for (i = 0; i < 4; i++)
			av_push(es, newRV(newSVnv(v.stat_reads[i])));

		hv_store(sh, "ReadStats", 9, (SV *)newRV((SV *)es), 0);

		es = (AV *)sv_2mortal((SV *)newAV());
		for (i = 0; i < 4; i++)
			av_push(es, newRV(newSVnv(v.stat_writes[i])));

		hv_store(sh, "WriteStats", 9, (SV *)newRV((SV *)es), 0);
	}
	aafs_volume_info_destroy_ctx(ctx);

	RETVAL = s;

    OUTPUT:
	RETVAL



AV *
aafs_vldb_query(cell, attrref)
	aafs_cell *cell
	SV * attrref
   PREINIT:
	struct aafs_vldb_query_attrs *attrs;
	struct aafs_server *server;	
	struct aafs_vldb_list *ql;
	struct aafs_vldb_ctx *vq;	
	struct aafs_volume *v;	
	int error;
	HV *sh, *attr;
	AV *s, *es;
	SV **hs;
   CODE:
	
	if ((!SvROK(attrref))
	    || (SvTYPE(SvRV(attrref)) != SVt_PVHV)) {
		crook(" attrref no pointer to hash\n");
	    XSRETURN_UNDEF;
	}

	attr = (HV *)SvRV(attrref);

	s = (AV *)sv_2mortal((SV *)newAV());

	error = aafs_vldb_query_attr_create(&attrs);
	if (error)
	    XSRETURN_UNDEF;

	hs = hv_fetch(attr, "-server", 7, 0);

	if (hs) {
	    uint32_t server_number;

	    if (!SvPOK(*hs)) {
		XSRETURN_UNDEF;
		aafs_vldb_query_attr_free(attrs);
	    }

	    error = aafs_server_create_by_name(cell,
		SvPV_nolen(*hs), &server);
	    if (error) {
		aafs_vldb_query_attr_free(attrs);
		XSRETURN_UNDEF;
	    }
	
	    error = aafs_server_get_long(server, &server_number);
	    if (error) {
		aafs_object_unref(server, "server");
		aafs_vldb_query_attr_free(attrs);
		XSRETURN_UNDEF;
	    }
	    aafs_vldb_query_attr_set_server(attrs, ntohl(server_number));
	}

	hs = hv_fetch(attr, "-partition", 10, 0);
	if (hs) {
	    if (!SvPOK(*hs)) {
		aafs_vldb_query_attr_free(attrs);
		XSRETURN_UNDEF;
	    }
	    aafs_vldb_query_attr_set_partition(attrs, 
		aafs_partition_from_name(SvPV_nolen(*hs)));
	}

	error = aafs_vldb_query(cell, attrs, &ql);
	if (error) {
	    aafs_vldb_query_attr_free(attrs);
	    XSRETURN_UNDEF;
	}

	aafs_vldb_query_attr_free(attrs);

	for (v = aafs_vldb_iterate_first(ql, &vq);
	     v != NULL;
	     v = aafs_vldb_iterate_next(vq))
	{
	    SV *sv = sv_newmortal();
            sv_setref_pv(sv, "aafs_volumePtr", (void*)v);
	    av_push(s, newRV(sv));
	}

	aafs_vldb_iterate_done(vq);
	aafs_vldb_query_free(ql);

	RETVAL = s;

    OUTPUT:
	RETVAL



MODULE = AAFS  PACKAGE = aafs_cellPtr  PREFIX = aafsc_

void
aafsc_DESTROY(cell)
	aafs_cell * cell
    CODE:

MODULE = AAFS  PACKAGE = aafs_volumePtr  PREFIX = aafsv_

void
aafsv_DESTROY(volume)
	aafs_volume * volume
    CODE:
	aafs_volume_free(volume);
