/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
 *      Höskolan and its contributors.
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

#include <sl.h>
#include "appl_locl.h"

RCSID("$Id: pts.c,v 1.1 2000/09/11 14:40:36 art Exp $");

static int help_cmd (int argc, char **argv);

/* Debugging on/off */
static int prdebug = 0;

static int
empty_cmd (int argc, char **argv)
{
    printf ("%s not implemented yet!\n", argv[0]);
    return 0;
}

static void
dump_usage (void)
{
    printf("Usage: pts dump <vldb server>+ [-cell <cell>]\n");
}

static int
dump_cmd (int argc, char **argv)
{
    struct rx_connection  *connptdb = NULL;
    struct prdebugentry    entry;
    struct prheader        header;
    const char            *host;
    int                    noauth = 0;
    unsigned int           pos;
    int                    error;

    argc--;
    argv++;

    if (argc < 1) {
	dump_usage ();
	return -1;
    }

    host = argv[0];

    connptdb = arlalib_getconnbyname(cell_getcellbyhost(host),
				     host,
				     afsprport,
				     PR_SERVICE_ID,
				     arlalib_getauthflag (noauth, 0, 0, 0));

    if (connptdb == NULL)
	return 0;

    error = PR_DumpEntry(connptdb, 0, (struct prdebugentry *) &header);

    if (error) {
	printf("dump_cmd: DumpEntry failed with: %s (%d)\n",
	       koerr_gettext(error),
	       error);
	return 0;
    }

    for (pos = header.headerSize; pos < header.eofPtr; pos += sizeof (struct prdebugentry)) {
	error = PR_DumpEntry(connptdb, pos, &entry);
	if (error) {
	    printf("dump_cmd: DumpEntry failed with: %s (%d)\n",
		   koerr_gettext(error),
		   error);
/*	    return -1;*/
	}
	else {
	    printf("-----\n");
	    printf("Name: %s, id: %d, owner: %d, creator: %d,\n",
		   entry.name, entry.id, entry.owner, entry.creator);
	    printf("  membership: %d, flags:", entry.ngroups);
	    if ((entry.flags & PRTYPE) == PRFREE)
		printf (" PRFREE");
	    if ((entry.flags & PRTYPE) == PRGRP)
		printf (" PRGRP");
	    if ((entry.flags & PRTYPE) == PRCONT)
		printf (" PRCONT");
	    if ((entry.flags & PRTYPE) == PRCELL)
		printf (" PRCELL");
	    if ((entry.flags & PRTYPE) == PRFOREIGN)
		printf (" PRFOREIGN");
	    if ((entry.flags & PRTYPE) == PRINST)
		printf (" PRINST");
	    if ((entry.flags & PRTYPE) == PRUSER)
		printf (" PRUSER");
	    if ((entry.flags & PRACCESS) == PRACCESS)
		printf (" PRACCESS");
	    if ((entry.flags & PRQUOTA) == PRQUOTA)
		printf (" PRQUOTA");

	    printf (" , group quota: %d.\n",
		    entry.ngroups);
	}
    }

    arlalib_destroyconn(connptdb);
    return 0;
}

static int
flags_to_string(int flags, char *string)
{
    strcpy(string, "-----");
    if((flags & PRP_STATUS_ANY) != 0)
	string[0]='S';
    else if((flags & PRP_STATUS_MEM) != 0)
	string[0]='s';
    if((flags & PRP_OWNED_ANY) != 0)
	string[1]='O';
    if((flags & PRP_MEMBER_ANY) != 0)
	string[2]='M';
    else if((flags & PRP_MEMBER_MEM) != 0)
	string[2]='m';
    if((flags & PRP_ADD_ANY) != 0)
	string[3]='A';
    else if((flags & PRP_ADD_MEM) != 0)
	string[3]='a';
    if((flags & PRP_REMOVE_MEM) != 0)
	string[4]='r';
    return 0;
}

static int
examine_id(struct rx_connection *conn, int32_t id, char *idname)
{
    prcheckentry ent;
    int error;
    namelist nlist;
    idlist ilist;
    char flags_str[6];

    error = PR_ListEntry(conn, id, &ent);
    if (error) {
        fprintf(stderr, "ListEntry in examine: error %s (%d)\n",
		koerr_gettext(error), error);
        return error;
    } 

    nlist.len = 3;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    if(nlist.val == NULL)
        errx(1, "Out of memory");

    ilist.len = 3;
    ilist.val = malloc(sizeof(int32_t) * ilist.len);
    if(ilist.val == NULL)
        errx(1, "Out of memory");

    ilist.val[0] = ent.id;
    ilist.val[1] = ent.owner;
    ilist.val[2] = ent.creator;
    
    error = PR_IDToName(conn, &ilist, &nlist);
    if (error) {
      fprintf(stderr, "IDToName in examine: error %s (%d)\n",
	      koerr_gettext(error), error);
      return error;
    } 

    flags_to_string(ent.flags << 16, flags_str); /* XXX why do i have to shift by 16? seems strange */

    printf("Name: %s, id: %d, owner: %s, creator: %s,\n", 
           nlist.val[0], ent.id, nlist.val[1], nlist.val[2]);
    printf("  membership: %d, flags: %s, group quota: %d.\n",
	   ent.count, flags_str, ent.ngroups);
    free(ilist.val);
    free(nlist.val);
    return 0;
}


/*
 * convert a `name' to `id'
 */

static int
pr_name2id(struct rx_connection *conn, const char *name, int32_t *id)
{
    int error;
    namelist nlist;
    idlist ilist;
    char rname[PR_MAXNAMELEN];

    assert(id);

    if (prdebug)
	printf("pr_name2id(%s, x)", name);

    strlcpy(rname, name, sizeof(rname));
    
    nlist.len = 1;
    nlist.val = &rname;

    error = PR_NameToID(conn, &nlist, &ilist);

    if (error || ilist.len != 1)
	*id = 0;
    else
	*id = *ilist.val;
	
    if (prdebug)
	printf(" id = %d, error= %d\n", *id, error);

    free(ilist.val);
    
    return error;
}

/*
 * add `user' to `group' on `conn'.  Return 0 or error.
 */

static int
adduser_1 (struct rx_connection *conn, const char *user, const char *group)
{
    int32_t uid;
    int32_t gid;
    int error;
      
    error = pr_name2id(conn, user, &uid);
    if (error) {
	if (prdebug)
	    warn("Problems finding user %s, error %s (%d)",
		 user, koerr_gettext(error), error);
	  return error;
    }
    
    error = pr_name2id(conn, group, &gid);
    if (error) {
	if (prdebug)
	    warn("Problems finding group %s, error %s (%d)",
		 user, koerr_gettext(error), error);
	return error;
    }
    
    if (prdebug)
	printf("PR_AddToGroup(conn, uid = %d, gid = %d);\n", uid, gid);
      
    error = PR_AddToGroup(conn, uid, gid);
    return error;
}

/*
 * add `user' to `group' in `cell'
 */

static int
pr_adduser(const char *cell, const char *host,
	   const char *user, const char *group,
	   arlalib_authflags_t auth)
{
    int error = ENETDOWN;
    struct rx_connection *conn;
    struct db_server_context conn_context;

    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db(error);
	 conn = arlalib_next_db(&conn_context)) {
	error = adduser_1 (conn, user, group);
    }
    free_db_server_context(&conn_context);
    return error;
}


/*
 * create user/group
 *  if you want the id returned
 *      set *id = 0
 *  if you want to decide what the userid is set the *id != 0, 
 *  id is still returned in this variable.
 *  
 */

#define PR_CREATE_USER  0
#define PR_CREATE_GROUP 2

static int
pr_create(const char *cell, const char *host,
	  const char *name, const char *owner,
	  int32_t *id, int flag, arlalib_authflags_t auth)
{
    int error;
    struct rx_connection *conn;
    struct db_server_context conn_context;

    error = ENETDOWN;
    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db (error);
	 conn = arlalib_next_db(&conn_context)) {
	int32_t owner_id = 0;
	int32_t out_id;

	if (owner != NULL) {
	    error = pr_name2id (conn, owner, &owner_id);
	    if (error)
		continue;
	}

	if (id == NULL || *id == 0) {
	    if (prdebug)
		printf("PR_NewEntry(%s, %d, %d, OUT)\n",
		       name, flag, owner_id);

	    error = PR_NewEntry(conn, name, flag, owner_id, &out_id);
	    if (id != NULL && error == 0)
		*id = out_id;
	} else {
	    if (prdebug)
		printf("PR_INewEntry(%s, %d, %d, OUT)\n",
		       name, *id, owner_id);

	    error = PR_INewEntry(conn, name, *id, owner_id);
	}
    }
    if(error)
	fprintf(stderr, "pts create: error %s (%d)\n",
		koerr_gettext(error), error);
    free_db_server_context(&conn_context);
    return error;
}

/*
 *
 */

static int
delete_1 (struct rx_connection *conn, const char *name)
{
    int error;
    int32_t id;

    error = pr_name2id(conn, name, &id);
    if (error) {
	if (prdebug)
	    warn("Problems finding name: %s, error %s (%d)",
		 name, koerr_gettext(error), error);
	return error;
    }

    if (prdebug)
	printf("PR_Delete(%s, %d)\n", name, id);

    error = PR_Delete(conn, id);
    return error;
}

static int
pr_delete(const char *cell, const char *host,
	  const char *name, arlalib_authflags_t auth)
{
    int error = ENETDOWN;
    struct rx_connection *conn;
    struct db_server_context conn_context;

    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db(error);
	 conn = arlalib_next_db(&conn_context)) {
	error = delete_1 (conn, name);
    }
    free_db_server_context(&conn_context);
    return error;
}    


static int
remove_1(struct rx_connection *conn, const char *user, const char *group)
{
    int error;
    int32_t uid;
    int32_t gid;

    error = pr_name2id(conn, user, &uid);
    if (error) {
	if (prdebug)
	    warn("Problems finding name: %s, error %s (%d)",
		 user, koerr_gettext(error), error);
	return error;
    }

    error = pr_name2id(conn, group, &gid);
    if (error) {
	if (prdebug)
	    warn("Problems finding name: %s, error %s (%d)",
		 group, koerr_gettext(error), error);
	return error;
    }
    
    if (prdebug)
	printf("PR_RemoveFromGroup(%d, %d)\n", uid, gid);

    error = PR_RemoveFromGroup(conn, uid, gid);
    return error;
}

/*
 *
 */

static int
pr_removeuser(const char *cell, const char *host,
	      const char *user, const char *group,
	      arlalib_authflags_t auth)
{
    int error = ENETDOWN;
    struct rx_connection *conn;
    struct db_server_context conn_context;

    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db(error);
	 conn = arlalib_next_db(&conn_context)) {
	error = remove_1 (conn, user, group);
    }
    free_db_server_context(&conn_context);
    return error;
}

static int
rename_1 (struct rx_connection *conn,
	 const char *fromname, const char *toname)
{
    int error;
    int32_t id;

    error = pr_name2id(conn, fromname, &id);
    if (error) {
	if (prdebug)
	    warn("Problems finding name: %s, error %s (%d)",
		 fromname, koerr_gettext(error), error);
	return error;
    }

    if (prdebug)
	printf("PR_ChangeEntry(%d, %s, 0, 0)\n", id, toname);

    error = PR_ChangeEntry(conn, id, toname, 0, 0);
    return error;
}

/*
 *
 */

static int
pr_rename(const char *cell, const char *host,
	  const char *fromname, const char *toname,
	  arlalib_authflags_t auth)
{
    int error = ENETDOWN;
    struct rx_connection *conn;
    struct db_server_context conn_context;

    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db(error);
	 conn = arlalib_next_db(&conn_context)) {
	error = rename_1 (conn, fromname, toname);
    }
    free_db_server_context(&conn_context);
    return error;
}

static int
setfields_1 (struct rx_connection *conn, const char *name,
	     int mask, int flags, int ngroup, int nusers)
{
    int error;
    int32_t id;

    error = pr_name2id(conn, name, &id);
    if (error) {
	if (prdebug)
	    warn("Problems finding name: %s, error %s (%d)",
		 name, koerr_gettext(error), error);
	return error;
    }

    if (prdebug)
	printf("PR_ChangeFields(%d, %d, %d, %d, %d, 0, 0)\n",
	       id, mask, flags, ngroup, nusers);

    error = PR_SetFieldsEntry(conn, id, mask, flags, ngroup, nusers, 0, 0);
    return error;
}

/*
 *
 */

static int
pr_setfields(const char *cell, const char *host,
	     const char *name, int flags, int ngroup, int nusers,
	     arlalib_authflags_t auth)
{
    int error = ENETDOWN;
    struct rx_connection *conn;
    struct db_server_context conn_context;
    int mask = 0;

    if (flags != -1)
	mask |= PR_SF_ALLBITS;
    if (ngroup != -1)
	mask |=  PR_SF_NGROUPS;
    if (nusers != -1)
	mask |=  PR_SF_NUSERS;

    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db(error);
	 conn = arlalib_next_db(&conn_context)) {
	error = setfields_1 (conn, name, mask, flags, ngroup, nusers);
    }
    free_db_server_context(&conn_context);
    return error;
}

static int
chown1(struct rx_connection *conn, const char *name, const char *owner)
{
    int error;
    int32_t id;
    int32_t ownerid;

    error = pr_name2id(conn, name, &id);
    if (error) {
	if (prdebug)
	    warn("Problems finding name: %s, error %s (%d)",
		 name, koerr_gettext(error), error);
	return error;
    }

    error = pr_name2id(conn, owner, &ownerid);
    if (error) {
	if (prdebug)
	    warn("Problems finding name: %s, error %s (%d)",
		 name, koerr_gettext(error), error);
	return error;
    }

    if (prdebug)
	printf("PR_ChangeEntry(%d, \"\", %d, 0)\n",
	       id, ownerid);

    error = PR_ChangeEntry(conn, id, "", ownerid, 0);
    return error;
}

/*
 *
 */

static int
pr_chown(const char *cell, const char *host,
	 const char *name, const char *owner,
	 arlalib_authflags_t auth)
{
    int error = ENETDOWN;
    struct rx_connection *conn;
    struct db_server_context conn_context;

    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db(error);
	 conn = arlalib_next_db(&conn_context)) {
	error = chown1 (conn, name, owner);
    }
    free_db_server_context(&conn_context);
    return error;
}

static int
pts_id_to_name(struct rx_connection *conn, int id, prname *pr)
{
    int32_t res = 0;
    namelist nlist;
    idlist ilist;
    nlist.len = 1;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    ilist.len = 1;
    ilist.val = malloc(sizeof(int32_t) * ilist.len);
    if((ilist.val == NULL) || (nlist.val == NULL))
	errx(1, "Out of memory");
    ilist.val[0]=id;
    res = PR_IDToName(conn, &ilist, &nlist);
    if(res == 0)
	strlcpy((char *)pr, (char *)nlist.val[0], PR_MAXNAMELEN);
    free(ilist.val);
    free(nlist.val);
    return res;
}

static int
pts_name_to_id(struct rx_connection *conn, const char *name, int32_t *id)
{
    int32_t res = 0;
    namelist nlist;
    idlist ilist;
    int isdig = 1;
    const char *ptr = name;

    while(*ptr && isdig) {
	if(!isdigit((unsigned char)*ptr))
	    isdig = 0;
	ptr++;
    }
    if(isdig) {
	*id = atoi(name);
	return 0;
    }

    nlist.len = 1;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    ilist.len = 1;
    ilist.val = malloc(sizeof(int32_t) * ilist.len);
    if((nlist.val == NULL) || (ilist.val == NULL))
	errx(1, "Out of memory");
    strlcpy(nlist.val[0], name, sizeof(prname));
    res = PR_NameToID(conn, &nlist, &ilist);
    *id = ilist.val[0];
    free(ilist.val);
    free(nlist.val);
    return res;
}

static int
listowned_1 (struct rx_connection *conn, const char *user)
{
    int error;
    int32_t id;
    int32_t over;
    prname *name;
    prlist pr;
    int i;

    printf("%s\n", user);

    error = pts_name_to_id (conn, user, &id);
    if(error != 0)
        return error;

    pr.len = PR_MAXGROUPS;
    pr.val = malloc(sizeof(int32_t) * pr.len);
    if(pr.val == NULL)
	errx(1, "Out of memory");

    printf("%d\n", id);
    error = PR_ListOwned(conn, id, &pr, &over);
    if(error != 0)
	return error;

    printf("id = %d\n", id);
    printf("pr.len = %d\n", pr.len);

    i = 0;
    name = malloc(PR_MAXNAMELEN);
    if(name == NULL)
	errx(1, "Out of memory");

    printf("Groups owned by %s (id: %d) are:\n", user, id);

    while(pr.val[i] != 0 && i < pr.len) {
	pts_id_to_name(conn, pr.val[i++], name);
	printf("  %s\n", (char *) name);
    }

    free(pr.val);
    return 0;
}

static int
pr_listowned (const char *cell, const char *host, const char *user,
	      arlalib_authflags_t auth)
{
    int error = ENETDOWN;
    struct rx_connection *conn;
    struct db_server_context conn_context;

    for (conn = arlalib_first_db(&conn_context,
				 cell, host, afsprport, PR_SERVICE_ID, auth);
	 conn != NULL && arlalib_try_next_db(error);
	 conn = arlalib_next_db(&conn_context)) {
	error = listowned_1 (conn, user);
    }
    free_db_server_context(&conn_context);
    return error;
}

/*
 * create user
 */

static int
create_cmd(int argc, char **argv, int groupp, const char *cmd_name)
{
    const char *name;
    const char *owner = NULL;
    const char *cell = (char *) cell_getthiscell();
    const char *host = NULL;
    int32_t id = 0;
    int noauth = 0;
    int error;
    int optind = 0;
    
    struct getargs createuserarg[] = {
	{"name",	0, arg_string,  NULL, NULL, NULL, arg_mandatory},
	{"owner",	0, arg_string,	NULL, "owner of the group"},
	{"id",		0, arg_integer, NULL, "id of user", NULL},
	{"cell",	0, arg_string,  NULL, "what cell to use", NULL},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,      0, arg_end, NULL}}, 
					 *arg;

    arg = createuserarg;
    if (groupp)
	arg->help = "name of user";
    else
	arg->help = "name of group";
    arg->value = &name;   arg++;
    arg->value = &owner;  arg++;
    arg->value = &id;     arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (!groupp)
	memmove (&createuserarg[1], &createuserarg[2],
		 6 * sizeof(createuserarg[0]));
	
    if (getarg (createuserarg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(createuserarg, cmd_name, NULL, ARG_AFSSTYLE);
	return 0;
    }
    
    error = pr_create(cell, host, name, owner, &id, groupp, 
		      arlalib_getauthflag (noauth, 0, 0, 0));
    if (error) {
	printf("pr_create failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
    } else
	printf("%s %s created with id %d\n", 
	       groupp ? "Group": "User", name, id);
    
    return 0;
}

/*
 *
 */

static int
createuser_cmd(int argc, char **argv)
{
    return create_cmd(argc, argv, PR_CREATE_USER, "pts createuser");
}

/*
 *
 */

static int
creategroup_cmd(int argc, char **argv)
{
    return create_cmd(argc, argv,  PR_CREATE_GROUP, "pts creategroup");
}

/*
 *
 */

static int
adduser_cmd(int argc, char **argv)
{
    const char *user;
    const char *group;
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    int noauth = 0;
    int error;
    int optind = 0;
    
    struct getargs addarg[] = {
	{"name",	0, arg_string,  NULL, "username", NULL, arg_mandatory},
	{"group",	0, arg_string,  NULL, "groupname",NULL, arg_mandatory},
	{"cell",	0, arg_string,  NULL, "what cell to use", NULL},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,      0, arg_end, NULL}}, 
					 *arg;

    arg = addarg;
    arg->value = &user;   arg++;
    arg->value = &group;  arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (addarg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(addarg, "pts adduser", NULL, ARG_AFSSTYLE);
	return 0;
    }
    
    error = pr_adduser(cell, host, user, group, 
		       arlalib_getauthflag (noauth, 0, 0, 0));
    if (error) {
	printf("pr_adduser failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
    }
    
    return 0;
}

/*
 *
 */

static int
delete_cmd(int argc, char **argv)
{
    const char *user;
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    int noauth = 0;
    int error;
    int optind = 0;
    
    struct getargs deletearg[] = {
	{"name",	0, arg_string,  NULL, "username", NULL, arg_mandatory},
	{"cell",	0, arg_string,  NULL, "what cell to use", NULL},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,      0, arg_end, NULL}}, 
					 *arg;

    arg = deletearg;
    arg->value = &user;   arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (deletearg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(deletearg, "pts delete", NULL, ARG_AFSSTYLE);
	return 0;
    }
    
    error = pr_delete(cell, host, user, 
		      arlalib_getauthflag (noauth, 0, 0, 0));
    if (error) {
	printf("pr_delete failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
    } else
	if (prdebug)
	    printf("Entry %s deleted successful\n", user);
    
    return 0;
}

/*
 *
 */

static int
removeuser_cmd(int argc, char **argv)
{
    const char *user;
    const char *group;
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    int noauth = 0;
    int error;
    int optind = 0;
    
    struct getargs removearg[] = {
	{"user",	0, arg_string,  NULL, "username", NULL, arg_mandatory},
	{"group",	0, arg_string,  NULL, "group", NULL, arg_mandatory},
	{"cell",	0, arg_string,  NULL, "what cell to use", NULL},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,      0, arg_end, NULL}}, 
					 *arg;

    arg = removearg;
    arg->value = &user;   arg++;
    arg->value = &group;  arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (removearg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(removearg, "pts remove", NULL, ARG_AFSSTYLE);
	return 0;
    }
    
    error = pr_removeuser(cell, host, user, group, 
			  arlalib_getauthflag (noauth, 0, 0, 0));
    if (error) {
	printf("pr_remove failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
    } else
	if (prdebug)
	    printf("User %s removed from group %s.\n", user, group);
    
    return 0;
}

/*
 *
 */

static int
rename_cmd(int argc, char **argv)
{
    const char *fromname;
    const char *toname;
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    int noauth = 0;
    int error;
    int optind = 0;
    
    struct getargs renamearg[] = {
	{"from",	0, arg_string,  NULL, "from name",NULL, arg_mandatory},
	{"to", 	  	0, arg_string,  NULL, "to name", NULL, arg_mandatory},
	{"cell",	0, arg_string,  NULL, "what cell to use", NULL},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,      0, arg_end, NULL}}, 
					 *arg;

    arg = renamearg;
    arg->value = &fromname;arg++;
    arg->value = &toname; arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (renamearg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(renamearg, "pts rename", NULL, ARG_AFSSTYLE);
	return 0;
    }
    
    error = pr_rename(cell, host, fromname, toname, 
		      arlalib_getauthflag (noauth, 0, 0, 0));
    if (error) {
	printf("pr_rename failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
    } else
	if (prdebug)
	    printf("Changed name from %s to %s.\n", fromname, toname);
    
    return 0;
}


/*
 *
 */

static int 
setfields_error(void)
{
    printf("text must be a union of the sets 'SOMA-' and 's-mar'\n");
    return 0;
}

/*
 *
 */

static int
setfields_cmd(int argc, char **argv)
{
    const char *name;
    const char *strflags = NULL;
    int flags = -1;
    int gquota = -1;
    int uquota = -1;
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    int noauth = 0;
    int error;
    int optind = 0;
    
    struct getargs setfieldarg[] = {
	{"name",	0, arg_string,  NULL, "name of user/group",
	 NULL, arg_mandatory},
	{"flags", 	0, arg_string,  NULL, "flags", NULL},
	{"groupquota", 	0, arg_integer, NULL, "groupquota",NULL},
	{"cell",	0, arg_string,  NULL, "what cell to use", NULL},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,      0, arg_end, NULL}}, 
					 *arg;

    arg = setfieldarg;
    arg->value = &name;   arg++;
    arg->value = &flags;  arg++;
    arg->value = &gquota; arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (setfieldarg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(setfieldarg, "pts setfields", NULL, ARG_AFSSTYLE);
	return 0;
    }

    if(strflags) {
	if (strlen(strflags) != 5) 
	    return setfields_error();
	
	flags = 0;

	if (strflags[0] == 'S')
	    flags |=  PRP_STATUS_ANY;
	else if (strflags[0] == 's')
	    flags |=  PRP_STATUS_MEM;
	else if (strflags[0] != '-')
	    return setfields_error();

	if (strflags[1] == 'O')
	    flags |=  PRP_OWNED_ANY;
	else if (strflags[1] != '-')
	    return setfields_error();

	if (strflags[2] == 'M')
	    flags |= PRP_MEMBER_ANY;
	else if (strflags[2] == 'm')
	    flags |= PRP_MEMBER_MEM;
	else if (strflags[2] != '-') 
	    return setfields_error();
	    
	if (strflags[3] == 'A') {
	    flags |= PRP_ADD_ANY;
	} else if (strflags[3] == 'a')
	    flags |= PRP_ADD_MEM;
	else if (strflags[3] != '-') 
	    return setfields_error();
	
	if (strflags[4] == 'r')
	    flags |= PRP_REMOVE_MEM;
	else if (strflags[4] != '-') 
	    return setfields_error();
    }
    
    error = pr_setfields(cell, host, name, flags, gquota, uquota, 
			 arlalib_getauthflag (noauth, 0, 0, 0));
    if (error) {
	printf("pr_setfields failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
    } else
	if (prdebug)
	    printf("Changed fields for %s.\n", name);
    
    return 0;
}

/*
 *
 */

static int
chown_cmd(int argc, char **argv)
{
    const char *name;
    const char *owner;
    const char *cell = cell_getthiscell();
    const char *host;
    int noauth = 0;
    int error;
    int optind = 0;
    
    struct getargs chownarg[] = {
	{"name",	0, arg_string,  NULL, "user or group name",
	 NULL, arg_mandatory},
	{"owner", 	  	0, arg_string,  NULL, "new owner", 
	 NULL, arg_mandatory},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"cell",	0, arg_string,  NULL, "what cell to use", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,      0, arg_end, NULL}}, 
				    *arg;
    
    arg = chownarg;
    arg->value = &name;   arg++;
    arg->value = &owner;  arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (chownarg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(chownarg, "pts chown", NULL, ARG_AFSSTYLE);
	return 0;
    }
    
    error = pr_chown(cell, host, name, owner, 
		     arlalib_getauthflag (noauth, 0, 0, 0));
    if (error) {
	printf("pr_chown failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
    } else
	if (prdebug)
	    printf("Changed owner of %s to %s.\n", name, owner);
    
    return 0;
}

/*
 * examine `users' in `cell'
 */

static int
examine1 (struct rx_connection *conn, getarg_strings *users)
{
    namelist nlist;
    idlist ilist;
    int i;
    int error;

    nlist.len = users->num_strings;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    if(nlist.val == NULL)
	errx(1, "Out of memory.");
      
    ilist.len = nlist.len;
    ilist.val = malloc(sizeof(int32_t)*ilist.len);
    if(ilist.val == NULL)
	errx(1, "Out of memory.");
      
    for(i = 0; i < nlist.len; i++)
	strlcpy(nlist.val[i], users->strings[i], sizeof(prname));
      
    error = PR_NameToID(conn, &nlist, &ilist);
    if (error) {
	free(nlist.val);
	free(ilist.val);
	return error;
    }

    for(i = 0; i < nlist.len; i++) {
	if(ilist.val[i] == PR_ANONYMOUSID) {
	    ilist.val[i] = atoi(nlist.val[i]);
	} 
    } 

    for(i = 0; i < nlist.len; i++) {
	error = examine_id(conn, ilist.val[i], nlist.val[i]);
	if (error)
	    break;
    }
    free(nlist.val);
    free(ilist.val);
    return error;
}


/*
 *
 */

static int
examine_cmd (int argc, char **argv)
{
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    struct rx_connection *connptdb = NULL;
    struct db_server_context conn_context;
    int noauth = 1;
    getarg_strings users = {0 , NULL };
    int optind = 0;
    int error = -1;
    
    struct getargs examinearg[] = {
	{"nameorid",	0, arg_strings,  NULL, "user or group name",
	 NULL, arg_mandatory},
	{"cell",	0, arg_string,  NULL, "what cell to use",
	 NULL, arg_optional},
	{"host",	0, arg_string,	NULL, "specified db",
	 NULL, arg_optional},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate",
	 NULL, arg_optional},
	{NULL,      0, arg_end, NULL}}, *arg;
    
    arg = examinearg;
    arg->value = &users;  arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (examinearg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(examinearg, "pts examine", NULL, ARG_AFSSTYLE);
	return 0;
    }

    error = ENETDOWN;
    for (connptdb = arlalib_first_db(&conn_context, cell, host, afsprport,
				     PR_SERVICE_ID,
				     arlalib_getauthflag (noauth, 0, 0, 0));
	 connptdb != NULL && arlalib_try_next_db (error);
	 connptdb = arlalib_next_db(&conn_context)) {
	error = examine1 (connptdb, &users);
    }
    free_db_server_context(&conn_context);
    return 0;
}

static int
listmax_cmd (int argc, char **argv)
{
    int error;
    int32_t uid = 0;
    int32_t gid = 0;
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    struct rx_connection *connptdb = NULL;
    struct db_server_context conn_context;
    int optind = 0;
    int noauth = 0;

    struct getargs examinearg[] = {
	{"cell",	0, arg_string,  NULL, "what cell to use",
	 NULL, arg_optional},
	{"host",	0, arg_string,	NULL, "specified db",
	 NULL, arg_optional},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate",
	 NULL, arg_optional},
	{NULL,      0, arg_end, NULL}}, *arg;
    
    arg = examinearg;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (examinearg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(examinearg, "pts listmax", NULL, ARG_AFSSTYLE);
	return 0;
    }

    error = ENETDOWN;
    for (connptdb = arlalib_first_db(&conn_context, cell, host, afsprport,
				     PR_SERVICE_ID,
				     arlalib_getauthflag (noauth, 0, 0, 0));
	 connptdb != NULL && arlalib_try_next_db (error);
	 connptdb = arlalib_next_db(&conn_context)) {
	error = PR_ListMax (connptdb, &uid, &gid);
    }
    free_db_server_context(&conn_context);
    if (error == 0)
	printf("Max user id is %d and max group id is %d.\n", uid, gid);
    else
	warnx("PR_ListMax failed: %s", koerr_gettext (error));
    return 0;
}

static int
listmembershipbyname(struct rx_connection *conn, const char *name)
{
    int32_t res = 0;
    int32_t id = 0;

    res = pts_name_to_id(conn, name, &id);
    if(res) {
	fprintf(stderr, "membership: error %s (%d)\n",
		koerr_gettext(res), res);
        return res;
    } else {
	int32_t over;
	int i;
	prlist elist;
	elist.len = PR_MAXGROUPS; /* XXX this will allocate 5000 ints, should
				  check how many groups first. That will take
				  a lot of code though. */
	elist.val = malloc(sizeof(int32_t) * elist.len);
	if(elist.val == NULL)
	    errx(1, "Out of memory");
	res = PR_ListElements(conn, id, &elist, &over);
	if(res != 0) {
	    fprintf(stderr, "membership: error %s (%d)\n",
		    koerr_gettext(res), res);
	    return res;
	} else {
	    if(id>=0)
		printf("Groups %s (id: %d) is a member of:\n", name, id);
	    else
		printf("Members of %s (id: %d) are:\n", name, id);
	    for(i = 0; i < elist.len; i++) {
		prname pr;
		res = pts_id_to_name(conn, elist.val[i], &pr);
		printf("  %s\n", (char *)&pr);
	    }
	    free(elist.val);
	    return 0;
	}
    }
}

static int
member_cmd (int argc, char **argv)
{
    int error;
    const char *cell = cell_getthiscell ();
    const char *host = NULL;
    struct rx_connection *connptdb = NULL;
    struct db_server_context conn_context;
    int32_t noauth = 0;
    int i = 0;
    int optind = 0;
    getarg_strings users = {0 , NULL };

    struct getargs memarg[] = {
	{"nameorid",	0, arg_strings,  NULL, "user or group name",
	 NULL, arg_mandatory},
	{"cell",	0, arg_string,  NULL, "what cell to use",
	 NULL, arg_optional},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth",	0, arg_flag,    NULL, "don't authenticate",
	 NULL, arg_optional},
	{NULL,      0, arg_end, NULL}}, *arg;
    
    arg = memarg;
    arg->value = &users;  arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;
    
    if (getarg (memarg, argc, argv, &optind, ARG_AFSSTYLE)) {
	arg_printusage(memarg, "pts membership", NULL, ARG_AFSSTYLE);
	return 0;
    }

    for(i=0;i<users.num_strings;i++)
	printf("%s\n", users.strings[i]);
    
    error = ENETDOWN;
    for (connptdb = arlalib_first_db(&conn_context, cell, host, afsprport,
				     PR_SERVICE_ID,
				     arlalib_getauthflag (noauth, 0, 0, 0));
	 connptdb != NULL && arlalib_try_next_db (error);
	 connptdb = arlalib_next_db(&conn_context)) {
	error = 0;
	for (i = 0; i < users.num_strings; ++i) {
	    int tmp = listmembershipbyname(connptdb, users.strings[i]);
	    if (error == 0 && tmp != 0)
		error = tmp;
	}
    }
    free_db_server_context(&conn_context);
    return 0;
}

static void
listowned_usage(void)
{
    printf("Usage: pts [-id] <user or group> [-cell <cell>] [-noauth] [-help]\n");
}

static int
listowned_cmd(int argc, char **argv)
{
    int error;
    int optind = 0;
    const char *user = NULL;
    const char *cell = cell_getthiscell();
    const char *host = NULL;
    int noauth = 0;
    
    static struct getargs listowned_args[] = {
	{"id",	   0, arg_string,  NULL,  "id of user/group",
	 NULL, arg_mandatory},
	{"cell",   0, arg_string,  NULL, "what cell to use",
	 NULL},
	{"host",	0, arg_string,	NULL, "specified db", NULL},
	{"noauth", 0,   arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,     0,    arg_end,   NULL}}, *arg;

    arg = listowned_args;
    arg->value = &user;   arg++;
    arg->value = &cell;   arg++;
    arg->value = &host;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (listowned_args, argc, argv, &optind, ARG_AFSSTYLE)) {
	listowned_usage();
	return 0;
    }

    error = pr_listowned (cell, host, user,
			  arlalib_getauthflag (noauth, 0, 0, 0));
    if (error)
	printf ("pr_listowned failed: %s\n",
		koerr_gettext (error));
    return 0;
}

static int
syncdb_cmd(int argc, char **argv)
{
    printf("sync...not implemented\n");
    return 0;
}

static void
pts_usage(void)
{
    printf("pts - an arla tool for administrating AFS users"
	   " and groups.\n");
    printf("Type \"pts help\" to get a list of commands.\n");
    exit(1);
}


/*
 * SL - switch
 */

static SL_cmd cmds[] = {
    {"adduser",     adduser_cmd,    "add a user to a group"},
    {"chown",       chown_cmd,      "change owner of user/group"},
    {"creategroup", creategroup_cmd,"create a group"},
    {"cg"},
    {"createuser",  createuser_cmd, "create a user"},
    {"dump",        dump_cmd,       "dump pts database"},
    {"delete",      delete_cmd,     "delete entry"},
    {"examine",     examine_cmd,    "examine a user or a group"},
    {"help",        help_cmd,       "get help on pts"},
    {"?"},
    {"listmax",     listmax_cmd,    "print largest uid and gid"},
    {"listowned",   listowned_cmd,      "list groups owned by a user or group, or orphaned groups"},
    {"membership",  member_cmd,     "list group or user membership"},
    {"groups"},
    {"removeuser",  removeuser_cmd, "remove user from group"},
    {"rename",      rename_cmd,     "rename user/group"},
    {"setfields",   setfields_cmd,  "not yet implemented"},
    {"setmax",      empty_cmd,      "not yet implemented"},
    {"syncdb",      syncdb_cmd,     "sync ptsdb with /etc/passwd"},
    {NULL}
};


static int
help_cmd (int argc, char **argv)
{
    sl_help(cmds, argc, argv);
    return 0;
}

int
main(int argc, char **argv)
{
    int ret = 0;
    char **myargv;
    int pos = 0;
    int i;

    ports_init();
    cell_init(0); /* XXX */

    myargv = malloc((argc + 1) * sizeof(char *));
    if (myargv == NULL)
	err(1, "malloc");

    for (i = 0; i < argc; ++i)
	myargv[i] = argv[i];
    myargv[argc] = NULL;

    if(argc > 1) {
	if (strcmp(myargv[1], "-debug") == 0) {
	    prdebug = 1;
	    argc--;
	    myargv[pos+1] = myargv[pos];
	    ++pos;
	}
	ret = sl_command(cmds, argc - 1, &myargv[pos+1]);
	if (ret == -1)
	    printf("%s: Unknown command\n", myargv[pos+1]);
    } else
	pts_usage();

    return ret;
}
