/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include <sl.h>
#include "appl_locl.h"
#include <ka.h>
#include <ka.cs.h>
#include <err.h>

RCSID("$KTH: asrvutil.c,v 1.7 2000/10/03 00:06:52 lha Exp $");

static int help_cmd (int argc, char **argv);

/*
 *  This code is pretty much just stolen out of ksrvutil_get.c
 */

void
safe_write(char *filename, int fd, void *buf, size_t len)
{
    if (write(fd, buf, len) != len) {
	warn("write %s", filename);
	close(fd);
	errx(1, "safe_write: In progress srvtab in this file. %s", filename);
    }
}

#ifndef ANAME_SZ
#define 	ANAME_SZ	40
#define		REALM_SZ	40
#define		SNAME_SZ	40
#define		INST_SZ		40
#endif

void
add_key_to_file (char *file, 
		 kaname *name, 
		 kaname *instance, 
		 int32_t kvno,
		 const EncryptionKey *key)
{
    char sname[ANAME_SZ];		/* name of service */
    char sinst[INST_SZ];		/* instance of service */
    char srealm[REALM_SZ];		/* realm of service */
    char realm[REALM_SZ];		/* realm of service */
    int8_t skvno;
    des_cblock skey;
    int fd;

    strlcpy (realm, cell_getthiscell(), sizeof(realm));

    { 
	char *cp;
	for (cp = realm; *cp; cp++)
	    *cp = toupper(*cp);
    }


    if (file == NULL)
	file = "/etc/srvtab";

    fd = open (file, O_RDWR|O_CREAT, 0600);
    if (fd < 0)
	err (1, "add_key_to_file: open");
    
    lseek(fd, 0, SEEK_SET);
    
    while(getst(fd, sname,  SNAME_SZ) > 0 &&
	  getst(fd, sinst,  INST_SZ) > 0  &&
	  getst(fd, srealm, REALM_SZ) > 0 &&
	  read(fd, &skvno,  sizeof(skvno)) > 0 &&
	  read(fd, skey,    sizeof(skey)) > 0) {

	if(strcmp((char *)name,  sname)  == 0 &&
	   strcmp((char *)instance,  sinst)  == 0 &&
	   strcasecmp(realm, srealm) == 0) {
	    lseek(fd, lseek(fd,0,SEEK_CUR)-(sizeof(skvno) + sizeof(skey)), 
		  SEEK_SET);
	    safe_write(file, fd, &kvno, sizeof(kvno));
	    safe_write(file, fd, (EncryptionKey *)key,   
		       sizeof(EncryptionKey));
	    if (close (fd))
		err (1, "add_key_to_file: close");
	    return;
	}
    }
    safe_write(file, fd, name,  strlen((char *)name) + 1);
    safe_write(file, fd, instance,  strlen((char *)instance) + 1);
    safe_write(file, fd, realm, strlen(realm) + 1);
    safe_write(file, fd, &kvno, sizeof(kvno));
    safe_write(file, fd, (EncryptionKey *)key,   
	       sizeof(EncryptionKey));

    if (close (fd))
	err (1, "add_key_to_file: close");
}
		 
/*
 *
 */

static void
asrvutil_usage(char *progname)
{
    printf("asrvutil - manages srvtabs. An afs replacement for ksrvutil\n");
    printf("Type \"%s help\" to get a list of commands.\n",
	   progname);
    exit(1);
}

/*
 *
 */

void
get_usage(void)
{
    printf("Usage: get  [-cell <cell>] [-noauth] [-help]\n");
}

/*
 *
 */

static int
get_cmd (int argc, char **argv)
{
    int optind = 0;
    int ret;
    struct rx_connection *connkadb = NULL;
    kaname name, instance;
    kaentryinfo entry;
    EncryptionKey key;
    des_cblock deskey;
    int32_t dbhost = 0;
    char *file = NULL;

    char *cell = (char *) cell_getthiscell();
    int noauth = 0;

    static struct agetargs get_args[] = {
	{"name",   0, arg_string,  NULL, "what name to use",
	 NULL},
	{"instance",   0, arg_string,  NULL, "what instance to use",
	 NULL},
	{"cell",   0, arg_string,  NULL, "what cell to use",
	 NULL},
	{"noauth", 0,   arg_flag,    NULL, "don't authenticate", NULL},
	{NULL,     0,    arg_end,   NULL}}, *arg;

    memset (&name, 0, sizeof(name));
    memset (&instance, 0, sizeof(instance));
    
    arg = get_args;
    arg->value = &name;   arg++;
    arg->value = &instance;arg++;
    arg->value = &cell;   arg++;
    arg->value = &noauth; arg++;

    if (getarg (get_args, argc, argv, &optind, AARG_AFSSTYLE)) {
	get_usage();
	return 0;
    }

    memset (&entry, 0, sizeof(entry));
    memset (&key, 0, sizeof(key));

    if (name[0] == '\0') {
	printf ("name: ");
	fflush (stdout);
	if (read(0, name, sizeof(name)-1) < 0) {
	    err(1, "get: read stdin");
	}
	name[sizeof(name)-1] = 0;
    } 

    if (instance[0] == '\0') {
	printf ("instace: ");
	fflush (stdout);
	if (read(0, instance, sizeof(instance)-1) < 0) {
	    err(1, "get: read stdin");
	}
	instance[sizeof(instance)-1] = 0;
    } 

    /* XXX should really try all dbservers */

    ret = arlalib_getsyncsite (cell, NULL, afskaport,
			       &dbhost, 
			       arlalib_getauthflag (noauth, 0, 0, 0));
    if (ret) {
	warnx ("get: arlalib_getsyncsite: %s (%d)", 
	       koerr_gettext(ret), ret);
	return 0;
    }
    if (dbhost == 0)
	errx (1, "arlalib_getsyncsite: returned dbhost == 0, that not valid");

    printf ("dbhost: %d\n", dbhost);

    
    connkadb = arlalib_getconnbyaddr(cell, 
				     dbhost, 
				     NULL,
				     afskaport,
				     KA_MAINTENANCE_SERVICE_ID,
				     arlalib_getauthflag (noauth, 0, 0, 0));

    if (connkadb == NULL)
	errx(1, "Could not connect to kaserver");

    des_new_random_key (&deskey);
    assert (sizeof (key) == sizeof(deskey));
    memcpy (&key, &deskey, sizeof(deskey));
    memset (&key, 0, sizeof(key));

    ret = KAM_GetEntry (connkadb,  name, instance, KAMAJORVERSION, &entry);
    switch (ret) {
    case 0:
	/* Entry does already exist, just get password */
	
	ret = KAM_SetPassword (connkadb, name, instance, 
			       entry.key_version + 1, key);
	if (ret)
	    warnx ("get: KAM_SetPassword returned %s (%d)",
		   koerr_gettext(ret), ret);

	add_key_to_file (file, &name, &instance, entry.key_version + 1, &key);

	break;
    case KANOENT:
	/* Entry doesn't exist, create (and with that set password) */

	ret = KAM_CreateUser (connkadb, name, instance, key);
	if (ret)
	    warnx ("get: KAM_CreateUser returned %s (%d)",
		   koerr_gettext(ret), ret);

	add_key_to_file (file, &name, &instance, 1, &key);

	break;
    default:
	warnx ("get: KAM_GetEntry returned %s (%d)",
	       koerr_gettext(ret), ret);

	ret = 0;
	break;
    }

    memset (&key, 0, sizeof(key));
    arlalib_destroyconn(connkadb);
    return ret;
}

/*
 *
 */

static SL_cmd cmds[] = {
    {"get",         get_cmd,    "get a new cred"},
    {"help",        help_cmd,       "get help on pts"},
    {"?"},
    {NULL}
};

/*
 *
 */

static int
help_cmd (int argc, char **argv)
{
    sl_help(cmds, argc, argv);
    return 0;
}

/*
 *
 */

int
main (int argc, char **argv)
{
    int ret;

    set_progname(argv[0]);

    method = log_open (get_progname(), "/dev/stderr:notime");
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();

    if(argc > 1) {
	ret = sl_command(cmds, argc - 1, argv + 1);
	if (ret == -1)
	    printf("%s: Unknown command\n", argv[1]);
    }
    else
	asrvutil_usage(argv[0]);

    return 0;
}

