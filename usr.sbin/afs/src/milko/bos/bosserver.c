/*
 * Copyright (c) 1999, 2000 Kungliga Tekniska Högskolan
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

#include "bos_locl.h"

RCSID("$arla: bosserver.c,v 1.23 2002/04/26 16:11:42 lha Exp $");

static char *email = "root";
static char *serverfile = MILKO_SYSCONFDIR "/server-file";


typedef struct mtype {
    char *name;				/* context name */
    char *program;			/* program name */
    char *path;				/* path to program */
    char *arg;				/* string of arguments */
    char *coredir;			/* where the corefile is */
    pid_t pid;				/* pid of server */
    time_t last_start;			/* last started */
    struct {
	unsigned email:1;		/* email when event happens */
	unsigned savecore:1;	/* try to save the corefile */
	unsigned trydebug:1;	/* try to include debugging */
	unsigned restart_p:1;	/* needs to be restarted */
	unsigned enablep:1;	/* is enabled */
    } flags;
    struct mtype *next;			/* next server on linked list */
} mtype;

struct mtype *servers = NULL;

static int debug = 1;
static char *bosserverprefix = NULL;

/*
 *
 */

void
bosdebug (char *fmt, ...)
{
    va_list args;
    if (!debug)
	return ;

    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end(args);
}

/*
 * Send mail to irresponsible person
 */

static void
sendmail (char *person, char *name, char **message)
{
    FILE *f;
    char *cmd;
    char hostname[400];
    int i;

    if (person == NULL) {
	bosdebug ("sendmail: person == NULL");
	return;
    }
    
    hostname[0] = '\0';
    gethostname (hostname, sizeof(hostname));

    asprintf (&cmd, "/usr/sbin/sendmail %s", person);

    f = popen (cmd, "w");
    if (f == NULL) {
	bosdebug ("sendmail: %s", cmd);
	free (cmd);
	return;
    }

    fprintf (f, "From: <bos-watcher@%s>\n", hostname);
    fprintf (f, "Subject: Death %s\n\n", name);

    i = 0;
    while (message[i]) {
	fprintf (f, "%s\n", message[i]);
	i++;
    }
    
    fflush (f);
    pclose (f);

    free (cmd);
}

/*
 * find server named `name' in linked list `old_srv'. If found
 * removed the found server from the list and return it.
 * If `name' isn't found, return NULL.
 */

static struct mtype *
find_and_remove_server (struct mtype **old_srv, const char *name)
{
    struct mtype *srv = *old_srv;

    while (srv) {
	if (srv->name && strcasecmp (name, srv->name) == 0) {
	    *old_srv = srv->next;
	    return srv;
	}
	old_srv = &srv->next;
	srv = srv->next;
    }
    return NULL;
}

/*
 * find server named `name' in linked list `old_srv'.
 * If `name' isn't found, return NULL.
 */

static struct mtype *
find_server (struct mtype **old_srv, const char *name)
{
    struct mtype *srv = *old_srv;

    while (srv) {
	if (srv->name && strcasecmp (name, srv->name) == 0) {
	    return srv;
	}
	srv = srv->next;
    }
    return NULL;
}


/*
 *
 */

static void
kill_server (struct mtype *s)
{
    if (s->pid)
	kill (s->pid, SIGTERM);
}


/*
 *
 */

static void
shutdown_servers (struct mtype *old_server)
{
    while (old_server) {
	struct mtype *s = old_server;
	old_server = s->next;
	kill_server (s);
	if (s->arg)
	    free (s->arg);
	free (s);
    }
}

/*
 *
 */

static struct mtype *
new_server (char *name)
{
    struct mtype *s;

    s = emalloc (sizeof(*s));
    memset (s, 0, sizeof(*s));
    s->name = estrdup (name);
    return s;
}

/*
 *
 */

static void
write_serverfile (struct mtype *s, const char *fn)
{
    FILE *f;

    f = fopen (fn, "w");
    if (f == NULL) {
	errx (1, "failed to open serverfile (%s) for writing",
	      serverfile);
    }
    while (s) {
	fprintf (f, "%s %d\n", 
		 s->name,
		 s->flags.enablep);
	s = s->next;
    }
    fclose (f);
}	

/*
 *
 */

static void
read_config_file (char *filename)
{
    kconf_context context;
    kconf_config_section *conf, *c;
    struct mtype *old_servers;
    struct mtype *s;
    const char *str;
    FILE *f;
    int ret;

    kconf_init (&context);

    ret = kconf_config_parse_file (filename, &conf);
    if (ret) {
	shutdown_servers (servers);
	errx (1, "read_config_file");
    }
    
    email = 
	estrdup (kconf_config_get_string_default (context, conf,
						  email,
						  "bos",
						  "email",
						  NULL));
    
    serverfile =
	estrdup (kconf_config_get_string_default (context, conf,
						  serverfile,
						  "bos",
						  "serverfile",
						  NULL));

    /*
     * Save the old list of servers
     */

    old_servers = servers;
    servers = NULL;
    c = conf;

    while (c) {
	char *name;
	
	if (!strcmp (c->name, "bos")) {
	    c = c->next;
	    continue;
	}
	
	name = c->name;

	if (name == NULL) {
	    shutdown_servers (servers);
	    shutdown_servers (old_servers);
	    errx (1, "error in config file (%s)", filename);
	}

	str = kconf_config_get_string (context, conf,
				       name,
				       "arguments",
				       NULL);
	
	s = find_and_remove_server (&old_servers, name);
	if (s) {
	    if (str && s->arg && strcmp (str, s->arg)) {
		s->flags.restart_p = 1;
		free (s->arg);
		s->arg = estrdup (str);
	    }
	    
	} else {
	    s = new_server (name);
	    if (str)
		s->arg = estrdup (str);

	    s->flags.enablep = 1;
	    s->flags.restart_p = 1;
	}
	
	s->flags.email = kconf_config_get_bool_default (context, conf,
							KCONF_FALSE,
							name,
							"email",
							NULL);
	s->program = 
	    estrdup (kconf_config_get_string_default (context, conf,
						      name,
						      name,
						      "program",
						      NULL));

	s->coredir =
	    estrdup (kconf_config_get_string_default (context, conf,
						      MILKO_LIBEXECDIR,
						      name,
						      "coredir",
						      NULL));
	s->flags.savecore =
	    kconf_config_get_bool_default (context, conf,
					   KCONF_FALSE,
					   name,
					   "savecore",
					   NULL);


	s->flags.trydebug =
	    kconf_config_get_bool_default (context, conf,
					   KCONF_FALSE,
					   name,
					   "trydebug",
					   NULL);

	s->next = servers;
	servers = s;
	c = c->next;
    }
    
    f = fopen (serverfile, "r");
    if (f == NULL) {
	shutdown_servers (old_servers);
/*	err (1, "tried to open serverfile \"%s\"", serverfile);*/
    } else {
	int lineno = 0;
	while (!feof (f)) {
	    char name[100];
	    int enablep;
	    lineno ++;
	    ret = fscanf (f, "%99s %d", name, &enablep);
	    if (ret == EOF)
		break;
	    if (ret != 2) {
		errx (1, "error scaning line %d of serverfile (%s)",
		      lineno, serverfile);
	    }

	    s = find_server (&servers, name);
	    if (s && s->flags.enablep != enablep) {
		s->flags.enablep = enablep;
		s->flags.restart_p = enablep;
	    }
	}
	fclose (f);
    }


    /*
     * nuke the old ones and write down a new serverfile
     */

    shutdown_servers (old_servers);
    write_serverfile (servers, serverfile);
}

/*
 * start the server `server'
 */

static int
start_server (struct mtype *server)
{
    int i;
    time_t newtime = time(NULL);

    bosdebug ("starting server %s (%s)\n", server->name, 
	      server->program);

    if (newtime + 10 < server->last_start 
	&& server->last_start != 0) {
	/* XXX */
	bosdebug ("server %s looping, will try again in 10 min",
		  server->name);
	return 0;
    }
    server->last_start = newtime;
    
    i = fork();
    switch (i) {
    case 0: {
	char *newargv[3];

	newargv[0] = server->program;
#if 1
	newargv[1] = NULL;	
#else
	XXX insert argv parser here
#endif

	if (debug == 0) {
	    close (0);
	    close (1);
	    close (2);
	}

	execvp (server->program, newargv);

	bosdebug ("server %s failed with %s (%d)\n", server->program,
		  strerror (errno), errno);
	exit (1);
	break;
    }
    case -1:
	bosdebug ("failed to fork with server %s with reson %s (%d)\n",
		  server->name, strerror (errno), errno);
	server->pid = 0;
	return errno;
	break;
    default:
	server->pid = i;
	bosdebug ("started server %s with pid %d\n", 
		  server->name, server->pid);
	break;
    }
    
    return 0;
}


/*
 * GC processes, and start new if appropriate.
 */

static int dead_children = 0;

static PROCESS riperprocess;

static void
theriper (char *no_arg)
{
    int status;
    char *msg[3] = { NULL, NULL, NULL };
    struct mtype *server = NULL;
    pid_t pid;

    while (1) {
	if (dead_children == 0) {
	    bosdebug ("theriper: sleeping\n");
	    LWP_WaitProcess ((char *)theriper);
	}	

	bosdebug ("theriper: running\n");

	if (dead_children == 0) {
	    bosdebug ("theriper: called without a dead child\n");
	    continue;
	}
	
	if (dead_children < 0) {
	    bosdebug ("theriper: called with negative dead child\n");
	    exit(-1);
	}

	dead_children--;
	    
	pid = wait (&status);

	if (WIFEXITED(status)) {
	    asprintf (&msg[1],
		      "normal exit: exit error %d", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
	    asprintf (&msg[1],
		      "terminated by signal num %d %s", WTERMSIG(status),
		      WCOREDUMP(status) ? "coredumped\n" : "");
	} else if (WIFSTOPPED(status)) {
	    asprintf (&msg[1],
		      "process stoped by signal %d", WSTOPSIG(status));
	} else {
	    asprintf (&msg[1],
		      "by unknown reason");
	}
	bosdebug (msg[1]);

	server = servers;
	while (server) {
	    if (server->pid == pid) {
		msg[0] = server->name;
		if (server->flags.email)
		    sendmail (email, server->name, msg);
		bosdebug ("%s was the killed, starting new", server->name);
		start_server (server);
		break;
	    }
	    server = server->next;
	}
	free (msg[1]);
    }
}

/*
 * Signal-handler for SIGCHLD
 */

static void
sigchild (int foo)
{
    bosdebug ("child died\n");

    dead_children++;

    signal (SIGCHLD, sigchild);
    LWP_NoYieldSignal ((char *) theriper);
}

/*
 * bootstrap bosserver
 * Create riper thread and start all servers.
 */

static int
bosserver_init (void)
{
    struct mtype *server;
    int i;

    i = LWP_CreateProcess (theriper, 0, 1, NULL, "the-riper", &riperprocess);
    if (i)
	errx (1, "LWP_CreateProcess returned %d", i);

    server = servers;
    while (server) {
	if (server->flags.enablep) {
	    i = start_server(server);
	    if (i)
		bosdebug ("server_server: returned %s (%d)",
			  strerror (i), i);
	}
	server = server->next;
    }
    
    return 0;
}


/*
 *
 */

static struct rx_service *bosservice = NULL;

static char *cell = NULL;
static char *realm = NULL;
static int do_help = 0;
static char *srvtab_file = NULL;
static int no_auth = 0;
static char *configfile = MILKO_SYSCONFDIR "/bos.conf";
static char *log_file = "syslog";

static struct agetargs args[] = {
    {"cell",	0, aarg_string,    &cell, "what cell to use", NULL},
    {"realm",  0, aarg_string,    &realm, "what realm to use"},
    {"prefix",'p', aarg_string,    &bosserverprefix, 
     "directory where servers is stored", NULL},
    {"noauth",   0,  aarg_flag,	  &no_auth, "disable authentication checks"},
    {"debug", 'd', aarg_flag,      &debug, "output debugging"},
    {"log", 'l', aarg_string,      &log_file, "where output debugging"},
    {"help",  'h', aarg_flag,      &do_help, "help"},
    {"srvtab",'s', aarg_string,    &srvtab_file, "what srvtab to use"},
    {"configfile", 'f', aarg_string, &configfile, "configuration file"},
    { NULL, 0, aarg_end, NULL }
};

static void
usage(int exit_code)
{
    aarg_printusage(args, NULL, "", AARG_AFSSTYLE);
    exit (exit_code);
}

/*
 *
 */

int
main (int argc, char **argv)
{
    Log_method *method;
    int optind = 0;
    int ret;
    
    setprogname(argv[0]);

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	usage (1);
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 1;
    }
    
    if (do_help)
	usage(0);

    if (bosserverprefix == NULL)
	bosserverprefix = MILKO_LIBEXECDIR;

    method = log_open (getprogname(), log_file);
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();
    
    read_config_file (configfile);

    if (no_auth)
	sec_disable_superuser_check ();

    if (cell)
	cell_setthiscell (cell);

    network_kerberos_init (srvtab_file);

    signal (SIGCHLD, sigchild);		/* XXX sigaction */

    ret = network_init(htons(afsbosport), "bos", BOS_SERVICE_ID, 
		       BOZO_ExecuteRequest, &bosservice, realm);
    if (ret)
	errx (1, "network_init failed with %d", ret);
    
    ret = bosserver_init();
    if (ret)
	errx(1, "bosserver_init: error %d", ret);

    printf("Milko bosserver %s-%s started\n", PACKAGE, VERSION);

    rx_SetMaxProcs(bosservice,5) ;
    rx_StartServer(1) ;

    /* NOTREACHED */
    return -1;
}
