#include <HTUtils.h>
#include <HTFile.h>
#include <HTParse.h>
#include <HTAlert.h>
#include <HTTP.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYStructs.h>
#include <LYGlobalDefs.h>
#include <LYShowInfo.h>
#include <LYCharUtils.h>
#include <GridText.h>
#include <LYReadCFG.h>

#include <LYLeaks.h>

#ifdef DIRED_SUPPORT
#include <HTAAProt.h>
#include <time.h>
#include <LYLocal.h>
#endif /* DIRED_SUPPORT */

#define ADVANCED_INFO 1		/* to get more info in advanced mode */

/*
 *  Showinfo prints a page of info about the current file and the link
 *  that the cursor is on.
 */

PUBLIC int showinfo ARGS4(
	document *,	doc,
	int,		size_of_file,
	document *,	newdoc,
	char *, 	owner_address)
{
    static char tempfile[LY_MAXPATH];
    int url_type;
    FILE *fp0;
    char *Address = NULL, *Title = NULL;
    char *name;
    CONST char *cp;
#ifdef ADVANCED_INFO
    BOOLEAN LYInfoAdvanced = (user_mode == ADVANCED_MODE);
#endif

#ifdef DIRED_SUPPORT
    char temp[LY_MAXPATH];
    struct stat dir_info;
#endif /* DIRED_SUPPORT */

    LYRemoveTemp(tempfile);
    if ((fp0 = LYOpenTemp (tempfile, HTML_SUFFIX, "w")) == 0) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }

    /*
     *	Point the address pointer at this Url
     */
    LYLocalFileToURL(&newdoc->address, tempfile);

    if (nlinks > 0 && links[doc->link].lname != NULL &&
	(url_type = is_url(links[doc->link].lname)) != 0 &&
	(url_type == LYNXEXEC_URL_TYPE ||
	 url_type == LYNXPROG_URL_TYPE)) {
	char *last_slash = strrchr(links[doc->link].lname,'/');
	int next_to_last = strlen(links[doc->link].lname) - 1;

	if ((last_slash - links[doc->link].lname) == next_to_last) {
	    links[doc->link].lname[next_to_last] = '\0';
	}
    }

    fprintf(fp0, "<html>\n<head>\n");
    LYAddMETAcharsetToFD(fp0, -1);
    fprintf(fp0, "<title>%s</title>\n</head>\n<body>\n",
		 SHOWINFO_TITLE);

    fprintf(fp0, "<h1>%s %s (%.*s) (<a href=\"%s\">%s</a>)",
		 LYNX_NAME, LYNX_VERSION,
		 LYNX_DATE_LEN,
		 (LYNX_RELEASE ? LYNX_RELEASE_DATE : &LYNX_DATE[LYNX_DATE_OFF]),
		 (LYNX_RELEASE ? LYNX_WWW_HOME     : LYNX_WWW_DIST),
		 (LYNX_RELEASE ? REL_VERSION       : DEV_VERSION) );

    if (!LYRestricted) {
#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_INFO)
	fprintf(fp0, " - <a href=\"LYNXCOMPILEOPTS:\">%s</a>\n",
		COMPILE_OPT_SEGMENT);
#else
	fprintf(fp0, " - <a href=\"LYNXCFG:\">%s lynx.cfg</a>\n",
		YOUR_SEGMENT);
#endif
    }
    fprintf(fp0, "</h1>\n");  /* don't forget to close <h1> */


#ifdef DIRED_SUPPORT
    if (lynx_edit_mode && nlinks > 0) {
	char *s;

	fprintf(fp0, "<pre>\n");
	fprintf(fp0, "\n%s\n\n", gettext("Directory that you are currently viewing"));

	s = HTfullURL_toFile(doc->address);
	strcpy(temp, s);
	FREE(s);

	fprintf(fp0, "   <em>%4s</em>  %s\n", gettext("Name:"), temp);
	fprintf(fp0, "   <em>%4s</em>  %s\n", gettext("URL:"), doc->address);

	s = HTfullURL_toFile(links[doc->link].lname);
	strcpy(temp, s);
	FREE(s);

	if (lstat(temp, &dir_info) == -1) {
	    CTRACE(tfp, "lstat(%s) failed, errno=%d\n", temp, errno);
	    HTAlert(CURRENT_LINK_STATUS_FAILED);
	} else {
	    char modes[80];
	    if (S_ISDIR(dir_info.st_mode)) {
		fprintf(fp0, "\n%s\n\n",
			gettext("Directory that you have currently selected"));
	    } else if (S_ISREG(dir_info.st_mode)) {
		fprintf(fp0, "\n%s\n\n",
			gettext("File that you have currently selected"));
#ifdef S_IFLNK
	    } else if (S_ISLNK(dir_info.st_mode)) {
		fprintf(fp0, "\n%s\n\n",
			gettext("Symbolic link that you have currently selected"));
#endif
	    } else {
		fprintf(fp0, "\n%s\n\n",
			gettext("Item that you have currently selected"));
	    }
	    fprintf(fp0, "       <em>%s</em>  %s\n", gettext("Full name:"), temp);
#ifdef S_IFLNK
	    if (S_ISLNK(dir_info.st_mode)) {
		char buf[1025];
		int buf_size;

		if ((buf_size = readlink(temp, buf, sizeof(buf)-1)) != -1) {
		    buf[buf_size] = '\0';
		} else {
		    strcpy(buf, gettext("Unable to follow link"));
		}
		fprintf(fp0, "  <em>%s</em>  %s\n", gettext("Points to file:"), buf);
	    }
#endif
	    name = HTAA_UidToName(dir_info.st_uid);
	    if (*name)
		fprintf(fp0, "   <em>%s</em>  %s\n", gettext("Name of owner"), name);
	    name = HTAA_GidToName (dir_info.st_gid);
	    if (*name)
		fprintf(fp0, "      <em>%s</em>  %s\n", gettext("Group name:"), name);
	    if (S_ISREG(dir_info.st_mode)) {
		fprintf(fp0, "       <em>%s</em>  %ld (bytes)\n",
			gettext("File size:"), (long)dir_info.st_size);
	    }
	    /*
	     *	Include date and time information.
	     */
	    cp = ctime(&dir_info.st_ctime);
	    fprintf(fp0, "   <em>%s</em>  %s", cp, gettext("Creation date:"));

	    cp = ctime(&dir_info.st_mtime);
	    fprintf(fp0, "   <em>%s</em>  %s", cp, gettext("Last modified:"));

	    cp = ctime(&dir_info.st_atime);
	    fprintf(fp0, "   <em>%s</em>  %s\n", cp, gettext("Last accessed:"));

	    fprintf(fp0, "   %s\n", gettext("Access Permissions"));
	    fprintf(fp0, "      <em>%s</em>  ", gettext("Owner:"));
	    modes[0] = '\0';
	    modes[1] = '\0';   /* In case there are no permissions */
	    modes[2] = '\0';
	    if ((dir_info.st_mode & S_IRUSR))
		strcat(modes, ", read");
	    if ((dir_info.st_mode & S_IWUSR))
		strcat(modes, ", write");
	    if ((dir_info.st_mode & S_IXUSR)) {
		if (S_ISDIR(dir_info.st_mode))
		    strcat(modes, ", search");
		else {
		    strcat(modes, ", execute");
		    if ((dir_info.st_mode & S_ISUID))
			strcat(modes, ", setuid");
		}
	    }
	    fprintf(fp0, "%s\n", (char *)&modes[2]); /* Skip leading ', ' */

	    fprintf(fp0, "      <em>Group:</em>  ");
	    modes[0] = '\0';
	    modes[1] = '\0';   /* In case there are no permissions */
	    modes[2] = '\0';
	    if ((dir_info.st_mode & S_IRGRP))
		strcat(modes, ", read");
	    if ((dir_info.st_mode & S_IWGRP))
		strcat(modes, ", write");
	    if ((dir_info.st_mode & S_IXGRP)) {
		if (S_ISDIR(dir_info.st_mode))
		    strcat(modes, ", search");
		else {
		    strcat(modes, ", execute");
		    if ((dir_info.st_mode & S_ISGID))
			strcat(modes, ", setgid");
		}
	    }
	    fprintf(fp0, "%s\n", (char *)&modes[2]);  /* Skip leading ', ' */

	    fprintf(fp0, "      <em>World:</em>  ");
	    modes[0] = '\0';
	    modes[1] = '\0';   /* In case there are no permissions */
	    modes[2] = '\0';
	    if ((dir_info.st_mode & S_IROTH))
		strcat(modes, ", read");
	    if ((dir_info.st_mode & S_IWOTH))
		strcat(modes, ", write");
	    if ((dir_info.st_mode & S_IXOTH)) {
		if (S_ISDIR(dir_info.st_mode))
		    strcat(modes, ", search");
		else {
		    strcat(modes, ", execute");
#ifdef S_ISVTX
		    if ((dir_info.st_mode & S_ISVTX))
			strcat(modes, ", sticky");
#endif
		}
	    }
	    fprintf(fp0, "%s\n", (char *)&modes[2]);  /* Skip leading ', ' */
	}
	fprintf(fp0,"</pre>\n");
    } else {
#endif /* DIRED_SUPPORT */

    fprintf(fp0, "<h2>%s</h2>\n<dl compact>",
	    gettext("File that you are currently viewing"));

    StrAllocCopy(Title, doc->title);
    LYEntify(&Title, TRUE);
    fprintf(fp0, "<dt><em>%s</em> %s%s\n",
		 gettext("Linkname:"),
		 Title, (doc->isHEAD ? " (HEAD)" : ""));

    StrAllocCopy(Address, doc->address);
    LYEntify(&Address, TRUE);
    fprintf(fp0,
	    "<dt>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<em>URL:</em> %s\n", Address);

    if (HTLoadedDocumentCharset()) {
	fprintf(fp0, "<dt><em>&nbsp;%s</em> %s\n",
		     gettext("Charset:"),
		     HTLoadedDocumentCharset());
    } else {
      LYUCcharset * p_in = HTAnchor_getUCInfoStage(HTMainAnchor,
							     UCT_STAGE_PARSER);
      if (!p_in || !(p_in->MIMEname) || !*(p_in->MIMEname) ||
	   HTAnchor_getUCLYhndl(HTMainAnchor, UCT_STAGE_PARSER) < 0) {
	   p_in = HTAnchor_getUCInfoStage(HTMainAnchor, UCT_STAGE_MIME);
      }
      if (p_in && p_in->MIMEname && *(p_in->MIMEname) &&
	  HTAnchor_getUCLYhndl(HTMainAnchor, UCT_STAGE_MIME) >= 0) {
	fprintf(fp0, "<dt><em>&nbsp;%s</em> %s (assumed)\n",
		     gettext("Charset:"),
		     p_in->MIMEname);
      }
    }

    if ((cp = HText_getServer()) != NULL && *cp != '\0')
	fprintf(fp0, "<dt><em>&nbsp;&nbsp;%s</em> %s\n", gettext("Server:"), cp);

    if ((cp = HText_getDate()) != NULL && *cp != '\0')
	fprintf(fp0, "<dt><em>&nbsp;&nbsp;&nbsp;&nbsp;%s</em> %s\n", gettext("Date:"), cp);

    if ((cp = HText_getLastModified()) != NULL && *cp != '\0')
	fprintf(fp0, "<dt><em>%s</em> %s\n", gettext("Last Mod:"), cp);

#ifdef ADVANCED_INFO
    if (LYInfoAdvanced) {
	if (HTMainAnchor && HTMainAnchor->expires) {
	    fprintf(fp0, "<dt><em>%s</em> %s\n",
		    gettext("&nbsp;Expires:"), HTMainAnchor->expires);
	}
	if (HTMainAnchor && HTMainAnchor->cache_control) {
	    fprintf(fp0, "<dt><em>%s</em> %s\n",
		    gettext("Cache-Control:"), HTMainAnchor->cache_control);
	}
	if (HTMainAnchor && HTMainAnchor->content_length > 0) {
	    fprintf(fp0, "<dt><em>%s</em> %d %s\n",
		    gettext("Content-Length:"),
		    HTMainAnchor->content_length, gettext("bytes"));
	}
	if (HTMainAnchor && HTMainAnchor->content_language) {
	    fprintf(fp0, "<dt><em>%s</em> %s\n",
		    gettext("Language:"), HTMainAnchor->content_language);
	}
    }
#endif /* ADVANCED_INFO */

    if (doc->post_data) {
	fprintf(fp0, "<dt><em>%s</em> <xmp>%s</xmp>\n",
		gettext("Post Data:"), doc->post_data);
	fprintf(fp0, "<dt><em>%s</em> %s\n",
		gettext("Post Content Type:"), doc->post_content_type);
    }

    if (owner_address) {
	StrAllocCopy(Address, owner_address);
	LYEntify(&Address, TRUE);
    } else {
	StrAllocCopy(Address, NO_NOTHING);
    }
    fprintf(fp0, "<dt><em>%s</em> %s\n", gettext("Owner(s):"), Address);

    fprintf(fp0, "<dt>&nbsp;&nbsp;&nbsp;&nbsp;<em>%s</em> %d %s\n",
	    gettext("size:"), size_of_file, gettext("lines"));

    fprintf(fp0, "<dt>&nbsp;&nbsp;&nbsp;&nbsp;<em>%s</em> %s%s%s",
		 gettext("mode:"),
		 (lynx_mode == FORMS_LYNX_MODE ?
				  gettext("forms mode") :
		  HTisDocumentSource() ?
				  gettext("source") : gettext("normal")),
		 (doc->safe ? gettext(", safe") : ""),
		 (doc->internal_link ? gettext(", internal link") : "")
	    );
#ifdef ADVANCED_INFO
    if (LYInfoAdvanced) {
	fprintf(fp0, "%s%s%s\n",
		(HText_hasNoCacheSet(HTMainText) ?
				  gettext(", no-cache") : ""),
		(HTAnchor_isISMAPScript((HTAnchor *)HTMainAnchor) ?
				  gettext(", ISMAP script") : ""),
		(doc->bookmark ?
				  gettext(", bookmark file") : "")
	    );
    }
#endif /* ADVANCED_INFO */

    fprintf(fp0, "\n</dl>\n");  /* end of list */

    if (nlinks > 0) {
	fprintf(fp0, "<h2>%s</h2>\n<dl compact>",
		gettext("Link that you currently have selected"));
	StrAllocCopy(Title, links[doc->link].hightext);
	LYEntify(&Title, TRUE);
	fprintf(fp0, "<dt><em>%s</em> %s\n",
		gettext("Linkname:"),
		Title);
	if (lynx_mode == FORMS_LYNX_MODE &&
	    links[doc->link].type == WWW_FORM_LINK_TYPE) {
	    if (links[doc->link].form->submit_method) {
		int method = links[doc->link].form->submit_method;
		char *enctype = links[doc->link].form->submit_enctype;

		fprintf(fp0, "<dt>&nbsp;&nbsp;<em>%s</em> %s\n",
			     gettext("Method:"),
			     (method == URL_POST_METHOD) ? "POST" :
			     (method == URL_MAIL_METHOD) ? "(email)" :
							   "GET");
		fprintf(fp0, "<dt>&nbsp;<em>%s</em> %s\n",
			     gettext("Enctype:"),
			     (enctype &&
			      *enctype ?
			       enctype : "application/x-www-form-urlencoded"));
	    }
	    if (links[doc->link].form->submit_action) {
		StrAllocCopy(Address, links[doc->link].form->submit_action);
		LYEntify(&Address, TRUE);
		fprintf(fp0, "<dt>&nbsp;&nbsp;<em>Action:</em> %s\n", Address);
	    }
	    if (!(links[doc->link].form->submit_method &&
		  links[doc->link].form->submit_action)) {
		fprintf(fp0, "<dt>&nbsp;%s\n", gettext("(Form field)"));
	    }
	} else {
	    if (links[doc->link].lname) {
		StrAllocCopy(Title, links[doc->link].lname);
		LYEntify(&Title, TRUE);
	    } else {
		StrAllocCopy(Title, "");
	    }
	    fprintf(fp0,
	       "<dt>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<em>URL:</em> %s\n", Title);
	}
	fprintf(fp0, "</dl>\n");  /* end of list */

    } else
	fprintf(fp0, "<h2>%s</h2>", gettext("No Links on the current page"));

#ifdef DIRED_SUPPORT
    }
#endif /* DIRED_SUPPORT */
    EndInternalPage(fp0);

    refresh();

    LYCloseTemp(tempfile);
    FREE(Address);
    FREE(Title);

    return(0);
}
