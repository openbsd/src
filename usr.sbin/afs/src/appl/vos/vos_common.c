/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "appl_locl.h"
#include <sl.h>
#include "vos_local.h"

RCSID("$KTH: vos_common.c,v 1.10.2.2 2001/09/18 00:15:32 mattiasa Exp $");

/*
 * Print the first of RW, RO, or BACKUP that there's a clone of
 * according to serverFlag.
 */

const char *
getvolumetype(int32_t flag)
{
    const char *str;

    if (flag & VLSF_RWVOL)
	str = "RW";
    else if (flag & VLSF_ROVOL)
	str = "RO";
    else if (flag & VLSF_BACKVOL)
	str = "BACKUP";
    else if (flag & VLSF_NEWREPSITE)
	str = "NewRepSite";
    else
	str = "FOO!";

    return str;
}

/*
 * Convert a volume `type' to a string.
 */

const char *
getvolumetype2(int32_t type)
{
    const char *str;

    switch (type) {
    case RWVOL:
	str = "RW";
	break;
    case ROVOL:
	str = "RO";
	break;
    case BACKVOL:
	str = "BK";
	break;
    default:
	str = "FOO!";
    }
    return str;
}

/*
 * Get the list of partitions from the server `host' in cell `cell'
 * and store them in `parts'.  Use authentication as specifined in `auth'.
 */

int
getlistparts(const char *cell, const char *host,
	     part_entries *parts, arlalib_authflags_t auth)
{
    struct rx_connection *connvolser;
    int error;
    
    connvolser = arlalib_getconnbyname(cell,
				       host,
				       afsvolport,
				       VOLSERVICE_ID,
				       auth);
    if (connvolser == NULL)
	return -1 ;
    
    error = VOLSER_AFSVolXListPartitions(connvolser, parts);
    if (error == RXGEN_OPCODE) {
	pIDs old_parts;

	error = VOLSER_AFSVolListPartitions(connvolser, &old_parts);
	if (error == 0) {
	    int n, i;

	    for (n = 0, i = 0; i < 26; ++i)
		if (old_parts.partIds[i] != -1)
		    ++n;
	    parts->len = n;
	    parts->val = emalloc(n * sizeof(*parts->val));

	    for (n = 0, i = 0; i < 26; ++i)
		if (old_parts.partIds[i] != -1)
		    parts->val[n++] = old_parts.partIds[i];
	}
    }

    if (error != 0) {
	printf("getlistparts: ListPartitions failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
	return -1;
    }

    arlalib_destroyconn(connvolser);
    return 0;
}

static void
print_fast_vols (volEntries ve)
{
    int i;

    for (i = 0; i < ve.len; ++i)
	printf ("%d\n", ve.val[i].volid);
}

static void
print_slow_vols (volEntries ve, const char *part_name, int flags)
{
    int i;
    int busy = 0, online = 0, offline = 0;

    for (i = 0; i < ve.len; i++) {
	volintInfo *vi = &ve.val[i];
	    
	if (vi->status == VBUSY)
	    busy++;
	else if (vi->inUse)
	    online++;
	else
	    offline++;

	if(vi->status == VOK) {
	    printf("%-38s %10u %s %10u K %s %s\n", 
		   vi->name,
		   vi->volid,
		   getvolumetype2(vi->type),
		   vi->size,
		   vi->inUse ? "On-line" : "Off-line",
		   flags & LISTVOL_PART ? part_name : "");
	} 
    }

    for (i = 0; i < ve.len; i++) {
	volintInfo *vi = &ve.val[i];

	if(vi->status == VBUSY)
	    printf("Volume with id number %u is currently busy\n",
		   vi->volid);
    }

    printf("\nTotal volumes onLine %d ; Total volumes offLine %d " \
	   "; Total busy %d\n\n",
	   online, offline, busy);
}

/*
 * print all the volumes of host `host' in cell `cell' and partition `part'.
 */

int
printlistvol(const char *cell, const char *host, int part, int flags,
	     arlalib_authflags_t auth)
{
    struct rx_connection *connvolser;
    part_entries parts;
    int error;
    int i;
    
    connvolser = arlalib_getconnbyname(cell,
				       host,
				       afsvolport,
				       VOLSERVICE_ID,
				       auth);
    if (connvolser == NULL)
	return -1 ;

    if (part == -1) {
	if ((error = getlistparts(cell, host, &parts,
				  auth)) != 0)
	    return -1;
    } else {
	parts.len = 1;
	parts.val = emalloc (sizeof(*parts.val));
	parts.val[0] = part;
    }

    for (i = 0; i < parts.len; ++i) {
	char part_name[17];
	volEntries volint;

	volint.val = NULL;
	if ((error = VOLSER_AFSVolListVolumes(connvolser,
					      parts.val[i],
					      1, /* We want full info */
					      &volint)) != 0) {
	    printf("printlistvol: PartitionInfo failed with: %s (%d)\n", 
		   koerr_gettext(error),
		   error);
	    return -1;
	}
	partition_num2name (parts.val[i], part_name, sizeof(part_name));

	printf("Total number of volumes on server %s partition %s: %d\n",
	       host, part_name, volint.len);

	if (flags & LISTVOL_FAST)
	    print_fast_vols (volint);
	else
	    print_slow_vols (volint, part_name, flags);
	free(volint.val);
    }

    arlalib_destroyconn(connvolser);
    return 0;
}

/*
 * Return the volume entry for `volname' in `cell' by asking the DB
 * server at `host', with the auth flags in `auth' and returning the
 * result in `nvldbentry'.  Returns 0 or error.
 */

int
get_vlentry (const char *cell, const char *host, const char *volname,
	     arlalib_authflags_t auth, nvldbentry *nvldbentry)
{
    struct rx_connection *conn;
    struct db_server_context *conn_context = NULL;
    int error;

    if(host == NULL) {
	conn_context = malloc(sizeof(struct db_server_context));
	memset(conn_context, 0, sizeof(struct db_server_context));
	for (conn = arlalib_first_db(conn_context,
				     cell, host, afsvldbport, VLDB_SERVICE_ID, 
				     auth);
	     conn != NULL && arlalib_try_next_db(error);
	     conn = arlalib_next_db(conn_context));
    } else {
	conn = arlalib_getconnbyname(cell, host, afsvldbport, VLDB_SERVICE_ID,
				     auth);
    }

    if (conn == NULL)
	return -1;

    error = VL_GetEntryByNameN (conn, volname, nvldbentry);

    if (error == RXGEN_OPCODE) {
	vldbentry vlentry;

	error = VL_GetEntryByName (conn, volname, &vlentry);
	if (error == 0)
	    vldb2vldbN (&vlentry, nvldbentry);
    }

    arlalib_destroyconn(conn);

    if(conn_context != NULL) {
	free_db_server_context(conn_context);
	free(conn_context);
    }

    return error;
}


/* 
 * insert `nvldbentry' to the dbserver using `conn' or it `conn' ==
 * NULL use `cell' (and if specified `host') to get a new conn. If the
 * db-server is old, use old method.  
 */

int
new_vlentry (struct rx_connection *conn, const char *cell, const char *host,
	     nvldbentry *nvldbentry, arlalib_authflags_t auth)
{
    int error;
    int freeconnp = 0;

    if (conn == NULL) {
	find_db_cell_and_host (&cell, &host);

	if (cell == NULL) {
	    fprintf (stderr, "Unable to find cell of host '%s'\n", host);
	    return -1;
	}

	if (host == NULL) {
	    fprintf (stderr, "Unable to find DB server in cell '%s'\n", cell);
	    return -1;
	}
	
	conn = arlalib_getconnbyname(cell, host, afsvldbport, VLDB_SERVICE_ID,
				     auth);
	freeconnp = 1;
	if (conn == NULL)
	    return -1;
    }

    error = VL_CreateEntryN (conn, nvldbentry);

    if (error == RXGEN_OPCODE) {
#if 0
	vldbentry vlentry;

	vldbN2vldb (nvldbentry, &vlentry);
	error = VL_CreateEntry (conn, volname, &vlentry);
#endif
	abort();
    }
    if (freeconnp)
	arlalib_destroyconn(conn);
    return error;
}

/*
 * Try to set *cell and *host to reasonable values.
 */

void
find_db_cell_and_host (const char **cell, const char **host)
{
    if (*cell == NULL && *host != NULL) {
	*cell = cell_getcellbyhost (*host);
	return;
    } 
    if (*cell == NULL) {
	*cell = cell_getthiscell();
    }
    if (*host == NULL) {
	*host = cell_findnamedbbyname (*cell);
    }
}

/*
 * give a name for the server `addr'
 */

void
get_servername (u_int32_t addr, char *str, size_t str_sz)
{
    struct sockaddr_in sock;
    int error;

    memset (&sock, 0, sizeof(sock));
#if HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sock.sin_len = sizeof(sock);
#endif
    sock.sin_family = AF_INET;
    sock.sin_port = 0;
    sock.sin_addr.s_addr = addr;

    error = getnameinfo((struct sockaddr *)&sock, sizeof(sock),
			str, str_sz, NULL, 0, 0);
    if (error)
	strlcpy (str, "<unknown>", str_sz);
}
