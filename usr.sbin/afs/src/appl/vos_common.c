/*	$OpenBSD: vos_common.c,v 1.1 1999/04/30 01:59:04 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$KTH: vos_common.c,v 1.7 1999/03/14 19:02:53 assar Exp $");

/*
 * partition_num2name
 *
 * write the string of the partition number `id' in `str' (which is of
 * size `sz') and return the number of characters written.
 */

int
partition_num2name (int id, char *str, size_t sz)
{
    if (id < 26)
	return snprintf (str, sz, "/vicep%c", 'a' + id);
    else
	return snprintf (str, sz, "/vicep%c%c",
			 'a' + id / 26 - 1, 'a' + id % 26);
}

/*
 * partition_name2num 
 *   convert a char string that might be a partition to a 
 *   number.
 *
 *  returns -1 is there is an error
 *
 */

int
partition_name2num(const char *name)
{
    int ret;

    if (strncmp(name, "/vicep", 6) == 0)
	name += 6;
    else if (strncmp(name, "vicep", 5) == 0)
	name += 5;

    if (*name == '\0')
	return -1;

    if(*(name+1) == '\0') {
	if(isalpha((unsigned char)*name)) {
	    ret = tolower(*name) - 'a';
	} else
	    return -1;
    } else if (name[2] == '\0') {
	if (isalpha((unsigned char)name[0])
	    && isalpha((unsigned char)name[1])) {
	    ret = 26 * (tolower(*(name)) - 'a' + 1) + tolower(*(name+1)) - 'a';
	} else
	    return -1;
    } else
	return -1;

    if(ret > 255)
	return -1;

    return ret;
}

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
    int error;

    conn = arlalib_getconnbyname(cell, host, afsvldbport, VLDB_SERVICE_ID,
				 auth);
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
