#include "HTUtils.h"
#include "tcp.h"
#include "HTParse.h"
#include "HTAlert.h"
#include "HTTP.h"
#include "LYCurses.h"
#include "LYStrings.h"
#include "LYUtils.h"
#include "LYStructs.h"
#include "LYGlobalDefs.h"
#include "LYShowInfo.h"
#include "LYSignal.h"
#include "LYCharUtils.h"
#include "GridText.h"

#include "LYLeaks.h"

#ifdef DIRED_SUPPORT
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include "LYLocal.h"
#endif /* DIRED_SUPPORT */

#define FREE(x) if (x) {free(x); x = NULL;}

/* 
 *  Showinfo prints a page of info about the current file and the link
 *  that the cursor is on.
 */
	     
PUBLIC int showinfo ARGS4(
	document *,	doc,
	int,		size_of_file,
	document *,	newdoc,
	char *,		owner_address)
{
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    static char info_url[256];
    int url_type;
    FILE *fp0;
    char *Address = NULL, *Title = NULL;
    char *cp;

#ifdef DIRED_SUPPORT
    char temp[300];
    struct stat dir_info;
    struct passwd *pw;
    struct group *grp;
#endif /* DIRED_SUPPORT */
    if (first) {
        tempname(tempfile, NEW_FILE);
	/*
	 *  Make the temporary file a URL now.
	 */
#if defined (VMS) || defined (DOSPATH)
	sprintf(info_url, "file://localhost/%s", tempfile);
#else
	sprintf(info_url, "file://localhost%s", tempfile);
#endif /* VMS */
	first = FALSE;
#ifdef VMS
    } else {
        remove(tempfile);   /* Remove duplicates on VMS. */
#endif /* VMS */
    }

    if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
        HTAlert(CANNOT_OPEN_TEMP);
        return(0);
    }

    /*
     *  Point the address pointer at this Url
     */
    StrAllocCopy(newdoc->address, info_url);

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

    fprintf(fp0, "<head>\n");
    LYAddMETAcharsetToFD(fp0, -1);
    fprintf(fp0, "<title>%s</title>\n</head>\n<body>\n",
		 SHOWINFO_TITLE);
    fprintf(fp0,"<h1>You have reached the Information Page</h1>\n");
    fprintf(fp0,"<h2>%s Version %s</h2>\n", LYNX_NAME, LYNX_VERSION);

#ifdef DIRED_SUPPORT
    if (lynx_edit_mode && nlinks > 0) {
	fprintf(fp0,
	 "<h2>Directory that you are currently viewing</h2>\n<pre>");

	cp = doc->address;
	if (!strncmp(cp, "file://localhost", 16)) 
	    cp += 16;
	else if (!strncmp(cp, "file:", 5))
	    cp += 5;
	strcpy(temp, cp);
	HTUnEscape(temp);

	fprintf(fp0,"   <em>Name:</em>  %s\n", temp);
	fprintf(fp0,"   <em> URL:</em>  %s\n", doc->address);

	cp = links[doc->link].lname;
	if (!strncmp(cp, "file://localhost", 16)) 
	    cp += 16;
	else if (!strncmp(cp, "file:", 5))
	    cp += 5;
	strcpy(temp, cp);
	HTUnEscape(temp);
	if (lstat(temp, &dir_info) == -1) {
	    _statusline(CURRENT_LINK_STATUS_FAILED);
	    sleep(AlertSecs);
	} else {
	    char modes[80];
	    if (((dir_info.st_mode) & S_IFMT) == S_IFDIR) {
		fprintf(fp0,
		 "\nDirectory that you have currently selected\n\n");
	    } else if (((dir_info.st_mode) & S_IFMT) == S_IFREG) {
		fprintf(fp0, 
		      "\nFile that you have currently selected\n\n");
#ifdef S_IFLNK
	    } else if (((dir_info.st_mode) & S_IFMT) == S_IFLNK) {
		fprintf(fp0,
	     "\nSymbolic link that you have currently selected\n\n");
#endif
	    } else {
		fprintf(fp0,
		      "\nItem that you have currently selected\n\n");
	    }
	    fprintf(fp0,"       <em>Full name:</em>  %s\n", temp);
#ifdef S_IFLNK
	    if (((dir_info.st_mode) & S_IFMT) == S_IFLNK) {
		char buf[1025];
		int buf_size;

		if ((buf_size = readlink(temp, buf, sizeof(buf)-1)) != -1) {
		    buf[buf_size] = '\0';
		} else {
		    strcpy(buf, "Unable to follow link");
		}
		fprintf(fp0, "  <em>Points to file:</em>  %s\n", buf);
	    }
#endif
	    pw = getpwuid(dir_info.st_uid);
	    if (pw)
	        fprintf(fp0, "   <em>Name of owner:</em>  %s\n", pw->pw_name);
	    grp = getgrgid(dir_info.st_gid);
	    if (grp && grp->gr_name)
	        fprintf(fp0, "      <em>Group name:</em>  %s\n", grp->gr_name);
	    if (((dir_info.st_mode) & S_IFMT) == S_IFREG) {
		sprintf(temp, "       <em>File size:</em>  %ld (bytes)\n",
		 	      (long)dir_info.st_size);
		fprintf(fp0, "%s", temp);
	    }
	    /*
	     *  Include date and time information.
	     */
	    cp = ctime(&dir_info.st_ctime);
	    fprintf(fp0, "   <em>Creation date:</em>  %s", cp);

	    cp = ctime(&dir_info.st_mtime);	      
	    fprintf(fp0, "   <em>Last modified:</em>  %s", cp);

	    cp = ctime(&dir_info.st_atime);
	    fprintf(fp0, "   <em>Last accessed:</em>  %s\n", cp);

	    fprintf(fp0, "   <em>Access Permissions</em>\n");
	    fprintf(fp0, "      <em>Owner:</em>  ");
	    modes[0] = '\0';
	    modes[1] = '\0';   /* In case there are no permissions */
	    modes[2] = '\0';
	    if ((dir_info.st_mode & S_IRUSR))
		strcat(modes, ", read");
	    if ((dir_info.st_mode & S_IWUSR))
		strcat(modes, ", write");
	    if ((dir_info.st_mode & S_IXUSR)) {
		if (((dir_info.st_mode) & S_IFMT) == S_IFDIR) 
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
		if (((dir_info.st_mode) & S_IFMT) == S_IFDIR) 
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
		if (((dir_info.st_mode) & S_IFMT) == S_IFDIR) 
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

    fprintf(fp0,
       "<h2>File that you are currently viewing</h2>\n<dl compact>");

    StrAllocCopy(Title, doc->title);
    LYEntify(&Title, TRUE);
    fprintf(fp0, "<dt><em>Linkname:</em> %s%s\n",
    		 Title, (doc->isHEAD ? " (HEAD)" : ""));

    StrAllocCopy(Address, doc->address);
    LYEntify(&Address, TRUE);
    fprintf(fp0,
    	    "<dt>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<em>URL:</em> %s\n", Address);

    if (HTLoadedDocumentCharset()) {
        fprintf(fp0, "<dt><em>&nbsp;Charset:</em> %s\n",
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
        fprintf(fp0, "<dt><em>&nbsp;Charset:</em> %s (assumed)\n",
		     p_in->MIMEname);
      }
    }

    if ((cp = HText_getServer()) != NULL && *cp != '\0')
        fprintf(fp0, "<dt><em>&nbsp;&nbsp;Server:</em> %s\n", cp);

    if ((cp = HText_getDate()) != NULL && *cp != '\0')
        fprintf(fp0, "<dt><em>&nbsp;&nbsp;&nbsp;&nbsp;Date:</em> %s\n", cp);

    if ((cp = HText_getLastModified()) != NULL && *cp != '\0')
        fprintf(fp0, "<dt><em>Last Mod:</em> %s\n", cp);

    if (doc->post_data) {
	fprintf(fp0,
		"<dt><em>Post Data:</em> <xmp>%s</xmp>\n", doc->post_data);
	fprintf(fp0,
	     "<dt><em>Post Content Type:</em> %s\n", doc->post_content_type);
    }

    if (owner_address) {
        StrAllocCopy(Address, owner_address);
	LYEntify(&Address, TRUE);
    } else {
        StrAllocCopy(Address, "None");
    }
    fprintf(fp0, "<dt><em>Owner(s):</em> %s\n", Address);

    fprintf(fp0,
	"<dt>&nbsp;&nbsp;&nbsp;&nbsp;<em>size:</em> %d lines\n", size_of_file);

    fprintf(fp0, "<dt>&nbsp;&nbsp;&nbsp;&nbsp;<em>mode:</em> %s%s%s\n",
		 (lynx_mode == FORMS_LYNX_MODE ?
				  "forms mode" : "normal"),
		 (doc->safe ? ", safe" : ""),
		 (doc->internal_link ? ", internal link" : "")
	    );

    fprintf(fp0, "</dl>\n");  /* end of list */

    if (nlinks > 0) {
	fprintf(fp0,
      "<h2>Link that you currently have selected</h2>\n<dl compact>");
	StrAllocCopy(Title, links[doc->link].hightext);
	LYEntify(&Title, TRUE);
	fprintf(fp0, "<dt><em>Linkname:</em> %s\n", Title);
	if (lynx_mode == FORMS_LYNX_MODE &&
	    links[doc->link].type == WWW_FORM_LINK_TYPE) {
	    if (links[doc->link].form->submit_method) {
	        int method = links[doc->link].form->submit_method;
	        char *enctype = links[doc->link].form->submit_enctype;

		fprintf(fp0, "<dt>&nbsp;&nbsp;<em>Method:</em> %s\n",
			     (method == URL_POST_METHOD) ? "POST" :
			     (method == URL_MAIL_METHOD) ? "(email)" :
							   "GET");
		fprintf(fp0, "<dt>&nbsp;<em>Enctype:</em> %s\n",
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
	        fprintf(fp0,"<dt>&nbsp;(Form field)\n");
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
	fprintf(fp0, "<h2>No Links on the current page</h2>");

#ifdef DIRED_SUPPORT
    }
#endif /* DIRED_SUPPORT */
    fprintf(fp0, "</body>\n");

    refresh();

    fclose(fp0);
    FREE(Address);
    FREE(Title);

    return(1);
}
