/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
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

/* 
   klog.c - a version of 'klog' for Arla

   Written by Chris Wing - wingc@engin.umich.edu
   based on examples of AFS code: pts.c (Arla) and kauth.c (KTH-KRB)

   Hacked to use agetarg by Love Hörnquist-Åstrand <lha@stacken.kth.se>

   Hacked to use agetarg and still work properly by Chris Wing

   This is a reimplementation of klog from AFS. The following new features
   have been added:
	-timeout	Number of seconds to wait for Kerberos operations
			to finish. By default, we will wait forever.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$arla: klog.c,v 1.38 2003/06/04 11:46:31 hin Exp $");

#include "appl_locl.h"
#include "klog.h"
#include "kafs.h"

/*
 * State variables
 */

/* Lifetime in seconds. Default token lifetime is 720 hours (30 days) */
static int lifetime = 720 * 60 * 60;

/* 
 * arguments passed to program 
 */

static char *arg_principal = NULL;
static char *arg_cell = NULL;
static char *arg_realm = NULL;
static char *arg_password = NULL;
static agetarg_strings arg_servers;
static char *arg_lifetime = NULL;
static int  arg_timeout = 0;
static int  arg_pipe = 0;
static int  arg_setpag = 0;
static int  arg_silent = 0;
static int  arg_getkrbtgt = 0;
static int  arg_version = 0;
static int  arg_help = 0;
static char *arg_kname = NULL;

/* AFS ID */
static int afsid = 0;

/* Password */
static char *password = NULL;

/* Did the attempted network command time out? */
static int timed_out = 0;

#ifdef KERBEROS

/* Do we have a ticket file lying around we'd like to get rid of? */
static int have_useless_ticket_file = 0;

/* Kerberos ticket file */
static char *tkfile = NULL;

#endif

/* Did we get a token that will sit around unused if not destroyed? */
static int have_useless_token = 0;

/*
 * Erase passwords from memory and remove tokens and tickets that
 * shouldn't be left laying around.
 */

static void
final_cleanup (void)
{
    /* Make sure to clear out the password in this process's memory image
       before exiting */
    if(password)
	memset(password, 0, PASSWD_MAX);

    /* Destroy any useless tokens that we acquired */
    if(have_useless_token)
	k_unlog();

#ifdef KERBEROS
    /* Destroy any temporary ticket files lying around. */
    if(have_useless_ticket_file)
	dest_tkt();
#endif
}

/* Death function that erases password before exiting */

void
die (int retcode)
{
    final_cleanup ();

    exit(retcode);
}

/* Die, print out an error message, and interpret errno
   Remember that *err*() from roken prints out a trailing newline. */

void
diet (int retcode, char *fmt, ...)
{
    va_list ap;

    final_cleanup ();

    va_start(ap, fmt);
    verr (retcode, fmt, ap);
    va_end(ap);
}

/* Die and print out an error message. Do not interpret errno. */

void
dietx (int retcode, char *fmt, ...)
{
    va_list ap;

    final_cleanup ();

    va_start(ap, fmt);
    verrx (retcode, fmt, ap);
    va_end(ap);
}


/* 
 * Figure out the AFS ID of a user name 
 */

int
get_afs_id(void)
{
    int32_t returned_id;
    const char **servers = (const char **)arg_servers.strings; /* ARGH */

    arlalib_get_token_id_servers (arg_principal,
				  arg_cell,
				  arg_servers.num_strings,
				  servers,
				  &returned_id);
    return returned_id;
}

#ifndef KRB_TICKET_GRANTING_TICKET
#define KRB_TICKET_GRANTING_TICKET "krbtgt"
#endif

/* Get a Kerberos 4 TGT */

int
get_k4_ticket(void)
{
	int rc;

	int krb_lifetime;

	/* The minimum lifetime is 5 minutes */
	if(lifetime < 5 * 60)
	    krb_lifetime = 1;
#ifdef KERBEROS
	else
	    krb_lifetime = krb_time_to_life(0, lifetime);
#endif

	/* If a ridiculously high lifetime is given to krb_time_to_life,
	   0 may be returned as a result... */
	if( (krb_lifetime > 255) || (krb_lifetime == 0) )
	    krb_lifetime = 255;

#ifdef KERBEROS
	rc = krb_get_pw_in_tkt(arg_principal, "", arg_realm, 
			       KRB_TICKET_GRANTING_TICKET, arg_realm,
			       krb_lifetime, password);
	if(rc)
	    warnx("Unable to authenticate to Kerberos: %s",
		  krb_get_err_text(rc));
#else
	warnx ("No kerberos included");
	return (1);
#endif /* KERBEROS */

	return(rc);
}


/* Get an AFS token */

int get_afs_token(void)
{
	int rc;

	/* FIXME: This will happily store a token for one cell with the
		  name of another cell, and this makes Arla
		  misbehave :( */

#ifdef KERBEROS
#ifdef HAVE_KRB_AFSLOG_UID
	rc = krb_afslog_uid(arg_cell, arg_realm, afsid);
#else
	rc = k_afsklog_uid(arg_cell, arg_realm, afsid);
#endif
	if(rc)
	    warnx("Unable to get an AFS token: %s", krb_get_err_text(rc));
#else
	warnx ("Unable to get an AFS token since there is no kerberos");
	return (1);
#endif

	return(rc);
}


/* 
 * Generalized machinery for performing a timeout on an arbitrary
 * function returning an integer. Fun, eh?
 */

int
do_timeout (int (*function)(void) )
{
    int pipearr[2];
    int reader, writer;
    pid_t pid;
    int status;
    
    timed_out = 0;
    
    /* Don't bother with all this jibba jabba if we don't want to timeout */
    if(arg_timeout == 0)
	return (function());

    if(pipe(pipearr) == -1)
	diet (1, "do_timeout(): Can't make a pipe");

    reader = pipearr[0];
    writer = pipearr[1];

    /* :) */
    fflush(stdout);
    fflush(stderr);

    pid = fork();

    if(pid == -1)
	diet (1, "do_timeout: Can't fork");

    if(pid) {
	/* parent's thread of execution */

	fd_set readfds;
	struct timeval tv;
	int retval;
	int result;
	ssize_t len;

	close(writer);

	if (reader >= FD_SETSIZE)
	    diet (1, "do_timeout(): fd too large");

	/* this is how you set up a select call */
	FD_ZERO(&readfds);
	FD_SET(reader, &readfds);

	/* Wait as many as timeout seconds. */
	tv.tv_sec = arg_timeout;
	tv.tv_usec = 0;

	retval = select(reader+1, &readfds, NULL, NULL, &tv);

	/* Kill the child process in any case */
	kill(pid, SIGKILL);

	if(!retval) {
	    timed_out = 1;
	} else {
	    /* okay, what happened */

	    len = read(reader, &result, sizeof(int));
	}

	/* no matter what, we must wait on the child or else we will
	   accumulate zombies */
	waitpid(pid, &status, 0);

	/* close the other end of the pipe */
	close(reader);

	if(timed_out)
	    return(1);
	else
	    return(result);

	/* PARENT ENDS HERE */

    } else {
	/* child's thread of execution */
	int retval;
	ssize_t len;

	close(reader);

	retval = function();

	len = write(writer, &retval, sizeof(int));

	/* Destroy the copy of the password in the child's memory image */
	memset(password, 0, PASSWD_MAX);

	exit(0);

	/* CHILD ENDS HERE */
    }
}

/*
 * randfilename()
 * Return a "random" file name, for use, e.g., as a ticket file
 * use umich compat basename of ticket.
 */

#ifndef TKT_ROOT
#define TKT_ROOT "/tmp"
#endif

char *
randfilename(void)
{
    const char *base;
    char *filename;
    int fd, i;

    /* 
     * this kind of sucks, before we use an array but in some kerberos
     * dists TKT_ROOT isn't a constant (its a function) so we needed
     * to stop using that.
     */

    for (i = 0; i < 3; i++) {
	base = NULL;
	switch (i) {
#ifdef HAVE_KRB_GET_DEFAULT_TKT_ROOT
	case 0:
	    base = krb_get_default_tkt_root ();
	    break;
#endif
	case 1:
	    base = KLOG_TKT_ROOT;
	    break;
	case 2:
	    base = TKT_ROOT;
	    break;
	default:
	    abort();
	}

	if (base == NULL)
	    continue;

	asprintf (&filename, "%s_%u_XXXXXX", base, (unsigned)getuid());
	if (filename == NULL)
	    dietx (1, "out of memory");

	fd = mkstemp(filename);
	if (fd >= 0) {
	    close(fd);
	    return filename;
	}
	free (filename);
    }
    dietx (1, "could not create ticket file");
}

struct agetargs args[] = {
    { "principal", 0, aarg_string, &arg_principal,
      "principal to obtain token for",
      "user name", aarg_optional},
    { "password", 0, aarg_string, &arg_password,
      "password to use (NOT RECOMMENDED TO USE)",
      "AFS password", aarg_optional},
    { "servers", 0, aarg_strings, &arg_servers,
      "list of servers to contact",
      "AFS dbservers", aarg_optional},
    { "lifetime", 0, aarg_string, &arg_lifetime,
      "lifetime given in hh[:mm[:ss]]",
      "hh:mm:ss", aarg_optional},
    { "pipe", 0, aarg_flag, &arg_pipe,
      "read password from stdin and close stdout", 
      NULL, aarg_optional},
    { "timeout", 0, aarg_integer, &arg_timeout,
      "network timeout given in seconds (default is forever)", 
      "seconds", aarg_optional},
    { "setpag", 0, aarg_flag, &arg_setpag,
      "store token in new PAG and spawn a shell", 
      NULL, aarg_optional},
    { "silent", 0, aarg_flag, &arg_silent,
      "close stderr", 
      NULL, aarg_optional},
    { "tmp", 0, aarg_flag, &arg_getkrbtgt,
      "get a Kerberos TGT (possibly overwriting any current one)", 
      NULL, aarg_optional},
    { "cell", 0, aarg_string, &arg_cell,
      "cell where to obtain token", 
      "cell name", aarg_optional},
    { "realm", 0, aarg_string, &arg_realm,
      "Kerberos realm to get TGT in (default same as AFS cell)",
      "Kerberos realm", aarg_optional},
    { "help", 0, aarg_flag, &arg_help, "help",
      NULL, aarg_optional},
    { "version", 0, aarg_flag, &arg_version, "print version",
      NULL, aarg_optional},
    { NULL, 0, aarg_generic_string, &arg_kname, "Kerberos identity",
      "user@cell", aarg_optional},
    { NULL, 0, aarg_end, NULL, NULL }
};

/*
 *
 */

static void
do_help (int exitval)
{
    aarg_printusage(args, NULL, NULL, AARG_AFSSTYLE);
    exit(exitval);
}

/*
 * If we dont have kerberos support, bail out.
 */

#ifndef KERBEROS

int
main(int argc, char **argv)
{
    errx (1, "Kerberos support isn't compiled in");
    return 1;
}
#else

/*
 * The core of this evil
 */

int
main(int argc, char **argv)
{
    char prompt[PW_PROMPT_MAX];
    int rc;
    int optind = 0;
    Log_method *method;

    char pwbuf[PASSWD_MAX];

    set_progname (argv[0]);

    method = log_open (getprogname(), "/dev/stderr:notime");
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();

     rc = agetarg(args, argc, argv, &optind, AARG_AFSSTYLE);
     if(rc) {
	 warnx ("Bad argument: %s", argv[optind]);
	 do_help(1);
     }

    if (arg_help)
	do_help(0);

    if (arg_version)
	dietx (0, "part of %s-%s", PACKAGE, VERSION);

    if (arg_password)
	warnx ("WARNING: The use of -password is STRONGLY DISCOURAGED");

    /* manually parse a string of the form user@cell, we used to use
       kname_parse(), but this no longer works properly as of krb4-1.0 */
    if (arg_kname) {
        char *at = strchr(arg_kname, '@');
        char *tmp_principal = arg_kname;
        char *tmp_cell;
	
        if(at) {
            *at = '\0';
	    
            tmp_principal = arg_kname;
            at++;
            tmp_cell = at;
	    
            if(*tmp_cell != '\0')
                arg_cell = tmp_cell;
        }
	
        if(*tmp_principal != '\0')
            arg_principal = tmp_principal;
    }
    
    if (arg_lifetime) {
	int h = 0, m = 0, s = 0;
	int matched;
	
	matched = sscanf(arg_lifetime, "%u:%u:%u", &h, &m, &s);
	
	if(matched < 1 || matched > 3)
	    dietx (1, "Bad argument for -lifetime: %s", arg_lifetime);
	
	lifetime = h * 3600 + m * 60 + s;
    }

    /* Simplest way to prevent any output from this command */
    if(arg_pipe)
	freopen("/dev/null", "r+", stdout);

    /* Simplest way to prevent any output from this command */
    if(arg_silent)
	freopen("/dev/null", "r+", stderr);

    if(!k_hasafs())
	dietx (1, "Hmm, your machine doesn't seem to have kernel support "
		  "for AFS");

    /* Try to get a new PAG, but don't abort if we can't */
    if(arg_setpag) {
	if (k_setpag() == -1)
	    warnx ("Couldn't get new PAG");
    }

    /* Figure out the AFS cell to use */
    if (arg_cell == NULL) {
	arg_cell = (char *)cell_getthiscell();
	
	if (arg_cell == NULL)
	    dietx (1, "Can't find local cell!");
    }

    /* FIXME: Figure out a way to automatically deal with setups where the
	      Kerberos realm is not just an uppercase version of the
	      AFS cell

              if libkafs exported kafs_realm_of_cell(), we could use that.
	      
              libkafs now does, but that isn't enough. kafs_realm_of_cell
              is currently implemented by looking up the first entry in
              CellServDB for the given cell, doing an inverse DNS lookup,
              and then using the domain name of the resulting host name as
              the Kerberos realm (subject to remapping via krb.realms)
	      
              This is bad, because (a) it requires a resolver call, which
              can hang for an arbitrary amount of time, and (b) it
              requires playing silly games with krb.realms and hoping that
	      the inverse DNS entry for the first server for a cell will  
              not change.
 
              libkafs should recognize a file called "CellRealms", or
              something like that, which yields a correct mapping between
              a given AFS cell and a Kerberos realm; otherwise it should
              just uppercase the cell name and be done with it.
 
              For now, give the user the option to specify a specific
              realm to use via -realm. */

    if(arg_realm == NULL) {
	char *p;

	arg_realm = estrdup(arg_cell);

	/* convert name to upper case */
	p = arg_realm;
	while(*p != '\0') {
	    *p = toupper(*p);
	    p++;
	}
    }

    /* Figure out the Kerberos principal we are going to use */
    if (arg_principal == NULL) {
        struct passwd *pwd = getpwuid (getuid ());
	
        if (pwd == NULL)
            dietx (1, "Could not get default principal");
	
        arg_principal = pwd->pw_name;
    }
    
    /* Figure out the db server we are going to contact */
    if (arg_servers.num_strings == 0) {
	arg_servers.strings = emalloc (sizeof(char *));
	arg_servers.strings[0] = (char *)cell_findnamedbbyname (arg_cell);

	if(arg_servers.strings[0] == NULL)
	    dietx (1, "Can't find any db server for cell %s", arg_cell);

	arg_servers.num_strings = 1;
    }

    /* 
     * Get the password 
     */

    if(arg_password == NULL) {
	password = pwbuf;
	
	if(arg_pipe) {
	    if (fgets (pwbuf, sizeof(pwbuf), stdin) == NULL)
		dietx (1, "EOF reading password");
	    pwbuf[strcspn(pwbuf, "\n")] = '\0';
	} else {
	    snprintf(prompt, PW_PROMPT_MAX, 
		     "%s@%s's Password:", arg_principal, arg_cell);
	    
	    if (des_read_pw_string(password, PW_PROMPT_MAX-1, prompt, 0))
		dietx (1, "Unable to login because can't read password "
			  "from terminal.");
	    
	}
    } else {
	/* Truncate user-specified password to PASSWD_MAX */
	/* This also lets us use memset() to clear it later */
	
	strlcpy(pwbuf, arg_password, PASSWD_MAX);
	
	password = pwbuf;
	
        {
            /* Have to clear out this copy of the password too from the
               memory image */
	    
            /*
	     * FIXME: we should also erase the copy in argv[].
	     * Is it safe to overwrite bits of argv, and is there
	     * any nice way to do this from the agetarg()
	     * framework?
	     */
            int pwlen;
            pwlen=strlen(arg_password);
            memset(arg_password, 0, pwlen);
        }
    }
    
    /* A familiar AFS warning message */
    if (password == NULL || *password == '\0')
	dietx (1, "Unable to login because zero length password is illegal.");
    
    /* 
     * Create a secure random ticket file if we are running with -setpag,
     * because we can set the environment variable in the child shell.
     */

    if(arg_setpag) {
	tkfile = randfilename();
	have_useless_ticket_file = 1;
    } else {
	if (arg_getkrbtgt) {
	    tkfile = getenv("KRBTKFILE");
	    if(tkfile == NULL) {
		/* the insecure default ticket file :( */
		tkfile = TKT_FILE;
	    }
	} else {
	    /* Create a unique temporary file to get the temporary TGT */
	    tkfile = randfilename();
	    have_useless_ticket_file = 1;
	}
    }

    setenv ("KRBTKFILE", tkfile, 1);
    krb_set_tkt_string (tkfile);

    /* Get the Kerberos TGT */
    rc = do_timeout (get_k4_ticket);
    if(timed_out)
	dietx (1, "Timed out trying to get Kerberos ticket for %s@%s", 
	       arg_principal, arg_realm);

    /* get_k4_ticket() will already have printed out an error message. */
    if(rc)
	die(rc);

    /* We can now clear the password from the memory image */
    memset(password, 0, PASSWD_MAX);

    /* Only keep this ticket file around if we were invoked as -tmp; this way,
       the user will still be able to get a Kerberos TGT even if he/she cannot
       obtain a token */
    if(arg_getkrbtgt)
	have_useless_ticket_file = 0;
    else
	have_useless_ticket_file = 1;

    /* 
     * Figure out the AFS ID to store with the token.
     * Moved this after the TGT-gathering stage because it may want to
     * get a ticket to talk to the dbserver?
     */

    afsid = do_timeout (get_afs_id);
    if(timed_out)
	warnx("Timed out trying to get AFS ID for %s@%s", 
	      arg_principal, arg_cell);

    /* Get the AFS token */
    rc = do_timeout (get_afs_token);
    if(timed_out)
	dietx (1, "Timed out trying to get AFS token for %s@%s", 
	       arg_principal, arg_realm);

    /* get_afs_token() will already have printed out an error message. */
    if(rc)
	die(rc);

    /* Destroy the temporary Kerberos TGT if we didn't need it */
    if(!arg_getkrbtgt) {
	dest_tkt();

	/* Avoid calling dest_tkt() twice-- may be security risk */
	have_useless_ticket_file = 0;
    }

    /* 
     * Exec a shell if the user specified -setpag, because otherwise the
     *  new PAG will magically disappear when the program exits 
     */

    if(arg_setpag) {
	/* 
	 * Get the default shell of the PRINCIPAL we are kloging as.
	 * This is the only thing that makes sense, as we are going to
	 * be opening up a session for this principal.
	 * Perhaps we should also change to this principal's home
	 * directory, as the shell will fail if the command was run in
	 * a restricted directory owned by a different AFS principal
	 */

	struct passwd *pwd;
	char *shell;

	pwd = getpwnam(arg_principal);
	if(pwd == NULL) {
	    pwd = getpwuid(getuid());
	    if(pwd == NULL) {

		shell = getenv("SHELL");
		if (shell == NULL) {
		    warnx ("Can't get default shell for user %s, using "
			   "fallback (/bin/sh) shell.", arg_principal);
		    shell = "/bin/sh";
		}

	    } else {
		warnx ("Can't get default shell for user %s, "
		       "using default shell for UID %d", 
		       arg_principal, getuid());

		shell = pwd->pw_shell;
	    }
	} else {
	    shell = pwd->pw_shell;
	}

	execl(shell, shell, NULL);

	/* the AFS token is useless if the shell exec fails, because it
	   is in a PAG that will soon go away. */
	have_useless_token = 1;

	diet (1, "Can't exec shell: %s", shell);
    }

    return 0;
}
#endif /* KERBEROS */
