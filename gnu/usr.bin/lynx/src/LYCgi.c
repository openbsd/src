/*                   Lynx CGI support                              LYCgi.c
**                   ================
**
** Authors
**          GL      George Lindholm <George.Lindholm@ubc.ca>
**
** History
**      15 Jun 95   Created as way to provide a lynx based service with
**                  dynamic pages without the need for a http daemon. GL
**      27 Jun 95   Added <index> (command line) support. Various cleanup
**                  and bug fixes. GL
**	04 Sep 97   Added support for PATH_INFO scripts.  JKT
**
** Bugs
**      If the called scripts aborts before sending the mime headers then
**      lynx hangs.
**
**      Should do something about SIGPIPE, (but then it should never happen)
**
**      No support for redirection. Or mime-types.
**
**      Should try and parse for a HTTP 1.1 header in case we are "calling" a
**      nph- script.
*/ 

#include "HTUtils.h"
#include "tcp.h"
#include "HTTP.h"
#include "HTParse.h"
#include "HTTCP.h"
#include "HTFormat.h"
#include "HTFile.h"
#include "HTAlert.h"
#include "HTMIME.h"
#include "HTAABrow.h"

#include "LYGlobalDefs.h"
#include "LYUtils.h"
#include "HTML.h"
#include "HTInit.h"
#include "LYGetFile.h"
#include "LYBookmark.h"
#include "GridText.h"
#include <ctype.h>
#include "LYCgi.h"
#include "LYSignal.h"
#include "LYLocal.h"

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

struct _HTStream 
{
  HTStreamClass * isa;
};

PRIVATE char **env = NULL;  /* Environment variables */
PRIVATE int envc_size = 0;  /* Slots in environment array */
PRIVATE int envc = 0;	    /* Slots used so far */
#ifdef LYNXCGI_LINKS
PRIVATE char user_agent[64];
PRIVATE char server_software[64];
#endif /* LYNXCGI_LINKS */

PRIVATE void add_environment_value PARAMS((char *env_value));


/*
 * Simple routine for expanding the environment array and adding a value to
 * it
 */
PRIVATE void add_environment_value ARGS1(
	char *,	env_value)
{
    if (envc == envc_size) {   /* Need some more slots */
	envc_size += 10;
	if (env)
	    env = (char **)realloc(env,
				   sizeof(env[0]) * (envc_size + 2));
						/* + terminator and base 0 */
	else
	    env = (char **)malloc(sizeof(env[0]) * (envc_size + 2));
						/* + terminator and base 0 */
	if (env == NULL) {
	    outofmem(__FILE__, "LYCgi");
	}
    }

    env[envc++] = env_value;
    env[envc] = NULL;      /* Make sure it is always properly terminated */
}
    
/*
 * Add the value of an existing environment variable to those passed on to the
 * lynxcgi script.
 */
PUBLIC void add_lynxcgi_environment ARGS1(
	CONST char *,	variable_name)
{
    char *env_value;

    env_value = getenv(variable_name);
    if (env_value != NULL) {
	char *add_value = NULL;

	add_value = (char *)malloc(strlen(variable_name) +
				   strlen(env_value) + 2);
	if (add_value == NULL) {
	    outofmem(__FILE__, "LYCgi");
	}
	strcpy(add_value, variable_name);
	strcat(add_value, "=");
	strcat(add_value, env_value);
	add_environment_value(add_value);
    }
}

PRIVATE int LYLoadCGI ARGS4(
	CONST char *, 		arg,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    int status;
#ifdef LYNXCGI_LINKS
#ifndef VMS
    char *cp;
    struct stat stat_buf;
    char *pgm = NULL;		        /* executable */
    char *pgm_args = NULL;	        /* and its argument(s) */
    int statrv;
    char *orig_pgm = NULL;		/* Path up to ? as given, URL-escaped*/
    char *document_root = NULL;		/* Corrected value of DOCUMENT_ROOT  */
    char *path_info = NULL;             /* PATH_INFO extracted from pgm      */
    char *pgm_buff = NULL;		/* PATH_INFO extraction buffer       */
    char *path_translated;		/* From document_root/path_info      */

    if (!arg || !*arg || strlen(arg) <= 8) {
	HTAlert(BAD_REQUEST);
	status = -2;
	return(status);

    } else {
	if (strncmp(arg, "lynxcgi://localhost", 19) == 0) {
	    StrAllocCopy(pgm, arg+19);
	} else {
	    StrAllocCopy(pgm, arg+8);
	}
	if ((cp=strchr(pgm, '?')) != NULL) { /* Need to terminate executable */
	    *cp++ = '\0';
	    pgm_args = cp;
	}
    }

    StrAllocCopy(orig_pgm, pgm);
    if ((cp=strchr(pgm, '#')) != NULL) {
	/*
	 *  Strip a #fragment from path.  In this case any pgm_args
	 *  found above will also be bogus, since the '?' came after
	 *  the '#' and is part of the fragment.  Note that we don't
	 *  handle the case where a '#' appears after a '?' properly
	 *  according to URL rules. - kw
	 */
	*cp = '\0';
	pgm_args = NULL;
    }
    HTUnEscape(pgm);

    /* BEGIN WebSter Mods */
    /* If pgm is not stat-able, see if PATH_INFO data is at the end of pgm */
    if ((statrv = stat(pgm, &stat_buf)) < 0) {
	StrAllocCopy(pgm_buff, pgm);
	while (statrv < 0 || (statrv = stat(pgm_buff, &stat_buf)) < 0) {
	    if ((cp=strrchr(pgm_buff, '/')) != NULL) {
		*cp = '\0';
		statrv = 999;	/* force new stat()  - kw */
	    } else {
		if (TRACE)
		    perror("LYNXCGI: strrchr(pgm_buff, '/') returned NULL");
	    	break;
	    }
        }

	if (statrv < 0) {
	    /* Did not find PATH_INFO data */
	    if (TRACE) 
		perror("LYNXCGI: stat() of pgm_buff failed");
	} else {
	    /* Found PATH_INFO data. Strip it off of pgm and into path_info. */
	    StrAllocCopy(path_info, pgm+strlen(pgm_buff));
	    strcpy(pgm, pgm_buff);
	    if (TRACE)
		fprintf(stderr,
			"LYNXCGI: stat() of %s succeeded, path_info=\"%s\".\n",
			pgm_buff, path_info);
	}
	FREE(pgm_buff);
    }
    /* END WebSter Mods */

    if (statrv != 0) {
	/*
	 *  Neither the path as given nor any components examined by
	 *  backing up were stat()able. - kw
	 */
	HTAlert("Unable to access cgi script");
	if (TRACE) {
	    perror("LYNXCGI: stat() failed");
	}
	status = -4;

    } else if (!(S_ISREG(stat_buf.st_mode) &&
		 stat_buf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))) {
	/*
	 *  Not a runnable file, See if we can load it using "file:" code.
	 */
	char *new_arg = NULL;

	/*
	 *  But try "file:" only if the file we are looking at is the path
	 *  as given (no path_info was extracted), otherwise it will be
	 *  to confusing to know just what file is loaded. - kw
	 */
	if (path_info) {
	    if (TRACE) {
		fprintf(stderr,
			"%s is not a file and %s not an executable, giving up.\n",
			orig_pgm, pgm);
	    }
	    FREE(path_info);
	    FREE(pgm);
	    FREE(orig_pgm);
	    status = -4;
	    return(status);
	}
	    
	StrAllocCopy(new_arg, "file://localhost");
	StrAllocCat(new_arg, orig_pgm);

	if (TRACE) {
	    fprintf(stderr,
		    "%s is not an executable file, passing the buck.\n", arg);
	}
	status = HTLoadFile(new_arg, anAnchor, format_out, sink);
	FREE(new_arg);

    } else if (path_info &&
	       anAnchor != HTMainAnchor &&
	       !(reloading && anAnchor->document) &&
	       strcmp(arg, HTLoadedDocumentURL()) &&
	       HText_AreDifferent(anAnchor, arg) &&
	       HTUnEscape(orig_pgm) &&
	       !exec_ok(HTLoadedDocumentURL(), orig_pgm,
			CGI_PATH)) { /* exec_ok gives out msg. */
	/*
	 *  If we have extra path info and are not just reloading
	 *  the current, check the full file path (after unescaping)
	 *  now to catch forbidden segments. - kw
	 */
	status = HT_NOT_LOADED;

    } else if (no_lynxcgi) {
	_statusline(CGI_DISABLED);
	sleep(MessageSecs);
	status = HT_NOT_LOADED;

    } else if (no_bookmark_exec &&
	       anAnchor != HTMainAnchor &&
	       !(reloading && anAnchor->document) &&
	       strcmp(arg, HTLoadedDocumentURL()) &&
	       HText_AreDifferent(anAnchor, arg) &&
 	       HTLoadedDocumentBookmark()) {
	/*
	 *  If we are reloading a lynxcgi document that had already been
	 *  loaded, the various checks above should allow it even if
	 *  no_bookmark_exec is TRUE an we are not now coming from a
	 *  bookmark page. - kw
	 */
	_statusline(BOOKMARK_EXEC_DISABLED);
	sleep(MessageSecs);
	status = HT_NOT_LOADED;

    } else if (anAnchor != HTMainAnchor &&
	       !(reloading && anAnchor->document) &&
	       strcmp(arg, HTLoadedDocumentURL()) &&
	       HText_AreDifferent(anAnchor, arg) &&
	       !exec_ok(HTLoadedDocumentURL(), pgm,
			CGI_PATH)) { /* exec_ok gives out msg. */
	/*
	 *  If we are reloading a lynxcgi document that had already been
	 *  loaded, the various checks above should allow it even if
	 *  exec_ok() would reject it because we are not now coming from
	 *  a document with a URL allowed by TRUSTED_LYNXCGI rules. - kw
	 */
	status = HT_NOT_LOADED;

    } else {
	HTFormat format_in;
	HTStream *target  = NULL;		/* Unconverted data */
	int fd1[2], fd2[2];
	char buf[1024];
	pid_t pid;
#if HAVE_TYPE_UNIONWAIT
	union wait wstatus;
#else
	int wstatus;
#endif

	if (anAnchor->isHEAD || keep_mime_headers) {

	    /* Show output as plain text */
	    format_in = WWW_PLAINTEXT;
	} else {
	    
	    /* Decode full HTTP response */
	    format_in = HTAtom_for("www/mime");
	}
		
	target = HTStreamStack(format_in,
			       format_out,
			       sink, anAnchor);
		
	if (!target || target == NULL) {
	    sprintf(buf, CANNOT_CONVERT_I_TO_O,
		    HTAtom_name(format_in), HTAtom_name(format_out));
	    _statusline(buf);
	    sleep(AlertSecs);
	    status = HT_NOT_LOADED;

	} else if (anAnchor->post_data && pipe(fd1) < 0) {
	    HTAlert(CONNECT_SET_FAILED);
	    if (TRACE) {
		perror("LYNXCGI: pipe() failed");
	    }
	    status = -3;
	    
	} else if (pipe(fd2) < 0) {
	    HTAlert(CONNECT_SET_FAILED);
	    if (TRACE) {
		perror("LYNXCGI: pipe() failed");
	    }
	    close(fd1[0]);
	    close(fd1[1]);
	    status = -3;
	    
	} else {	
	    static BOOL first_time = TRUE;      /* One time setup flag */

	    if (first_time) {	/* Set up static environment variables */
		first_time = FALSE;	/* Only once */
		
		add_environment_value("REMOTE_HOST=localhost");
		add_environment_value("REMOTE_ADDR=127.0.0.1");
		
		sprintf(user_agent, "HTTP_USER_AGENT=%s/%s libwww/%s",
			LYNX_NAME, LYNX_VERSION, HTLibraryVersion);
		add_environment_value(user_agent);
		
		sprintf(server_software, "SERVER_SOFTWARE=%s/%s",
			LYNX_NAME, LYNX_VERSION);
		add_environment_value(server_software);
	    }
	    
	    if ((pid = fork()) > 0) { /* The good, */
		int chars, total_chars;
		
		close(fd2[1]);
		
		if (anAnchor->post_data) {
		    int written, remaining, total_written = 0;
		    close(fd1[0]);

		    /* We have form data to push across the pipe */
		    if (TRACE) {
			fprintf(stderr, "LYNXCGI: Doing post, content-type '%s'\n",
				anAnchor->post_content_type);
			fprintf(stderr,
				"LYNXCGI: Writing:\n%s----------------------------------\n",
				anAnchor->post_data);			
		    }
		    remaining = strlen(anAnchor->post_data);
		    while ((written = write(fd1[1],
					    anAnchor->post_data + total_written,
					    remaining)) != 0) {
			if (written < 0) {
#ifdef EINTR
			    if (errno == EINTR)
				continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
			    if (errno == ERESTARTSYS)
				continue;
#endif /* ERESTARTSYS */
			    if (TRACE) {
				perror("LYNXCGI: write() of POST data failed");
			    }
			    break;
			}
			if (TRACE) {
			    fprintf(stderr,
				    "LYNXCGI: Wrote %d bytes of POST data.\n",
				    written);
			}
			total_written += written;
			remaining -= written;
			if (remaining == 0)
			    break;
		    }
		    if (remaining != 0) {
			if (TRACE)
			    fprintf(stderr,
				    "LYNXCGI: %d bytes remain unwritten!\n",
				    remaining);
		    }
		    close(fd1[1]);
		}
		
		total_chars = 0;
		while((chars = read(fd2[0], buf, sizeof(buf))) > 0) {
		    char line[40];
		    
		    total_chars += chars;
		    sprintf (line, "Read %d bytes of data.", total_chars);
		    HTProgress(line);
		    if (TRACE) {
			fprintf(stderr, "LYNXCGI: Rx: %.*s\n", chars, buf);
		    }
		    
		    (*target->isa->put_block)(target, buf, chars);
		}
#if !HAVE_WAITPID
		while (wait(&wstatus) != pid)
		    ; /* do nothing */
#else
		while (-1 == waitpid(pid, &wstatus, 0)) { /* wait for child */
#ifdef EINTR
		    if (errno == EINTR)
			continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
		    if (errno == ERESTARTSYS)
			continue;
#endif /* ERESTARTSYS */
		    break;
		}
#endif /* !HAVE_WAITPID */
		close(fd2[0]);
		status = HT_LOADED;
		
	    } else if (pid == 0) { /* The Bad, */
		char **argv = NULL;
		char post_len[32];
		int argv_cnt = 3; /* name, one arg and terminator */
		char **cur_argv = NULL;
		char buf[BUFSIZ];

		/* Set up output pipe */
		close(fd2[0]);
		dup2(fd2[1], fileno(stdout)); /* Should check success code */
		dup2(fd2[1], fileno(stderr));
		close(fd2[1]);

		sprintf(buf, "HTTP_ACCEPT_LANGUAGE=%.*s",
			     (int)(sizeof(buf) - 22), language);
		buf[(sizeof(buf) - 1)] = '\0';
		add_environment_value(buf);

		if (pref_charset) {
		    cp = NULL;
		    StrAllocCopy(cp, "HTTP_ACCEPT_CHARSET=");
		    StrAllocCat(cp, pref_charset);
		    add_environment_value(cp);
		}

		if (anAnchor->post_data &&
		    anAnchor->post_content_type) {
		    cp = NULL;
		    StrAllocCopy(cp, "CONTENT_TYPE=");
		    StrAllocCat(cp, anAnchor->post_content_type);
		    add_environment_value(cp);
		}

		if (anAnchor->post_data) { /* post script, read stdin */
		    close(fd1[1]);
		    dup2(fd1[0], fileno(stdin));
		    close(fd1[0]);

		    /* Build environment variables */

		    add_environment_value("REQUEST_METHOD=POST");

		    sprintf(post_len, "CONTENT_LENGTH=%d",
			    strlen(anAnchor->post_data));
		    add_environment_value(post_len);
		} else {
		    close(fileno(stdin));

		    if (anAnchor->isHEAD) {
			add_environment_value("REQUEST_METHOD=HEAD");
		    }
		}

		/* 
		 * Set up argument line, mainly for <index> scripts
		 */
		if (pgm_args != NULL) {
		    for (cp = pgm_args; *cp != '\0'; cp++) {
			if (*cp == '+') {
			    argv_cnt++;
			}
		    }
		}

		argv = (char**)malloc(argv_cnt * sizeof(char*));
		if (argv == NULL) {
		    outofmem(__FILE__, "LYCgi");
		}
		cur_argv = argv + 1;		/* For argv[0] */
		if (pgm_args != NULL) {		
		    char *cr;

		    /* Data for a get/search form */
		    if (is_www_index) {
			add_environment_value("REQUEST_METHOD=SEARCH");
		    } else if (!anAnchor->isHEAD) {
			add_environment_value("REQUEST_METHOD=GET");
		    }
		    
		    cp = NULL;
		    StrAllocCopy(cp, "QUERY_STRING=");
		    StrAllocCat(cp, pgm_args);
		    add_environment_value(cp);

		    /*
		     * Split up arguments into argv array
		     */
		    cp = pgm_args;
		    cr = cp;
		    while(1) {
			if (*cp == '\0') {
			    *(cur_argv++) = HTUnEscape(cr);
			    break;
			    
			} else if (*cp == '+') {
			    *cp++ = '\0';
			    *(cur_argv++) = HTUnEscape(cr);
			    cr = cp;
			}
			cp++;
		    }
		}
		*cur_argv = NULL;	/* Terminate argv */		
		argv[0] = pgm;

		/* Begin WebSter Mods  -jkt */                
		if (LYCgiDocumentRoot != NULL) {
		    /* Add DOCUMENT_ROOT to env */
		    cp = NULL;
		    StrAllocCopy(cp, "DOCUMENT_ROOT=");
		    StrAllocCat(cp, LYCgiDocumentRoot);
		    add_environment_value(cp);
		}
		if (path_info != NULL ) {
		    /* Add PATH_INFO to env */
		    cp = NULL;
		    StrAllocCopy(cp, "PATH_INFO=");
		    StrAllocCat(cp, path_info);
		    add_environment_value(cp);
		}
		if (LYCgiDocumentRoot != NULL && path_info != NULL ) {
		    /* Construct and add PATH_TRANSLATED to env */
		    StrAllocCopy(document_root, LYCgiDocumentRoot);
		    if (document_root[strlen(document_root) - 1] == '/') {
			document_root[strlen(document_root) - 1] = '\0';
		    }
		    path_translated = document_root;
		    StrAllocCat(path_translated, path_info);
		    cp = NULL;
		    StrAllocCopy(cp, "PATH_TRANSLATED=");
		    StrAllocCat(cp, path_translated);
		    add_environment_value(cp);
		    FREE(path_translated);
		}
		/* End WebSter Mods  -jkt */

		execve(argv[0], argv, env);
		if (TRACE) {
		    perror("LYNXCGI: execve failed");
		}
		
	    } else {	/* and the Ugly */
		HTAlert(CONNECT_FAILED);
		if (TRACE) {
		    perror("LYNXCGI: fork() failed");
		}
		status = HT_NO_DATA;
		close(fd1[0]);
		close(fd1[1]);
		close(fd2[0]);
		close(fd2[1]);
		status = -1;
	    }

	}
	if (target != NULL) {
	    (*target->isa->_free)(target);
	}
    }
    FREE(path_info);
    FREE(pgm);
    FREE(orig_pgm);
#else  /* VMS */
	HTStream *target;
	char buf[256];

	target = HTStreamStack(WWW_HTML, 
			       format_out,
			       sink, anAnchor);

	sprintf(buf,"<head>\n<title>Good Advice</title>\n</head>\n<body>\n");
	(*target->isa->put_block)(target, buf, strlen(buf));
	
	sprintf(buf,"<h1>Good Advice</h1>\n");
	(*target->isa->put_block)(target, buf, strlen(buf));

	sprintf(buf, "An excellent http server for VMS is available via <a\n");
	(*target->isa->put_block)(target, buf, strlen(buf));

	sprintf(buf,
	 "href=\"http://kcgl1.eng.ohio-state.edu/www/doc/serverinfo.html\"\n");
	(*target->isa->put_block)(target, buf, strlen(buf));

	sprintf(buf, ">this link</a>.\n");
	(*target->isa->put_block)(target, buf, strlen(buf));

	sprintf(buf,
		"<p>It provides <b>state of the art</b> CGI script support.\n");
	(*target->isa->put_block)(target, buf, strlen(buf));

	sprintf(buf,"</body>\n");
	(*target->isa->put_block)(target, buf, strlen(buf));

	(*target->isa->_free)(target);
	status = HT_LOADED;
#endif /* VMS */
#else /* LYNXCGI_LINKS */
    _statusline(CGI_NOT_COMPILED);
    sleep(MessageSecs);
    status = HT_NOT_LOADED;
#endif /* LYNXCGI_LINKS */
    return(status);
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYCGI_C_GLOBALDEF_1_INIT { "lynxcgi", LYLoadCGI, 0 }
GLOBALDEF (HTProtocol,LYLynxCGI,_LYCGI_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol LYLynxCGI = { "lynxcgi", LYLoadCGI, 0 };
#endif /* GLOBALDEF_IS_MACRO */
