#include "HTUtils.h"
#include "tcp.h"
#include "HTTP.h"
#include "HTAnchor.h"	    /* Anchor class */
#include "HTAccess.h"
#include "HTParse.h"
#include "LYCurses.h"
#include "GridText.h"
#include "LYGlobalDefs.h"
#include "LYUtils.h"
#include "LYCharSets.h"
#include "LYCharUtils.h"
#include "HTAlert.h"
#include "LYSignal.h"
#include "LYGetFile.h"
#include "LYPrint.h"
#include "LYHistory.h"
#include "LYStrings.h"
#include "LYClean.h"
#include "LYDownload.h"
#include "LYNews.h"
#include "LYMail.h"
#include "LYSystem.h"
#include "LYKeymap.h"
#include "LYBookmark.h"
#include "LYMap.h"
#include "LYList.h"
#ifdef VMS
#include "HTVMSUtils.h"
#endif /* VMS */
#ifdef DOSPATH
#include "HTDOS.h"
#endif
#ifdef DIRED_SUPPORT
#include "LYLocal.h"
#endif /* DIRED_SUPPORT */

#include "LYexit.h"
#include "LYLeaks.h"

#ifndef VMS
#ifdef SYSLOG_REQUESTED_URLS
#include <syslog.h>
#endif /* SYSLOG_REQUESTED_URLS */
#endif /* !VMS */

#define FREE(x) if (x) {free(x); x = NULL;}

PRIVATE int fix_http_urls PARAMS((document *doc));
extern char * WWW_Download_File;
#ifdef VMS
extern BOOLEAN LYDidRename;
#endif /* VMS */

#if 0 /* UNUSED */
#ifdef DIRED_SUPPORT
PRIVATE char * LYSanctify ARGS1(
	char *, 	href)
{
    int i;
    char *p, *cp, *tp;
    char address_buffer[1024];

    i = (strlen(href) - 1);
    while (i && href[i] == '/') href[i--] = '\0';

    if ((cp = (char *)strchr(href,'~')) != NULL) {
       if (!strncmp(href, "file://localhost/", 17))
	 tp = (href + 17);
       else
	 tp = (href + 5);
       if ((cp - tp) && *(cp-1) != '/')
	 return href;
       LYstrncpy(address_buffer, href, (cp - href));
       if (address_buffer[(strlen(address_buffer) - 1)] == '/')
	 address_buffer[(strlen(address_buffer) - 1)] = '\0';
       p = (char *)Home_Dir();
       strcat(address_buffer, p);
       if (strlen(++cp))
	 strcat(address_buffer, cp);
       if (strcmp(href, address_buffer))
	 StrAllocCopy(href, address_buffer);
    }
    return href;
}
#endif /* DIRED_SUPPORT */
#endif


PUBLIC BOOLEAN getfile ARGS1(
	document *,	doc)
{
	int url_type = 0;
	char *cp = NULL;
	char *temp = NULL;
	DocAddress WWWDoc;  /* a WWW absolute doc address struct */

	/*
	 *  Reset LYCancelDownload to prevent unwanted delayed effect. - KW
	 */
	if (LYCancelDownload) {
	    if (TRACE)
		fprintf(stderr,
			"getfile:    resetting LYCancelDownload to FALSE\n");
	    LYCancelDownload = FALSE;
	}

	/*
	 *  Reset fake 'Z' to prevent unwanted delayed effect. - kw
	 */
	LYFakeZap(NO);

Try_Redirected_URL:
	/*
	 *  Load the WWWDoc struct in case we need to use it.
	 */
	WWWDoc.address = doc->address;
	WWWDoc.post_data = doc->post_data;
	WWWDoc.post_content_type = doc->post_content_type;
	WWWDoc.bookmark = doc->bookmark;
	WWWDoc.isHEAD = doc->isHEAD;
	WWWDoc.safe = doc->safe;

	/*
	 *  Reset WWW_Download_File just in case.
	 */
	FREE(WWW_Download_File);

	/*
	 *  Reset redirect_post_content just in case.
	 */
	redirect_post_content = FALSE;

	if (TRACE) {
	    fprintf(stderr,"getfile: getting %s\n\n",doc->address);
	}

	/*
	 *  Protect against denial of service attacks
	 *  via the port 19 CHARGEN service, and block
	 *  connections to the port 25 ESMTP service.
	 *  Also reject any likely spoof attempts via
	 *  wrap arounds at 65536. - FM
	 */
	if ((temp = HTParse(doc->address, "", PARSE_HOST)) != NULL &&
	    strlen(temp) > 3) {
	    char *cp1;

	    if ((cp1 = strchr(temp, '@')) == NULL)
		cp1 = temp;
	    if ((cp = strrchr(cp1, ':')) != NULL) {
		long int value;

		cp++;
		if (sscanf(cp, "%ld", &value) == 1) {
		    if (value == 19 || value == 65555) {
			HTAlert(PORT_NINETEEN_INVALID);
			FREE(temp);
			return(NULLFILE);
		    }
		    if (value == 25 || value == 65561) {
			HTAlert(PORT_TWENTYFIVE_INVALID);
			FREE(temp);
			return(NULLFILE);
		    }
		    if (value > 65535 || value < 0) {
			char msg[265];
			sprintf(msg, PORT_INVALID, (unsigned long)value);
			HTAlert(msg);
			FREE(temp);
			return(NULLFILE);
		    }
		} else if (isdigit((unsigned char)*cp)) {
		    HTAlert(URL_PORT_BAD);
		    FREE(temp);
		    return(NULLFILE);
		}
	    }
	}
	cp = NULL;
	FREE(temp);

	/*
	 *  Check to see if this is a universal document ID
	 *  that lib WWW wants to handle.
	 *
	 *  Some special URL's we handle ourselves. :)
	 */
	if ((url_type = is_url(doc->address)) != 0) {
		if (LYValidate && !LYPermitURL) {
		    if (!(url_type == HTTP_URL_TYPE ||
			  url_type == HTTPS_URL_TYPE ||
			  url_type == LYNXHIST_URL_TYPE ||
			  url_type == LYNXKEYMAP_URL_TYPE ||
			  url_type == LYNXIMGMAP_URL_TYPE ||
			  url_type == LYNXCOOKIE_URL_TYPE ||
			  0==strncasecomp(WWWDoc.address, helpfilepath,
					  strlen(helpfilepath)) ||
			  (lynxlistfile != NULL &&
			   0==strncasecomp(WWWDoc.address, lynxlistfile,
					  strlen(lynxlistfile))) ||
			  (lynxlinksfile != NULL &&
			   0==strncasecomp(WWWDoc.address, lynxlinksfile,
					  strlen(lynxlinksfile))) ||
			  (lynxjumpfile != NULL &&
			   0==strncasecomp(WWWDoc.address, lynxjumpfile,
					  strlen(lynxjumpfile))))) {
			_statusline(NOT_HTTP_URL_OR_ACTION);
			sleep(MessageSecs);
			return(NULLFILE);
		    }
		}
		if (traversal) {
		    /*
		     *	Only traverse http URLs.
		     */
		    if (url_type != HTTP_URL_TYPE &&
			url_type != LYNXIMGMAP_URL_TYPE)
			return(NULLFILE);
		} else if (check_realm && !LYPermitURL && !LYJumpFileURL) {
		    if (!(0==strncmp(startrealm, WWWDoc.address,
				     strlen(startrealm)) ||
			  url_type == LYNXHIST_URL_TYPE ||
			  url_type == LYNXKEYMAP_URL_TYPE ||
			  url_type == LYNXIMGMAP_URL_TYPE ||
			  url_type == LYNXCOOKIE_URL_TYPE ||
			  url_type == LYNXPRINT_URL_TYPE ||
			  url_type == LYNXDOWNLOAD_URL_TYPE ||
			  url_type == MAILTO_URL_TYPE ||
			  url_type == NEWSPOST_URL_TYPE ||
			  url_type == NEWSREPLY_URL_TYPE ||
			  url_type == SNEWSPOST_URL_TYPE ||
			  url_type == SNEWSREPLY_URL_TYPE ||
			  (!LYUserSpecifiedURL &&
			   (url_type == LYNXEXEC_URL_TYPE ||
			    url_type == LYNXPROG_URL_TYPE ||
			    url_type == LYNXCGI_URL_TYPE)) ||
			  (WWWDoc.bookmark != NULL &&
			   *WWWDoc.bookmark != '\0') ||
			  0==strncasecomp(WWWDoc.address, helpfilepath,
					  strlen(helpfilepath)) ||
			  (lynxlistfile != NULL &&
			   0==strncasecomp(WWWDoc.address, lynxlistfile,
					  strlen(lynxlistfile))) ||
			  (lynxjumpfile != NULL &&
			   0==strncasecomp(WWWDoc.address, lynxjumpfile,
					  strlen(lynxjumpfile))))) {
			_statusline(NOT_IN_STARTING_REALM);
			sleep(MessageSecs);
			return(NULLFILE);
		    }
		}
		if (WWWDoc.post_data &&
		    url_type != HTTP_URL_TYPE &&
		    url_type != HTTPS_URL_TYPE &&
		    url_type != LYNXCGI_URL_TYPE &&
		    url_type != LYNXIMGMAP_URL_TYPE &&
		    url_type != GOPHER_URL_TYPE &&
		    url_type != CSO_URL_TYPE &&
		    url_type != PROXY_URL_TYPE &&
		    !(url_type == FILE_URL_TYPE &&
		      *(LYlist_temp_url()) &&
		      !strncmp(WWWDoc.address, LYlist_temp_url(),
			       strlen(LYlist_temp_url())))) {
		    if (TRACE)
			fprintf(stderr,
				"getfile: dropping post_data!\n");
		    HTAlert("POST not supported for this URL - ignoring POST data!");
		    FREE(doc->post_data);
		    FREE(doc->post_content_type);
		    WWWDoc.post_data = NULL;
		    WWWDoc.post_content_type = NULL;
		}
#ifndef VMS
#ifdef SYSLOG_REQUESTED_URLS
		syslog(LOG_INFO|LOG_LOCAL5, "%s", doc->address);
#endif /* SYSLOG_REQUESTED_URLS */
#endif /* !VMS */
		if (url_type == UNKNOWN_URL_TYPE ||
		    url_type == AFS_URL_TYPE ||
		    url_type == PROSPERO_URL_TYPE) {
		    HTAlert(UNSUPPORTED_URL_SCHEME);
		    return(NULLFILE);

		} else if (url_type == DATA_URL_TYPE) {
		    HTAlert(UNSUPPORTED_DATA_URL);
		    return(NULLFILE);

		} else if (url_type == LYNXPRINT_URL_TYPE) {
		    return(printfile(doc));

		} else if (url_type == NEWSPOST_URL_TYPE ||
			   url_type == NEWSREPLY_URL_TYPE ||
			   url_type == SNEWSPOST_URL_TYPE ||
			   url_type == SNEWSREPLY_URL_TYPE) {

		    if (no_newspost) {
			_statusline(NEWSPOSTING_DISABLED);
			sleep(MessageSecs);
			return(NULLFILE);
		    } else {
			HTLoadAbsolute(&WWWDoc);
			return(NULLFILE);
		    }

		} else if (url_type == LYNXDOWNLOAD_URL_TYPE) {
		    LYDownload(doc->address);
#ifdef VMS
		    if (LYDidRename) {
			/*
			 *  The temporary file was saved to disk via a
			 *  rename(), so we can't access the temporary
			 *  file again via the download menu.  Clear the
			 *  flag, and return NULLFILE to pop. - FM
			 */
			LYDidRename = FALSE;
			return(NULLFILE);
		    } else {
			return(NORMAL);
		    }
#else
		    return(NORMAL);
#endif /* VMS */
		} else if (url_type == LYNXDIRED_URL_TYPE) {
#ifdef DIRED_SUPPORT
		    if (no_dired_support) {
		       _statusline(DIRED_DISABLED);
		       sleep(MessageSecs);
		       return(NULLFILE);
		    } else {
		       local_dired(doc);
		       WWWDoc.address = doc->address;
		       WWWDoc.post_data = doc->post_data;
		       WWWDoc.post_content_type = doc->post_content_type;
		       WWWDoc.bookmark = doc->bookmark;
		       WWWDoc.isHEAD = doc->isHEAD;
		       WWWDoc.safe = doc->safe;

		       if (!HTLoadAbsolute(&WWWDoc))
			   return(NOT_FOUND);
		       return(NORMAL);
		    }
#else
		    _statusline(DIRED_DISABLED);
		    sleep(MessageSecs);
		    return(NULLFILE);
#endif /* DIRED_SUPPORT */

		} else if (url_type == LYNXHIST_URL_TYPE) {
		    /*
		     *	'doc' will change to the new file
		     *	if we had a successful LYpop_num(),
		     *	and the return value will be FALSE
		     *	if we had a cancel. - FM
		     */
		    if ((historytarget(doc) == FALSE) ||
			!doc || !doc->address) {
			HTMLSetCharacterHandling(current_char_set);
			return(NOT_FOUND);
		    }

		    /*
		     *	We changed it so reload.
		     */
		    WWWDoc.address = doc->address;
		    WWWDoc.post_data = doc->post_data;
		    WWWDoc.post_content_type = doc->post_content_type;
		    WWWDoc.bookmark = doc->bookmark;
		    WWWDoc.isHEAD = doc->isHEAD;
		    WWWDoc.safe = doc->safe;
#ifndef DONT_TRACK_INTERNAL_LINKS
		    if (doc->internal_link && !reloading) {
			LYinternal_flag = TRUE;
		    }
#endif

		    if (!HTLoadAbsolute(&WWWDoc)) {
			HTMLSetCharacterHandling(current_char_set);
			return(NOT_FOUND);
		    }
		    HTMLSetCharacterHandling(current_char_set);
		    return(NORMAL);

		} else if (url_type == LYNXEXEC_URL_TYPE ||
			   url_type == LYNXPROG_URL_TYPE) {
#ifdef EXEC_LINKS
		    if (no_exec &&
			!exec_ok(HTLoadedDocumentURL(),
				 doc->address+9, ALWAYS_EXEC_PATH)) {
			statusline(EXECUTION_DISABLED);
			sleep(MessageSecs);
		    } else if (no_bookmark_exec &&
			       HTLoadedDocumentBookmark()) {
			statusline(BOOKMARK_EXEC_DISABLED);
			sleep(MessageSecs);
		    } else if (local_exec || (local_exec_on_local_files &&
			       exec_ok(HTLoadedDocumentURL(),
				       doc->address+9, EXEC_PATH))) {

			char *p, addressbuf[1024];

			/*
			 *  Bug puts slash on end if none is in the string.
			 */
			char *last_slash = strrchr(doc->address,'/');
			if (last_slash-doc->address==strlen(doc->address)-1)
			    doc->address[strlen(doc->address)-1] = '\0';

			p = doc->address;
			/*
			 *  Convert '~' to $HOME.
			 */
			if ((cp = strchr(doc->address, '~'))) {
			    strncpy(addressbuf, doc->address, cp-doc->address);
			    addressbuf[cp - doc->address] = '\0';
#ifdef DOSPATH
			    p = HTDOS_wwwName((char *)Home_Dir());
#else
#ifdef VMS
			    p = HTVMS_wwwName((char *)Home_Dir());
#else
			    p = (char *)Home_Dir();
#endif /* VMS */
#endif /* DOSPATH */
			    strcat(addressbuf, p);
			    strcat(addressbuf, cp+1);
			    p = addressbuf;
			}
			/*
			 *  Show URL before executing it.
			 */
			statusline(doc->address);
			sleep(InfoSecs);
			stop_curses();
			/*
			 *  Run the command.
			 */
			if (strstr(p,"//") == p+9)
			    system(p+11);
			else
			    system(p+9);
			if (url_type != LYNXPROG_URL_TYPE) {
			    /*
			     *	Make sure user gets to see screen output.
			     */
#ifndef VMS
			    signal(SIGINT, SIG_IGN);
#endif /* !VMS */
			    printf("\n%s", RETURN_TO_LYNX);
			    fflush(stdout);
			    LYgetch();
#ifdef VMS
			    {
			      extern BOOLEAN HadVMSInterrupt;
			      HadVMSInterrupt = FALSE;
			    }
#endif /* VMS */
			}
			start_curses();
			LYAddVisitedLink(doc);

		     } else {
			char buf[512];

			sprintf(buf,
				EXECUTION_DISABLED_FOR_FILE,
				key_for_func(LYK_OPTIONS));
			_statusline(buf);
			sleep(AlertSecs);
		     }
#else /* no exec_links */
		     _statusline(EXECUTION_NOT_COMPILED);
		     sleep(MessageSecs);
#endif /* EXEC_LINKS */
		     return(NULLFILE);

		} else if (url_type == MAILTO_URL_TYPE) {
		    if (no_mail) {
			_statusline(MAIL_DISABLED);
			sleep(MessageSecs);
		    } else {
			HTParentAnchor *tmpanchor;
			CONST char *title;

			title = "";
			if ((tmpanchor = HTAnchor_parent(
						HTAnchor_findAddress(&WWWDoc)
							)) != NULL) {
			    if (HTAnchor_title(tmpanchor)) {
				title = HTAnchor_title(tmpanchor);
			    }
			}
			cp = (char *)strchr(doc->address,':')+1;
			reply_by_mail(cp,
				      ((HTMainAnchor && !LYUserSpecifiedURL) ?
				       (char *)HTMainAnchor->address :
				       (char *)doc->address),
				      title);
		    }
		    return(NULLFILE);

		/*
		 *  From here on we could have a remote host,
		 *  so check if that's allowed.
		 */
		} else if (local_host_only &&
			   url_type != NEWS_URL_TYPE &&
			   url_type != LYNXKEYMAP_URL_TYPE &&
			   url_type != LYNXIMGMAP_URL_TYPE &&
			   url_type != LYNXCOOKIE_URL_TYPE &&
			   url_type != LYNXCGI_URL_TYPE &&
			   !(LYisLocalHost(doc->address) ||
			     LYisLocalAlias(doc->address))) {
		    statusline(ACCESS_ONLY_LOCALHOST);
		    sleep(MessageSecs);
		    return(NULLFILE);

		/*
		 *  Disable www telnet access if not telnet_ok.
		 */
		} else if (url_type == TELNET_URL_TYPE ||
			   url_type == TN3270_URL_TYPE ||
			   url_type == TELNET_GOPHER_URL_TYPE) {
		    if (!telnet_ok) {
			_statusline(TELNET_DISABLED);
			sleep(MessageSecs);
		    } else if (no_telnet_port && strchr(doc->address+7, ':')) {
			statusline(TELNET_PORT_SPECS_DISABLED);
			sleep(MessageSecs);
		    } else {
			stop_curses();
			HTLoadAbsolute(&WWWDoc);
			start_curses();
			fflush(stdout);
			LYAddVisitedLink(doc);
		    }
		    return(NULLFILE);

		/*
		 *  Disable www news access if not news_ok.
		 */
		} else if (url_type == NEWS_URL_TYPE && !news_ok) {
		    _statusline(NEWS_DISABLED);
		    sleep(MessageSecs);
		    return(NULLFILE);

		} else if (url_type == RLOGIN_URL_TYPE) {
		    if (!rlogin_ok) {
			statusline(RLOGIN_DISABLED);
			sleep(MessageSecs);
		    } else {
			stop_curses();
			HTLoadAbsolute(&WWWDoc);
			fflush(stdout);
			start_curses();
			LYAddVisitedLink(doc);
		    }
		    return(NULLFILE);

		/*
		 *  If its a gopher index type and there isn't a search
		 *  term already attached then do this.  Otherwise
		 *  just load it!
		 */
		} else if (url_type == INDEX_GOPHER_URL_TYPE &&
					strchr(doc->address,'?') == NULL) {
		    int status;
		    /*
		     *	Make sure we don't have a gopher+ escaped tab
		     *	instead of a gopher0 question mark delimiting
		     *	the search term. - FM
		     */
		    if ((cp = strstr(doc->address, "%09")) != NULL) {
			*cp = '\0';
			StrAllocCopy(temp, doc->address);
			cp += 3;
			if (*cp && strncmp(cp, "%09", 3)) {
			    StrAllocCat(temp, "?");
			    StrAllocCat(temp, cp);
			    if ((cp = strstr(temp, "%09")) != NULL) {
				*cp = '\0';
			    }
			}
			StrAllocCopy(doc->address, temp);
			FREE(temp);
			goto Try_Redirected_URL;
		    }
		    /*
		     *	Load it because the do_www_search routine
		     *	uses the base url of the currently loaded
		     *	document :(
		     */
		    if (!HTLoadAbsolute(&WWWDoc))
			return(NOT_FOUND);
		    status = do_www_search(doc);
		    if (status == NULLFILE) {
			LYpop(doc);
			WWWDoc.address = doc->address;
			WWWDoc.post_data = doc->post_data;
			WWWDoc.post_content_type = doc->post_content_type;
			WWWDoc.bookmark = doc->bookmark;
			WWWDoc.isHEAD = doc->isHEAD;
			WWWDoc.safe = doc->safe;
			status = HTLoadAbsolute(&WWWDoc);
		    }
		    return(status);

		} else {

		    if (url_type == FTP_URL_TYPE && !ftp_ok) {
			statusline(FTP_DISABLED);
			sleep(MessageSecs);
			return(NULLFILE);
		    }

		    if (url_type == HTML_GOPHER_URL_TYPE) {
			char *tmp=NULL;
		       /*
			*  If tuple's Path=GET%20/... convert to an http URL.
			*/
			if ((cp=strchr(doc->address+9, '/')) != NULL &&
			   0==strncmp(++cp, "hGET%20/", 8)) {
			    StrAllocCopy(tmp, "http://");
			    if (TRACE)
				fprintf(stderr,
					"getfile: URL '%s'\n",
					doc->address);
			    *cp = '\0';
			    StrAllocCat(tmp, doc->address+9);
			   /*
			    *  If the port is defaulted, it should stay 70.
			    */
			    if (strchr(tmp+6, ':') == NULL) {
				StrAllocCat(tmp, "70/");
				tmp[strlen(tmp)-4] = ':';
			    }
			    if (strlen(cp+7) > 1)
				StrAllocCat(tmp, cp+8);
			    StrAllocCopy(doc->address, tmp);
			    if (TRACE)
				fprintf(stderr, "  changed to '%s'\n",
						doc->address);
			    FREE(tmp);
			    url_type = HTTP_URL_TYPE;
			}
		    }
		    if (url_type == HTTP_URL_TYPE ||
			url_type == HTTPS_URL_TYPE ||
			url_type == FTP_URL_TYPE ||
			url_type == CSO_URL_TYPE)
			fix_http_urls(doc);
		    WWWDoc.address = doc->address;  /* possible reload */
#ifdef DIRED_SUPPORT
		    lynx_edit_mode = FALSE;
#endif /* DIRED_SUPPORT */

		    if (url_type == FILE_URL_TYPE) {
			/*
			 *  If a file URL has a '~' as the lead character
			 *  of its first symbolic element, convert the '~'
			 *  to Home_Dir(), then append the rest of of path,
			 *  if present, skipping "user" if "~user" was
			 *  entered, simplifying, and eliminating any
			 *  residual relative elements. - FM
			 */
			if (((cp = HTParse(doc->address, "",
				   PARSE_PATH+PARSE_ANCHOR+PARSE_PUNCTUATION))
			      != NULL) &&
			    !strncmp(cp, "/~", 2)) {
			    char *cp1 = strstr(doc->address, "/~");
			    char *cp2;

			    if (TRACE)
				fprintf(stderr, "getfile: URL '%s'\n",
						doc->address);
			    *cp1 = '\0';
			    cp1 += 2;
			    StrAllocCopy(temp, doc->address);
#ifdef DOSPATH
			    StrAllocCat(temp, "/");
			    StrAllocCat(temp, HTDOS_wwwName((char *)Home_Dir()));
#else
#ifdef VMS
			    StrAllocCat(temp,
					HTVMS_wwwName((char *)Home_Dir()));
#else
			    StrAllocCat(temp, Home_Dir());
#endif /* VMS */
#endif /* DOSPATH */
			    if ((cp2 = strchr(cp1, '/')) != NULL) {
				LYTrimRelFromAbsPath(cp2);
				StrAllocCat(temp, cp2);
			    }
			    StrAllocCopy(doc->address, temp);
			    FREE(temp);
			    if (TRACE)
				fprintf(stderr, "  changed to '%s'\n",
						doc->address);
			    WWWDoc.address = doc->address;
			}
			FREE(cp);
		    }
		    if (TRACE && LYTraceLogFP == NULL)
			sleep(MessageSecs);
		    user_message(WWW_WAIT_MESSAGE, doc->address);
#ifdef NOTDEFINED
		    sleep(InfoSecs);
#endif /* NOTDEFINED */
		    if (TRACE) {
#ifdef USE_SLANG
			if (LYCursesON) {
			    addstr("*\n");
			    refresh();
			}
#endif /* USE_SLANG */
			fprintf(stderr,"\n");
		    }
		    if ((LYNoRefererHeader == FALSE &&
			 LYNoRefererForThis == FALSE) &&
			(url_type == HTTP_URL_TYPE ||
			 url_type == HTTPS_URL_TYPE) &&
			(cp = strchr(HTLoadedDocumentURL(), '?')) != NULL &&
			strchr(cp, '=') != NULL) {
			/*
			 *  Don't send a Referer header if the URL is
			 *  the reply from a form with method GET, in
			 *  case the content has personal data (e.g.,
			 *  a password or credit card number) which
			 *  would become visible in logs. - FM
			 */
			LYNoRefererForThis = TRUE;
		    }
		    cp = NULL;
		    if (!HTLoadAbsolute(&WWWDoc)) {
			/*
			 *  Check for redirection.
			 */
			if (use_this_url_instead != NULL) {
			    char *pound;

			    if (!is_url(use_this_url_instead)) {
				/*
				 *  The server did not return a complete
				 *  URL in its Location: header, probably
				 *  due to a FORM or other CGI script written
				 *  by someone who doesn't know that the http
				 *  protocol requires that it be a complete
				 *  URL, or using a server which does not treat
				 *  such a redirect string from the script as
				 *  an instruction to resolve it versus the
				 *  initial request, check authentication with
				 *  that URL, and then act on it without
				 *  returning redirection to us.  We'll
				 *  violate the http protocol and resolve it
				 *  ourselves using the URL of the original
				 *  request as the BASE, rather than doing
				 *  the RIGHT thing and returning an invalid
				 *  address message. - FM
				 */
				HTAlert(LOCATION_NOT_ABSOLUTE);
				temp = HTParse(use_this_url_instead,
					       WWWDoc.address,
					       PARSE_ALL);
				if (temp && *temp) {
				    StrAllocCopy(use_this_url_instead, temp);
				}
				FREE(temp);
			    }
			    HTMLSetCharacterHandling(current_char_set);
			    url_type = is_url(use_this_url_instead);
			    if (url_type == LYNXDOWNLOAD_URL_TYPE ||
				url_type == LYNXEXEC_URL_TYPE ||
				url_type == LYNXPROG_URL_TYPE ||
#ifdef DIRED_SUPPORT
				url_type == LYNXDIRED_URL_TYPE ||
#endif /* DIRED_SUPPORT */
				url_type == LYNXPRINT_URL_TYPE ||
				url_type == LYNXHIST_URL_TYPE ||
				url_type == LYNXCOOKIE_URL_TYPE ||
				(LYValidate &&
				 url_type != HTTP_URL_TYPE &&
				 url_type != HTTPS_URL_TYPE) ||
				((no_file_url || no_goto_file) &&
				 url_type == FILE_URL_TYPE) ||
				(no_goto_lynxcgi &&
				 url_type == LYNXCGI_URL_TYPE) ||
				(no_goto_cso &&
				 url_type == CSO_URL_TYPE) ||
				(no_goto_finger &&
				 url_type == FINGER_URL_TYPE) ||
				(no_goto_ftp &&
				 url_type == FTP_URL_TYPE) ||
				(no_goto_gopher &&
				 url_type == GOPHER_URL_TYPE) ||
				(no_goto_http &&
				 url_type == HTTP_URL_TYPE) ||
				(no_goto_https &&
				 url_type == HTTPS_URL_TYPE) ||
				(no_goto_mailto &&
				 url_type == MAILTO_URL_TYPE) ||
				(no_goto_news &&
				 url_type == NEWS_URL_TYPE) ||
				(no_goto_nntp &&
				 url_type == NNTP_URL_TYPE) ||
				(no_goto_rlogin &&
				 url_type == RLOGIN_URL_TYPE) ||
				(no_goto_snews &&
				 url_type == SNEWS_URL_TYPE) ||
				(no_goto_telnet &&
				 url_type == TELNET_URL_TYPE) ||
				(no_goto_tn3270 &&
				 url_type == TN3270_URL_TYPE) ||
				(no_goto_wais &&
				 url_type == WAIS_URL_TYPE)) {
				/*
				 *  Some schemes are not acceptable from
				 *  server redirections. - KW & FM
				 */
				HTAlert(ILLEGAL_REDIRECTION_URL);
				if (LYCursesON) {
				    _user_message(WWW_ILLEGAL_URL_MESSAGE,
						  use_this_url_instead);
				    sleep(AlertSecs);
				} else {
				    fprintf(stderr,
					    "Illegal Redirection URL: %s",
					    use_this_url_instead);
				}
				FREE(use_this_url_instead);
				return(NULLFILE);
			    }
			    if ((pound = strchr(doc->address, '#')) != NULL &&
				strchr(use_this_url_instead, '#') == NULL) {
				/*
				 *  Our requested URL had a fragment
				 *  associated with it, and the redirection
				 *  URL doesn't, so we'll append the fragment
				 *  associated with the original request.  If
				 *  it's bogus for the redirection URL, we'll
				 *  be positioned at the top of that document,
				 *  so there's no harm done. - FM
				 */
				if (TRACE) {
				    fprintf(stderr,
			"getfile: Adding fragment '%s' to redirection URL.\n",
				    pound);
				}
				StrAllocCat(use_this_url_instead, pound);
			    }
			    if (TRACE && LYTraceLogFP == NULL)
				sleep(MessageSecs);
			    _user_message(WWW_USING_MESSAGE,
					  use_this_url_instead);
			    sleep(InfoSecs);
			    if (TRACE)
				fprintf(stderr, "\n");
			    StrAllocCopy(doc->address,
					use_this_url_instead);
			    FREE(use_this_url_instead);
			    if (redirect_post_content == FALSE) {
				/*
				 *  Freeing the content also yields
				 *  a GET request. - FM
				 */
				FREE(doc->post_data);
				FREE(doc->post_content_type);
			    }
			    /*
			     *	Go to top to check for URL's which get
			     *	special handling and/or security checks
			     *	in Lynx. - FM
			     */
			    goto Try_Redirected_URL;
			}
			HTMLSetCharacterHandling(current_char_set);
			return(NOT_FOUND);
		    }

		    lynx_mode = NORMAL_LYNX_MODE;

		    /*
		     *	Some URL's don't actually return a document
		     *	compare doc->address with the document that is
		     *	actually loaded and return NULL if not
		     *	loaded.  If www_search_result is not -1
		     *	then this is a reference to a named anchor
		     *	within the same document.  Do NOT return
		     *	NULL.
		     */
		    {
			char *pound;
			/*
			 *  Check for a #fragment selector.
			 */
			pound = (char *)strchr(doc->address, '#');

			/*
			 *  Check to see if there is a temp
			 *  file waiting for us to download.
			 */
			if (WWW_Download_File) {
			    HTParentAnchor *tmpanchor;
			    char *fname = NULL;

			    HTMLSetCharacterHandling(current_char_set);
			    /*
			     *	Check for a suggested filename from
			     *	the Content-Dispostion header. - FM
			     */
			    if (((tmpanchor = HTAnchor_parent(
						HTAnchor_findAddress(&WWWDoc)
							     )) != NULL) &&
				HTAnchor_SugFname(tmpanchor) != NULL) {
				StrAllocCopy(fname,
					     HTAnchor_SugFname(tmpanchor));
			    } else {
				StrAllocCopy(fname, doc->address);
			    }
			    /*
			     *	Check whether this is a compressed file,
			     *	which we don't uncompress for downloads,
			     *	and adjust any suffix appropriately. - FM
			     */
			    if (tmpanchor != NULL) {
				HTCheckFnameForCompression(&fname, tmpanchor,
							   FALSE);
			    }
			    if (LYdownload_options(&fname,
						   WWW_Download_File) < 0) {
				FREE(fname);
				return(NOT_FOUND);
			    }
			    LYAddVisitedLink(doc);
			    StrAllocCopy(doc->address, fname);
			    FREE(fname);
			    doc->internal_link = FALSE;
			    WWWDoc.address = doc->address;
			    FREE(doc->post_data);
			    WWWDoc.post_data = NULL;
			    FREE(doc->post_content_type);
			    WWWDoc.post_content_type = NULL;
			    WWWDoc.bookmark = doc->bookmark = FALSE;
			    WWWDoc.isHEAD = doc->isHEAD = FALSE;
			    WWWDoc.safe = doc->safe = FALSE;
			    HTOutputFormat = WWW_PRESENT;
			    if (!HTLoadAbsolute(&WWWDoc))
				return(NOT_FOUND);
			    else
				return(NORMAL);

			} else if (pound == NULL &&
				   /*
				    *  HTAnchor hash-table searches are now
				    *  case-sensitive (hopefully, without
				    *  anchor deletion problems), so this
				    *  is too. - FM
				    */
				   (strcmp(doc->address,
					   HTLoadedDocumentURL()) ||
				   /*
				    *  Also check the post_data elements. - FM
				    */
				   strcmp((doc->post_data ?
					   doc->post_data : ""),
					  HTLoadedDocumentPost_data()) ||
				   /*
				    *  Also check the isHEAD element. - FM
				    */
				   doc->isHEAD != HTLoadedDocumentIsHEAD())) {
			    HTMLSetCharacterHandling(current_char_set);
			    /*
			     *	Nothing needed to be shown.
			     */
			    LYAddVisitedLink(doc);
			    return(NULLFILE);

			} else {
			/*
			 *  May set www_search_result.
			 */
			    if (pound != NULL)
				HTFindPoundSelector(pound+1);
			    HTMLSetCharacterHandling(current_char_set);
			    return(NORMAL);
			}
		    }
		}
	  } else {
	      if (TRACE && LYTraceLogFP == NULL)
		  sleep(MessageSecs);
	      _user_message(WWW_BAD_ADDR_MESSAGE, doc->address);
	      if (TRACE)
		  fprintf(stderr,"\n");
	      sleep(MessageSecs);
	      return(NULLFILE);
	  }
}

/*
 *  The user wants to select a link or page by number.
 *  If follow_link_number returns DO_LINK_STUFF do_link
 *   will be run immediately following its execution.
 *  If follow_link_number returns DO_GOTOLINK_STUFF
 *   it has updated the passed in doc for positioning on a link.
 *  If follow_link_number returns DO_GOTOPAGE_STUFF
 *   it has set doc->line to the top line of the desired page
 *   for displaying that page.
 *  If follow_link_number returns PRINT_ERROR an error message
 *   will be given to the user.
 *  If follow_link_number returns DO_FORMS_STUFF some forms stuff
 *   will be done. (Not yet implemented.)
 *  If follow_link_number returns DO_NOTHING nothing special
 *   will run after it.
 */
PUBLIC int follow_link_number ARGS4(
	int,		c,
	int,		cur,
	document *,	doc,
	int *,		num)
{
    char temp[120];
    int new_top, new_link;
    BOOL want_go;

    temp[0] = c;
    temp[1] = '\0';
    *num = -1;
    _statusline(FOLLOW_LINK_NUMBER);
    /*
     *	Get the number, possibly with a letter suffix, from the user.
     */
    if (LYgetstr(temp, VISIBLE, sizeof(temp), NORECALL) < 0 || *temp == 0) {
	_statusline(CANCELLED);
	sleep(InfoSecs);
	return(DO_NOTHING);
    }
    *num = atoi(temp);

    /*
     *	Check if we had a 'p' or 'P' following the number as
     *	a flag for displaying the page with that number. - FM
     */
    if (strchr(temp, 'p') != NULL || strchr(temp, 'P') != NULL) {
	int nlines = HText_getNumOfLines();
	int npages = ((nlines + 1) > display_lines) ?
		(((nlines + 1) + (display_lines - 1))/(display_lines))
						    : 1;
	if (*num < 1)
	    *num = 1;
	doc->line = (npages <= 1) ?
				1 :
		((*num <= npages) ? (((*num - 1) * display_lines) + 1)
				  : (((npages - 1) * display_lines) + 1));
	return(DO_GOTOPAGE_STUFF);
    }

    /*
     *	Check if we want to make the link corresponding to the
     *	number the current link, rather than ACTIVATE-ing it.
     */
    want_go = (strchr(temp, 'g') != NULL || strchr(temp, 'G') != NULL);

   /*
    *  If we have a valid number, act on it.
    */
   if (*num > 0) {
	int info;
	/*
	 *  Get the lname, and hightext, directly from www
	 *  structures and add it to the cur link so that
	 *  we can pass it transparently on to getfile(),
	 *  and load new_top and new_link if we instead want
	 *  to make the link number current.  These things
	 *  are done so that a link can be selected anywhere
	 *  in the current document, whether it is displayed
	 *  on the screen or not!
	 */
	if ((info = HTGetLinkInfo(*num,
				  want_go,
				  &new_top,
				  &new_link,
				  &links[cur].hightext,
			  &links[cur].lname)) == WWW_INTERN_LINK_TYPE) {
	    links[cur].type = WWW_INTERN_LINK_TYPE;
	    return(DO_LINK_STUFF);
	} else if (info == LINK_LINE_FOUND) {
	    doc->line = new_top + 1;
	    doc->link = new_link;
	    return(DO_GOTOLINK_STUFF);
	} else if (info) {
	    links[cur].type = WWW_LINK_TYPE;
	    return(DO_LINK_STUFF);
	} else {
	    return(PRINT_ERROR);
	}
    } else {
	return(PRINT_ERROR);
    }
}

#if defined(EXEC_LINKS) || defined(LYNXCGI_LINKS)

struct trust {
	char *src;
	char *path;
	int type;
	struct trust *next;
};

static struct trust trusted_exec_default = {
  "file://localhost/",	"",	EXEC_PATH,		NULL
};
static struct trust always_trusted_exec_default = {
  "none",		"",	ALWAYS_EXEC_PATH,	NULL
};
static struct trust trusted_cgi_default = {
  "",			"",	CGI_PATH,		NULL
};

static struct trust *trusted_exec = &trusted_exec_default;
static struct trust *always_trusted_exec = &always_trusted_exec_default;
static struct trust *trusted_cgi = &trusted_cgi_default;

PRIVATE void LYTrusted_free NOARGS
{
    struct trust *cur;
    struct trust *next;

    if (trusted_exec != &trusted_exec_default) {
	cur = trusted_exec;
	while (cur) {
	    FREE(cur->src);
	    FREE(cur->path);
	    next = cur->next;
	    FREE(cur);
	    cur = next;
	}
    }

    if (always_trusted_exec != &always_trusted_exec_default) {
	cur = always_trusted_exec;
	while (cur) {
	    FREE(cur->src);
	    FREE(cur->path);
	    next = cur->next;
	    FREE(cur);
	    cur = next;
	}
    }

    if (trusted_cgi != &trusted_cgi_default) {
	cur = trusted_cgi;
	while (cur) {
	    FREE(cur->src);
	    FREE(cur->path);
	    next = cur->next;
	    FREE(cur);
	    cur = next;
	}
    }

    return;
}

PUBLIC void add_trusted ARGS2(
	char *, 	str,
	int,		type)
{
    struct trust *tp;
    char *path;
    char *src = str;
    int Type = type;
    static BOOLEAN first = TRUE;

    if (!src)
	return;
    if (first) {
	atexit(LYTrusted_free);
	first = FALSE;
    }

    path = strchr(src, '\t');
    if (path)
	*path++ = '\0';
    else
	path = "";

    tp = (struct trust *)malloc(sizeof(*tp));
    if (tp == NULL)
	outofmem(__FILE__, "add_trusted");
    tp->src = NULL;
    tp->path = NULL;
    tp->type = Type;
    StrAllocCopy(tp->src, src);
    StrAllocCopy(tp->path, path);
    if (Type == EXEC_PATH) {
	if (trusted_exec == &trusted_exec_default)
	    tp->next = NULL;
	else
	    tp->next = trusted_exec;
	trusted_exec = tp;
    } else if (Type == ALWAYS_EXEC_PATH) {
	if (always_trusted_exec == &always_trusted_exec_default)
	    tp->next = NULL;
	else
	    tp->next = always_trusted_exec;
	always_trusted_exec = tp;
    } else if (Type == CGI_PATH) {
	if (trusted_cgi == &trusted_cgi_default)
	    tp->next = NULL;
	else
	    tp->next = trusted_cgi;
	trusted_cgi = tp;
    }
}

/*
 *  Check to see if the supplied paths is allowed to be executed.
 */
PUBLIC BOOLEAN exec_ok ARGS3(
	CONST char *,	source,
	CONST char *,	link,
	int,		type)
{
    struct trust *tp;
    CONST char *cp;
    int Type = type;

    /*
     *	Always OK if it is a jump file shortcut.
     */
    if (LYJumpFileURL)
	return TRUE;

    /*
     *	Choose the trust structure based on the type.
     */
    if (Type == EXEC_PATH) {
	tp = trusted_exec;
    } else if (Type == ALWAYS_EXEC_PATH) {
	tp = always_trusted_exec;
    } else if (Type == CGI_PATH) {
	tp = trusted_cgi;
    } else {
	HTAlert(MALFORMED_EXEC_REQUEST);
	return FALSE;
    }

#ifdef VMS
    /*
     *	Security: reject on relative path.
     */
    if ((cp = strchr(link, '[')) != NULL) {
	char *cp1;
	if (((cp1 = strchr(cp, '-')) != NULL) &&
	    strchr(cp1, ']') != NULL) {
	    while (cp1[1] == '-')
		cp1++;
	    if (cp1[1] == ']' ||
		cp1[1] == '.') {
		HTAlert(RELPATH_IN_EXEC_LINK);
		return FALSE;
	    }
	}
    }
#else
    /*
     *	Security: reject on relative path.
     */
    if (strstr(link, "../") != NULL) {
	HTAlert(RELPATH_IN_EXEC_LINK);
	return FALSE;
    }

    /*
     *	Security: reject on strange character.
     */
    for (cp = link; *cp != '\0'; cp++) {
	if (!isalnum(*cp) &&
	    *cp != '_' && *cp != '-' && *cp != ' ' &&
	    *cp != ':' && *cp != '.' && *cp != '/' &&
	    *cp != '@' && *cp != '~' && *cp != '$' &&
	    *cp != '&' && *cp != '+' && *cp != '=' &&
	    *cp != '\t') {
	    char buf[128];

	    sprintf(buf,
		    BADCHAR_IN_EXEC_LINK,
		    *cp);
	    HTAlert(buf);
	    return FALSE;
	}
    }
#endif /* VMS */

check_tp_for_entry:
    while (tp) {
	if (tp->type == Type) {
	    char CONST *command = link;

	    if (strstr(command,"//") == link) {
		command += 2;
	    }
#ifdef VMS
	    if (strncasecomp(source, tp->src, strlen(tp->src)) == 0 &&
		strncasecomp(command, tp->path, strlen(tp->path)) == 0)
#else
	    if (strncmp(source, tp->src, strlen(tp->src)) == 0 &&
		strncmp(command, tp->path, strlen(tp->path)) == 0)
#endif /* VMS */
		return TRUE;
	}
	tp = tp->next;
    }
    if (Type == EXEC_PATH &&
	always_trusted_exec != &always_trusted_exec_default) {
	Type = ALWAYS_EXEC_PATH;
	tp = always_trusted_exec;
	goto check_tp_for_entry;
    }
    if (!(no_exec && type == ALWAYS_EXEC_PATH))
	HTAlert(BADLOCPATH_IN_EXEC_LINK);
    return FALSE;
}
#endif /* EXEC_LINKS || LYNXCGI_LINKS */

PRIVATE int fix_http_urls ARGS1(
	document *,	doc)
{
    char *slash;

    /*
     *	If it's an ftp URL with a trailing slash, trim it off.
     */
    if (!strncmp(doc->address, "ftp", 3) &&
	doc->address[strlen(doc->address)-1] == '/') {
	char * proxy;
	char *path = HTParse(doc->address, "", PARSE_PATH|PARSE_PUNCTUATION);

	/*
	 *  If the path is a lone slash, we're done. - FM
	 */
	if (path) {
	    if (path[0] == '/' && path[1] == '\0') {
		FREE(path);
		return 0;
	    }
	    FREE(path);
	}

	/*
	 *  If we're proxying ftp, don't trim anything. - KW
	 */
	if (((proxy = (char *)getenv("ftp_proxy")) != NULL) &&
	    *proxy != '\0' && !override_proxy(doc->address))
	    return 0;

	/*
	 *  If we get to here, trim the trailing slash. - FM
	 */
	if (TRACE)
	    fprintf(stderr, "fix_http_urls: URL '%s'\n", doc->address);
	doc->address[strlen(doc->address)-1] = '\0';
	if (TRACE) {
	    fprintf(stderr, "        changed to '%s'\n", doc->address);
	    if (!LYTraceLogFP)
		sleep(MessageSecs);
	}
    }

    /*
     *	If there isn't a slash besides the two at the beginning, append one.
     */
    if ((slash = strrchr(doc->address, '/')) != NULL) {
	if (*(slash-1) != '/' || *(slash-2) != ':') {
	    return(0);
	}
    }
    if (TRACE)
	fprintf(stderr, "fix_http_urls: URL '%s'\n", doc->address);
    StrAllocCat(doc->address, "/");
    if (TRACE) {
	fprintf(stderr, "        changed to '%s'\n",doc->address);
	if (!LYTraceLogFP)
	    sleep(MessageSecs);
    }

    return(1);
}
