/*                   Lynx CGI support                              LYCgi.c
 *                   ================
 *
 * Authors
 *          GL      George Lindholm <George.Lindholm@ubc.ca>
 *
 * History
 *      15 Jun 95   Created as way to provide a lynx based service with
 *                  dynamic pages without the need for a http daemon.  GL
 *      27 Jun 95   Added <index> (command line) support.  Various cleanup
 *                  and bug fixes. GL
 *	04 Sep 97   Added support for PATH_INFO scripts.  JKT
 *
 * Bugs
 *      If the called scripts aborts before sending the mime headers then
 *      lynx hangs.
 *
 *      Should do something about SIGPIPE, (but then it should never happen)
 *
 *      No support for redirection.  Or mime-types.
 *
 *      Should try and parse for a HTTP 1.1 header in case we are "calling" a
 *      nph- script.
 */

#include <HTUtils.h>
#include <HTTP.h>
#include <HTParse.h>
#include <HTTCP.h>
#include <HTFormat.h>
#include <HTFile.h>
#include <HTAlert.h>
#include <HTMIME.h>
#include <HTAABrow.h>

#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <HTML.h>
#include <HTInit.h>
#include <LYGetFile.h>
#include <LYBookmark.h>
#include <GridText.h>
#include <LYCgi.h>
#include <LYStrings.h>
#include <LYLocal.h>

#include <LYLeaks.h>
#include <www_wait.h>

struct _HTStream {
    HTStreamClass *isa;
};

static char **env = NULL;	/* Environment variables */
static int envc_size = 0;	/* Slots in environment array */
static int envc = 0;		/* Slots used so far */
static HTList *alloced = NULL;

#ifdef LYNXCGI_LINKS
static char *user_agent = NULL;
static char *server_software = NULL;
static char *accept_language = NULL;
static char *post_len = NULL;
#endif /* LYNXCGI_LINKS */

static void add_environment_value(const char *env_value);

#define PERROR(msg) CTRACE((tfp, "LYNXCGI: %s: %s\n", msg, LYStrerror(errno)))

#define PUTS(buf)    (*target->isa->put_block)(target, buf, strlen(buf))

#ifdef LY_FIND_LEAKS
static void free_alloced_lynxcgi(void)
{
    void *ptr;

    while ((ptr = HTList_removeLastObject(alloced)) != NULL) {
	FREE(ptr);
    }
    FREE(alloced);
#ifdef LYNXCGI_LINKS
    FREE(user_agent);
    FREE(server_software);
#endif
}
#endif /* LY_FIND_LEAKS */

static void remember_alloced(void *ptr)
{
    if (!alloced) {
	alloced = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(free_alloced_lynxcgi);
#endif
    }
    HTList_addObject(alloced, ptr);
}

/*
 * Simple routine for expanding the environment array and adding a value to
 * it
 */
static void add_environment_value(const char *env_value)
{
    if (envc == envc_size) {	/* Need some more slots */
	envc_size += 10;
	if (env) {
	    env = (char **) realloc(env,
				    sizeof(env[0]) * (envc_size + 2));
	    /* + terminator and base 0 */
	} else {
	    env = (char **) malloc(sizeof(env[0]) * (envc_size + 2));
	    /* + terminator and base 0 */
	    remember_alloced(env);
	}
	if (env == NULL) {
	    outofmem(__FILE__, "LYCgi");
	}
    }

    env[envc++] = (char *) env_value;
    env[envc] = NULL;		/* Make sure it is always properly terminated */
}

/*
 * Add the value of an existing environment variable to those passed on to the
 * lynxcgi script.
 */
void add_lynxcgi_environment(const char *variable_name)
{
    char *env_value;

    env_value = LYGetEnv(variable_name);
    if (env_value != NULL) {
	char *add_value = NULL;

	HTSprintf0(&add_value, "%s=%s", variable_name, env_value);
	add_environment_value(add_value);
	remember_alloced(add_value);
    }
}

#ifdef __MINGW32__
static int LYLoadCGI(const char *arg,
		     HTParentAnchor *anAnchor,
		     HTFormat format_out,
		     HTStream *sink)
{
    return -1;
}
#else
#ifdef LYNXCGI_LINKS
/*
 * Wrapper for exec_ok(), confirming with user if the link text is not visible
 * in the status line.
 */
static BOOL can_exec_cgi(const char *linktext, const char *linkargs)
{
    const char *format = gettext("Do you want to execute \"%s\"?");
    char *message = NULL;
    char *command = NULL;
    char *p;
    BOOL result = TRUE;

    if (!exec_ok(HTLoadedDocumentURL(), linktext, CGI_PATH)) {
	/* exec_ok gives out msg. */
	result = FALSE;
    } else if (user_mode < ADVANCED_MODE) {
	StrAllocCopy(command, linktext);
	if (non_empty(linkargs)) {
	    HTSprintf(&command, " %s", linkargs);
	}
	HTUnEscape(command);
	for (p = command; *p; ++p)
	    if (*p == '+')
		*p = ' ';
	HTSprintf0(&message, format, command);
	result = HTConfirm(message);
	FREE(message);
	FREE(command);
    }
    return result;
}
#endif /* LYNXCGI_LINKS */

static int LYLoadCGI(const char *arg,
		     HTParentAnchor *anAnchor,
		     HTFormat format_out,
		     HTStream *sink)
{
    int status = 0;

#ifdef LYNXCGI_LINKS
#ifndef VMS
    char *cp;
    struct stat stat_buf;
    char *pgm = NULL;		/* executable */
    char *pgm_args = NULL;	/* and its argument(s) */
    int statrv;
    char *orig_pgm = NULL;	/* Path up to ? as given, URL-escaped */
    char *document_root = NULL;	/* Corrected value of DOCUMENT_ROOT  */
    char *path_info = NULL;	/* PATH_INFO extracted from pgm      */
    char *pgm_buff = NULL;	/* PATH_INFO extraction buffer       */
    char *path_translated;	/* From document_root/path_info      */

    if (isEmpty(arg) || strlen(arg) <= 8) {
	HTAlert(BAD_REQUEST);
	status = -2;
	return (status);

    } else {
	if (strncmp(arg, "lynxcgi://localhost", 19) == 0) {
	    StrAllocCopy(pgm, arg + 19);
	} else {
	    StrAllocCopy(pgm, arg + 8);
	}
	if ((cp = strchr(pgm, '?')) != NULL) {	/* Need to terminate executable */
	    *cp++ = '\0';
	    pgm_args = cp;
	}
    }

    StrAllocCopy(orig_pgm, pgm);
    if ((cp = trimPoundSelector(pgm)) != NULL) {
	/*
	 * Strip a #fragment from path.  In this case any pgm_args found above
	 * will also be bogus, since the '?' came after the '#' and is part of
	 * the fragment.  Note that we don't handle the case where a '#'
	 * appears after a '?' properly according to URL rules.  - kw
	 */
	pgm_args = NULL;
    }
    HTUnEscape(pgm);

    /* BEGIN WebSter Mods */
    /* If pgm is not stat-able, see if PATH_INFO data is at the end of pgm */
    if ((statrv = stat(pgm, &stat_buf)) < 0) {
	StrAllocCopy(pgm_buff, pgm);
	while (statrv < 0 || (statrv = stat(pgm_buff, &stat_buf)) < 0) {
	    if ((cp = strrchr(pgm_buff, '/')) != NULL) {
		*cp = '\0';
		statrv = 1;	/* force new stat()  - kw */
	    } else {
		PERROR("strrchr(pgm_buff, '/') returned NULL");
		break;
	    }
	}

	if (statrv < 0) {
	    /* Did not find PATH_INFO data */
	    PERROR("stat() of pgm_buff failed");
	} else {
	    /* Found PATH_INFO data.  Strip it off of pgm and into path_info. */
	    StrAllocCopy(path_info, pgm + strlen(pgm_buff));
	    /* The following is safe since pgm_buff was derived from pgm
	       by stripping stuff off its end and by HTUnEscaping, so we
	       know we have enough memory allocated for pgm.  Note that
	       pgm_args may still point into that memory, so we cannot
	       reallocate pgm here. - kw */
	    strcpy(pgm, pgm_buff);
	    CTRACE((tfp,
		    "LYNXCGI: stat() of %s succeeded, path_info=\"%s\".\n",
		    pgm_buff, path_info));
	}
	FREE(pgm_buff);
    }
    /* END WebSter Mods */

    if (statrv != 0) {
	/*
	 * Neither the path as given nor any components examined by backing up
	 * were stat()able.  - kw
	 */
	HTAlert(gettext("Unable to access cgi script"));
	PERROR("stat() failed");
	status = -4;

    } else
#ifdef _WINDOWS			/* 1998/01/14 (Wed) 09:16:04 */
#define isExecutable(mode) (mode & (S_IXUSR))
#else
#define isExecutable(mode) (mode & (S_IXUSR|S_IXGRP|S_IXOTH))
#endif
    if (!(S_ISREG(stat_buf.st_mode) && isExecutable(stat_buf.st_mode))) {
	/*
	 * Not a runnable file, See if we can load it using "file:" code.
	 */
	char *new_arg = NULL;

	/*
	 * But try "file:" only if the file we are looking at is the path as
	 * given (no path_info was extracted), otherwise it will be to
	 * confusing to know just what file is loaded.  - kw
	 */
	if (path_info) {
	    CTRACE((tfp,
		    "%s is not a file and %s not an executable, giving up.\n",
		    orig_pgm, pgm));
	    FREE(path_info);
	    FREE(pgm);
	    FREE(orig_pgm);
	    status = -4;
	    return (status);
	}

	LYLocalFileToURL(&new_arg, orig_pgm);

	CTRACE((tfp, "%s is not an executable file, passing the buck.\n", arg));
	status = HTLoadFile(new_arg, anAnchor, format_out, sink);
	FREE(new_arg);

    } else if (path_info &&
	       anAnchor != HTMainAnchor &&
	       !(reloading && anAnchor->document) &&
	       strcmp(arg, HTLoadedDocumentURL()) &&
	       HText_AreDifferent(anAnchor, arg) &&
	       HTUnEscape(orig_pgm) &&
	       !can_exec_cgi(orig_pgm, "")) {
	/*
	 * If we have extra path info and are not just reloading the current,
	 * check the full file path (after unescaping) now to catch forbidden
	 * segments.  - kw
	 */
	status = HT_NOT_LOADED;

    } else if (no_lynxcgi) {
	HTUserMsg(CGI_DISABLED);
	status = HT_NOT_LOADED;

    } else if (no_bookmark_exec &&
	       anAnchor != HTMainAnchor &&
	       !(reloading && anAnchor->document) &&
	       strcmp(arg, HTLoadedDocumentURL()) &&
	       HText_AreDifferent(anAnchor, arg) &&
	       HTLoadedDocumentBookmark()) {
	/*
	 * If we are reloading a lynxcgi document that had already been loaded,
	 * the various checks above should allow it even if no_bookmark_exec is
	 * TRUE an we are not now coming from a bookmark page.  - kw
	 */
	HTUserMsg(BOOKMARK_EXEC_DISABLED);
	status = HT_NOT_LOADED;

    } else if (anAnchor != HTMainAnchor &&
	       !(reloading && anAnchor->document) &&
	       strcmp(arg, HTLoadedDocumentURL()) &&
	       HText_AreDifferent(anAnchor, arg) &&
	       !can_exec_cgi(pgm, pgm_args)) {
	/*
	 * If we are reloading a lynxcgi document that had already been loaded,
	 * the various checks above should allow it even if exec_ok() would
	 * reject it because we are not now coming from a document with a URL
	 * allowed by TRUSTED_LYNXCGI rules.  - kw
	 */
	status = HT_NOT_LOADED;

    } else {
	HTFormat format_in;
	HTStream *target = NULL;	/* Unconverted data */
	int fd1[2], fd2[2];
	char buf[1024];
	int pid;

#ifdef HAVE_TYPE_UNIONWAIT
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
	    char *tmp = 0;

	    HTSprintf0(&tmp, CANNOT_CONVERT_I_TO_O,
		       HTAtom_name(format_in),
		       HTAtom_name(format_out));
	    HTAlert(tmp);
	    FREE(tmp);
	    status = HT_NOT_LOADED;

	} else if (anAnchor->post_data && pipe(fd1) < 0) {
	    HTAlert(CONNECT_SET_FAILED);
	    PERROR("pipe() failed");
	    status = -3;

	} else if (pipe(fd2) < 0) {
	    HTAlert(CONNECT_SET_FAILED);
	    PERROR("pipe() failed");
	    close(fd1[0]);
	    close(fd1[1]);
	    status = -3;

	} else {
	    static BOOL first_time = TRUE;	/* One time setup flag */

	    if (first_time) {	/* Set up static environment variables */
		first_time = FALSE;	/* Only once */

		add_environment_value("REMOTE_HOST=localhost");
		add_environment_value("REMOTE_ADDR=127.0.0.1");

		HTSprintf0(&user_agent, "HTTP_USER_AGENT=%s/%s libwww/%s",
			   LYNX_NAME, LYNX_VERSION, HTLibraryVersion);
		add_environment_value(user_agent);

		HTSprintf0(&server_software, "SERVER_SOFTWARE=%s/%s",
			   LYNX_NAME, LYNX_VERSION);
		add_environment_value(server_software);
	    }
	    fflush(stdout);
	    fflush(stderr);
	    CTRACE_FLUSH(tfp);

	    if ((pid = fork()) > 0) {	/* The good, */
		int chars, total_chars;

		close(fd2[1]);

		if (anAnchor->post_data) {
		    int written, remaining, total_written = 0;

		    close(fd1[0]);

		    /* We have form data to push across the pipe */
		    if (TRACE) {
			CTRACE((tfp,
				"LYNXCGI: Doing post, content-type '%s'\n",
				anAnchor->post_content_type));
			CTRACE((tfp, "LYNXCGI: Writing:\n"));
			trace_bstring(anAnchor->post_data);
			CTRACE((tfp, "----------------------------------\n"));
		    }
		    remaining = BStrLen(anAnchor->post_data);
		    while ((written = write(fd1[1],
					    BStrData(anAnchor->post_data) + total_written,
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
			    PERROR("write() of POST data failed");
			    break;
			}
			CTRACE((tfp, "LYNXCGI: Wrote %d bytes of POST data.\n",
				written));
			total_written += written;
			remaining -= written;
			if (remaining == 0)
			    break;
		    }
		    if (remaining != 0) {
			CTRACE((tfp, "LYNXCGI: %d bytes remain unwritten!\n",
				remaining));
		    }
		    close(fd1[1]);
		}

		HTReadProgress(total_chars = 0, 0);
		while ((chars = read(fd2[0], buf, sizeof(buf))) != 0) {
		    if (chars < 0) {
#ifdef EINTR
			if (errno == EINTR)
			    continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
			if (errno == ERESTARTSYS)
			    continue;
#endif /* ERESTARTSYS */
			PERROR("read() of CGI output failed");
			break;
		    }
		    HTReadProgress(total_chars += chars, 0);
		    CTRACE((tfp, "LYNXCGI: Rx: %.*s\n", chars, buf));
		    (*target->isa->put_block) (target, buf, chars);
		}

		if (chars < 0 && total_chars == 0) {
		    status = HT_NOT_LOADED;
		    (*target->isa->_abort) (target, NULL);
		    target = NULL;
		} else if (chars != 0) {
		    status = HT_PARTIAL_CONTENT;
		} else {
		    status = HT_LOADED;
		}

#if !HAVE_WAITPID
		while (wait(&wstatus) != pid) ;		/* do nothing */
#else
		while (-1 == waitpid(pid, &wstatus, 0)) {	/* wait for child */
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

	    } else if (pid == 0) {	/* The Bad, */
		char **argv = NULL;
		int argv_cnt = 3;	/* name, one arg and terminator */
		char **cur_argv = NULL;
		int exec_errno;

		/* Set up output pipe */
		close(fd2[0]);
		dup2(fd2[1], fileno(stdout));	/* Should check success code */
		dup2(fd2[1], fileno(stderr));
		close(fd2[1]);

		if (non_empty(language)) {
		    HTSprintf0(&accept_language, "HTTP_ACCEPT_LANGUAGE=%s", language);
		    add_environment_value(accept_language);
		}

		if (non_empty(pref_charset)) {
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

		if (anAnchor->post_data) {	/* post script, read stdin */
		    close(fd1[1]);
		    dup2(fd1[0], fileno(stdin));
		    close(fd1[0]);

		    /* Build environment variables */

		    add_environment_value("REQUEST_METHOD=POST");

		    HTSprintf0(&post_len, "CONTENT_LENGTH=%d",
			       BStrLen(anAnchor->post_data));
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

		argv = (char **) malloc(argv_cnt * sizeof(char *));

		if (argv == NULL) {
		    outofmem(__FILE__, "LYCgi");
		}
		cur_argv = argv + 1;	/* For argv[0] */
		if (pgm_args != NULL) {
		    char *cr;

		    /* Data for a get/search form */
		    if (is_www_index) {
			add_environment_value("REQUEST_METHOD=SEARCH");
		    } else if (!anAnchor->isHEAD && !anAnchor->post_data) {
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
		    while (1) {
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
		} else if (!anAnchor->isHEAD && !anAnchor->post_data) {
		    add_environment_value("REQUEST_METHOD=GET");
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
		if (path_info != NULL) {
		    /* Add PATH_INFO to env */
		    cp = NULL;
		    StrAllocCopy(cp, "PATH_INFO=");
		    StrAllocCat(cp, path_info);
		    add_environment_value(cp);
		}
		if (LYCgiDocumentRoot != NULL && path_info != NULL) {
		    /* Construct and add PATH_TRANSLATED to env */
		    StrAllocCopy(document_root, LYCgiDocumentRoot);
		    LYTrimHtmlSep(document_root);
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
		exec_errno = errno;
		PERROR("execve failed");
		printf("Content-Type: text/plain\r\n\r\n");
		if (!anAnchor->isHEAD) {
		    printf("exec of %s failed", pgm);
		    printf(": %s.\r\n", LYStrerror(exec_errno));
		}
		fflush(stdout);
		fflush(stderr);
		_exit(1);

	    } else {		/* and the Ugly */
		HTAlert(CONNECT_FAILED);
		PERROR("fork() failed");
		status = HT_NO_DATA;
		close(fd1[0]);
		close(fd1[1]);
		close(fd2[0]);
		close(fd2[1]);
		status = -1;
	    }

	}
	if (target != NULL) {
	    (*target->isa->_free) (target);
	}
    }
    FREE(path_info);
    FREE(pgm);
    FREE(orig_pgm);
#else /* VMS */
    HTStream *target;
    char *buf = 0;

    target = HTStreamStack(WWW_HTML,
			   format_out,
			   sink, anAnchor);

    HTSprintf0(&buf, "<html>\n<head>\n<title>%s</title>\n</head>\n<body>\n",
	       gettext("Good Advice"));
    PUTS(buf);

    HTSprintf0(&buf, "<h1>%s</h1>\n", gettext("Good Advice"));
    PUTS(buf);

    HTSprintf0(&buf, "%s <a\n",
	       gettext("An excellent http server for VMS is available via"));
    PUTS(buf);

    HTSprintf0(&buf,
	       "href=\"http://www.ecr6.ohio-state.edu/www/doc/serverinfo.html\"\n");
    PUTS(buf);

    HTSprintf0(&buf, ">%s</a>.\n", gettext("this link"));
    PUTS(buf);

    HTSprintf0(&buf, "<p>%s\n",
	       gettext("It provides state of the art CGI script support.\n"));
    PUTS(buf);

    HTSprintf0(&buf, "</body>\n</html>\n");
    PUTS(buf);

    (*target->isa->_free) (target);
    FREE(buf);
    status = HT_LOADED;
#endif /* VMS */
#else /* LYNXCGI_LINKS */
    HTUserMsg(CGI_NOT_COMPILED);
    status = HT_NOT_LOADED;
#endif /* LYNXCGI_LINKS */

    (void) arg;
    (void) anAnchor;
    (void) format_out;
    (void) sink;

    return (status);
}
#endif /* __MINGW32__ */

#ifdef GLOBALDEF_IS_MACRO
#define _LYCGI_C_GLOBALDEF_1_INIT { "lynxcgi", LYLoadCGI, 0 }
GLOBALDEF(HTProtocol, LYLynxCGI, _LYCGI_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF HTProtocol LYLynxCGI =
{"lynxcgi", LYLoadCGI, 0};
#endif /* GLOBALDEF_IS_MACRO */
