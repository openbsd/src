/*		FILE WRITER				HTFWrite.h
**		===========
**
**	This version of the stream object just writes to a C file.
**	The file is assumed open and left open.
**
**	Bugs:
**		strings written must be less than buffer size.
*/

#include "HTUtils.h"
#include "tcp.h"
#include "LYCurses.h"
#include "HTFWriter.h"
#include "HTSaveToFile.h"

#include "HTFormat.h"
#include "UCDefs.h"
#include "HTAlert.h"
#include "HTFile.h"
#include "HTPlain.h"
#include "HTFile.h"
#ifdef VMS
#include "HTVMSUtils.h"
#endif /* VMS */
#ifdef DOSPATH
#include "HTDOS.h"
#endif

#include "LYStrings.h"
#include "LYUtils.h"
#include "LYGlobalDefs.h"
#include "LYSignal.h"
#include "LYSystem.h"
#include "GridText.h"
#include "LYexit.h"
#include "LYLeaks.h"
#include "LYKeymap.h"

PUBLIC char * WWW_Download_File=NULL; /* contains the name of the temp file
				      ** which is being downloaded into
				      */
PUBLIC char LYCancelDownload=FALSE;   /* exported to HTFormat.c in libWWW */

#ifdef VMS
extern BOOLEAN HadVMSInterrupt;      /* flag from cleanup_sig() 	*/
PRIVATE char * FIXED_RECORD_COMMAND = NULL;
#ifdef USE_COMMAND_FILE 	     /* Keep this as an option. - FM	*/
#define FIXED_RECORD_COMMAND_MASK "@Lynx_Dir:FIXED512 %s"
#else
#define FIXED_RECORD_COMMAND_MASK "%s"
PUBLIC unsigned long LYVMS_FixedLengthRecords PARAMS((char *filename));
#endif /* USE_COMMAND_FILE */
#endif /* VMS */

PUBLIC HTStream* HTSaveToFile PARAMS((
	HTPresentation *       pres,
	HTParentAnchor *       anchor,
	HTStream *	       sink));

#define FREE(x) if (x) {free(x); x = NULL;}


/*	Stream Object
**	-------------
*/
struct _HTStream {
	CONST HTStreamClass *	isa;

	FILE *			fp;		/* The file we've opened */
	char *			end_command;	/* What to do on _free.  */
	char *			remove_command; /* What to do on _abort. */
	char *			viewer_command; /* Saved external viewer */
	HTFormat		input_format;  /* Original pres->rep	 */
	HTFormat		output_format; /* Original pres->rep_out */
	HTParentAnchor *	anchor;     /* Original stream's anchor. */
	HTStream *		sink;	    /* Original stream's sink.	 */
#ifdef FNAMES_8_3
	int			idash; /* remember position to become '.'*/
#endif
};


/*_________________________________________________________________________
**
**			A C T I O N	R O U T I N E S
**  Bug:
**	All errors are ignored.
*/

/*	Character handling
**	------------------
*/
PRIVATE void HTFWriter_put_character ARGS2(HTStream *, me, char, c)
{
    putc(c, me->fp);
}

/*	String handling
**	---------------
**
**	Strings must be smaller than this buffer size.
*/
PRIVATE void HTFWriter_put_string ARGS2(HTStream *, me, CONST char*, s)
{
    fputs(s, me->fp);
}

/*	Buffer write.  Buffers can (and should!) be big.
**	------------
*/
PRIVATE void HTFWriter_write ARGS3(HTStream *, me, CONST char*, s, int, l)
{
    fwrite(s, 1, l, me->fp);
}




/*	Free an HTML object
**	-------------------
**
**	Note that the SGML parsing context is freed, but the created
**	object is not,
**	as it takes on an existence of its own unless explicitly freed.
*/
PRIVATE void HTFWriter_free ARGS1(HTStream *, me)
{
    FILE *fp = NULL;
    int len;
    char *path = NULL;
    char *addr = NULL;
    int status;
    BOOL use_gzread = NO;

    fflush(me->fp);
    if (me->end_command) {		/* Temp file */
	fclose(me->fp);
#ifdef VMS
	if (0 == strcmp(me->end_command, "SaveVMSBinaryFile")) {
	/*
	 *  It's a binary file saved to disk on VMS, which
	 *  we want to convert to fixed records format. - FM
	 */
#ifdef USE_COMMAND_FILE
	    system(FIXED_RECORD_COMMAND);
#else
	    LYVMS_FixedLengthRecords(FIXED_RECORD_COMMAND);
#endif /* USE_COMMAND_FILE */
	    FREE(FIXED_RECORD_COMMAND);

	    if (me->remove_command) {
		/* NEVER REMOVE THE FILE unless during an abort!*/
		/* system(me->remove_command); */
		FREE(me->remove_command);
	    }
	} else
#endif /* VMS */
	if (me->input_format == HTAtom_for("www/compressed")) {
	    /*
	     *	It's a compressed file supposedly cached to
	     *	a temporary file for uncompression. - FM
	     */
	    if (me->anchor->FileCache != NULL) {
		BOOL skip_loadfile = (me->viewer_command != NULL);
		/*
		 *  Save the path with the "gz" or "Z" suffix trimmed,
		 *  and remove any previous uncompressed copy. - FM
		 */
		StrAllocCopy(path, me->anchor->FileCache);
		if ((len = strlen(path)) > 2) {
		    if (!strcasecomp((char *)&path[len-2], "gz")) {
#ifdef USE_ZLIB
			if (!skip_loadfile) {
			    use_gzread = YES;
			} else
#endif /* USE_ZLIB */
			{
			    path[len-3] = '\0';
			    remove(path);
			}
		    } else if (!strcasecomp((char *)&path[len-1], "Z")) {
			path[len-2] = '\0';
			remove(path);
		    }
		}
		if (!use_gzread) {
		    if (!dump_output_immediately) {
			/*
			 *  Tell user what's happening. - FM
			 */
			_HTProgress(me->end_command);
		    }
		    /*
		     *	Uncompress it. - FM
		     */
		    if (me->end_command && me->end_command[0])
			system(me->end_command);
		    fp = fopen(me->anchor->FileCache, "r");
		}
		if (fp != NULL) {
		    /*
		     *	It's still there with the "gz" or "Z" suffix,
		     *	so the uncompression failed. - FM
		     */
		    fclose(fp);
		    fp = NULL;
		    if (!dump_output_immediately) {
			lynx_force_repaint();
			refresh();
		    }
		    HTAlert(ERROR_UNCOMPRESSING_TEMP);
		    remove(me->anchor->FileCache);
		    FREE(me->anchor->FileCache);
		} else {
		    /*
		     *	Succeeded!  Create a complete address
		     *	for the uncompressed file and invoke
		     *	HTLoadFile() to handle it. - FM
		     */
#ifdef FNAMES_8_3
		    /*
		     *	Assuming we have just uncompressed e.g.
		     *	FILE-mpeg.gz -> FILE-mpeg, restore/shorten
		     *	the name to be fit for passing to an external
		     *	viewer, by renaming FILE-mpeg -> FILE.mpe  - kw
		     */
		    if (skip_loadfile) {
			char *new_path = NULL;
			if (me->idash > 1 && path[me->idash] == '-') {
			    StrAllocCopy(new_path, path);
			    new_path[me->idash] = '.';
			    if (strlen(new_path + me->idash) > 4)
				new_path[me->idash + 4] = '\0';
			    if (rename(path, new_path) == 0) {
				FREE(path);
				path = new_path;
			    } else {
				FREE(new_path);
			    }
			}
		    }
#endif /* FNAMES_8_3 */
		    StrAllocCopy(addr, "file://localhost");
#ifdef DOSPATH
		    StrAllocCat(addr, "/");
		    StrAllocCat(addr, HTDOS_wwwName(path));
#else
#ifdef VMS
		    StrAllocCat(addr, HTVMS_wwwName(path));
#else
		    StrAllocCat(addr, path);
#endif /* VMS */
#endif /* DOSPATH */
		    if (!use_gzread) {
			StrAllocCopy(me->anchor->FileCache, path);
			StrAllocCopy(me->anchor->content_encoding, "binary");
		    }
		    FREE(path);
		    if (!skip_loadfile) {
			/*
			 *  Lock the chartrans info we may possibly have,
			 *  so HTCharsetFormat() will not apply the default
			 *  for local files. - KW
			 */
			if (HTAnchor_getUCLYhndl(me->anchor,
						 UCT_STAGE_PARSER) < 0 ) {
			    /*
			     *	If not yet set - KW
			     */
			    HTAnchor_copyUCInfoStage(me->anchor,
						     UCT_STAGE_PARSER,
						     UCT_STAGE_MIME,
						     UCT_SETBY_DEFAULT+1);
			}
			HTAnchor_copyUCInfoStage(me->anchor,
						 UCT_STAGE_PARSER,
						 UCT_STAGE_MIME, -1);
		    }
		    /*
		     *	Create a complete address for
		     *	the uncompressed file. - FM
		     */
		    if (!dump_output_immediately) {
			/*
			 *  Tell user what's happening. - FM
			 */
			_user_message(WWW_USING_MESSAGE, addr);
		    }

		    if (skip_loadfile) {
			/*
			 *  It's a temporary file we're passing to a
			 *  viewer or helper application.
			 *  Loading the temp file through HTLoadFile()
			 *  would result in yet another HTStream (created
			 *  with HTSaveAndExecute()) which would just
			 *  copy the temp file to another temp file
			 *  (or even the same!).  We can skip this
			 *  needless duplication by using the
			 *  viewer_command which has already been
			 *  determind when the HTCompressed stream was
			 *  created. - kw
			 */
			FREE(me->end_command);
			me->end_command = (char *)calloc (
			    (strlen (me->viewer_command) + 10 +
			     strlen(me->anchor->FileCache))
			    * sizeof (char),1);
			if (me->end_command == NULL)
			    outofmem(__FILE__, "HTFWriter_free (HTCompressed)");

			sprintf(me->end_command,
				me->viewer_command, me->anchor->FileCache,
				"", "", "", "", "", "");
			if (!dump_output_immediately) {
			    /*
			     *	Tell user what's happening. - FM
			     */
			    HTProgress(me->end_command);
			    stop_curses();
			}
			system(me->end_command);

			if (me->remove_command) {
			    /* NEVER REMOVE THE FILE unless during an abort!!!*/
			    /* system(me->remove_command); */
			    FREE(me->remove_command);
			}
			if (!dump_output_immediately)
			    start_curses();
		    } else
		    status = HTLoadFile(addr,
					me->anchor,
					me->output_format,
					me->sink);
		    if (dump_output_immediately &&
			me->output_format == HTAtom_for("www/present")) {
			FREE(addr);
			remove(me->anchor->FileCache);
			FREE(me->anchor->FileCache);
			FREE(me->remove_command);
			FREE(me->end_command);
			FREE(me->viewer_command);
			FREE(me);
			return;
		    }
		}
		FREE(addr);
	    }
	    if (me->remove_command) {
		/* NEVER REMOVE THE FILE unless during an abort!!!*/
		/* system(me->remove_command); */
		FREE(me->remove_command);
	    }
	} else if (strcmp(me->end_command, "SaveToFile")) {
	    /*
	     *	It's a temporary file we're passing to a
	     *	viewer or helper application. - FM
	     */
	    if (!dump_output_immediately) {
		/*
		 *  Tell user what's happening. - FM
		 */
		_HTProgress(me->end_command);
		stop_curses();
	    }
	    system(me->end_command);

	    if (me->remove_command) {
		/* NEVER REMOVE THE FILE unless during an abort!!!*/
		/* system(me->remove_command); */
		FREE(me->remove_command);
	    }
	    if (!dump_output_immediately)
		start_curses();
	} else {
	    /*
	     *	It's a file we saved to disk for handling
	     *	via a menu. - FM
	     */
	    if (me->remove_command) {
		/* NEVER REMOVE THE FILE unless during an abort!!!*/
		/* system(me->remove_command); */
		FREE(me->remove_command);
	    }
	}
	FREE(me->end_command);
    }
    FREE(me->viewer_command);

    if (dump_output_immediately) {
	if (me->anchor->FileCache)
	    remove(me->anchor->FileCache);
	FREE(me);
#ifndef NOSIGHUP
	(void) signal(SIGHUP, SIG_DFL);
#endif /* NOSIGHUP */
	(void) signal(SIGTERM, SIG_DFL);
#ifndef VMS
	(void) signal(SIGINT, SIG_DFL);
#endif /* !VMS */
#ifdef SIGTSTP
	if (no_suspend)
	  (void) signal(SIGTSTP,SIG_DFL);
#endif /* SIGTSTP */
	exit(0);
    }

    FREE(me);
    return;
}

/*	Abort writing
**	-------------
*/
PRIVATE void HTFWriter_abort ARGS2(
	HTStream *,	me,
	HTError,	e GCC_UNUSED)
{
    if (TRACE)
	fprintf(stderr,"HTFWriter_abort called\n");

    fclose(me->fp);
    FREE(me->viewer_command);
    if (me->end_command) {		/* Temp file */
	if (TRACE)
	    fprintf(stderr, "HTFWriter: Aborting: file not executed.\n");
	FREE(me->end_command);
	if (me->remove_command) {
	    system(me->remove_command);
	    FREE(me->remove_command);
	}
    }

    FREE(WWW_Download_File);

    FREE(me);
}

/*	Structured Object Class
**	-----------------------
*/
PRIVATE CONST HTStreamClass HTFWriter = /* As opposed to print etc */
{
	"FileWriter",
	HTFWriter_free,
	HTFWriter_abort,
	HTFWriter_put_character,	HTFWriter_put_string,
	HTFWriter_write
};

/*	Subclass-specific Methods
**	-------------------------
*/
PUBLIC HTStream* HTFWriter_new ARGS1(FILE *, fp)
{
    HTStream* me;

    if (!fp)
	return NULL;

    me = (HTStream*)calloc(sizeof(*me),1);
    if (me == NULL)
	outofmem(__FILE__, "HTFWriter_new");
    me->isa = &HTFWriter;

    me->fp = fp;
    me->end_command = NULL;
    me->remove_command = NULL;
    me->anchor = NULL;
    me->sink = NULL;

    return me;
}

/*	Make system command from template
**	---------------------------------
**
**	See mailcap spec for description of template.
*/
/* @@ to be written.  sprintfs will do for now.  */

#ifndef VMS
#define REMOVE_COMMAND "/bin/rm -f %s"
#else
#define REMOVE_COMMAND "delete/noconfirm/nolog %s;"
#endif /* VMS */

/*	Take action using a system command
**	----------------------------------
**
**	originally from Ghostview handling by Marc Andreseen.
**	Creates temporary file, writes to it, executes system command
**	on end-document.  The suffix of the temp file can be given
**	in case the application is fussy, or so that a generic opener can
**	be used.
*/
PUBLIC HTStream* HTSaveAndExecute ARGS3(
	HTPresentation *,	pres,
	HTParentAnchor *,	anchor,
	HTStream *,		sink)
{
    char fnam[256];
    CONST char *suffix;
    char *cp;
    HTStream* me;
    FILE *fp = NULL;

    if (traversal) {
	LYCancelledFetch = TRUE;
	return(NULL);
    }

#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
    if (pres->quality == 999.0) { /* exec link */
	if (dump_output_immediately) {
	    LYCancelledFetch = TRUE;
	    return(NULL);
	}
	if (no_exec) {
	    _statusline(EXECUTION_DISABLED);
	    sleep(AlertSecs);
	    return HTPlainPresent(pres, anchor, sink);
	}
	if (!local_exec)
	    if (local_exec_on_local_files &&
		(LYJumpFileURL ||
		 !strncmp(anchor->address,"file://localhost",16))) {
		/* allow it to continue */
	    } else {
		char buf[512];

		sprintf(buf, EXECUTION_DISABLED_FOR_FILE,
			     key_for_func(LYK_OPTIONS));
		_statusline(buf);
		sleep(AlertSecs);
		return HTPlainPresent(pres, anchor, sink);
	    }
    }
#endif /* EXEC_LINKS || EXEC_SCRIPTS */

    if (dump_output_immediately) {
	return(HTSaveToFile(pres, anchor, sink));
    }

    me = (HTStream*)calloc(sizeof(*me),1);
    if (me == NULL)
	outofmem(__FILE__, "HTSaveAndExecute");
    me->isa = &HTFWriter;
    me->input_format = pres->rep;
    me->output_format = pres->rep_out;
    me->anchor = anchor;
    me->sink = sink;

    if (anchor->FileCache) {
	strcpy(fnam, anchor->FileCache);
	FREE(anchor->FileCache);
	if ((fp = fopen(fnam, "r")) != NULL) {
	    fclose(fp);
	    fp = NULL;
	    remove(fnam);
	}
    } else {
	/*
	 *  Lynx routine to create a temporary filename
	 */
SaveAndExecute_tempname:
	tempname (fnam, NEW_FILE);
	/*
	 *  Check for a suffix.
	 */
	if (((cp = strrchr(fnam, '.')) != NULL) &&
#ifdef VMS
	    NULL == strchr(cp, ']') &&
#endif /* VMS */
	    NULL == strchr(cp, '/')) {
	    /*
	     *	Save the file under a suitably suffixed name.
	     */
	    *cp = '\0';
	    if (!strcasecomp(pres->rep->name, "text/html")) {
		strcat(fnam, HTML_SUFFIX);
	    } else if (!strcasecomp(pres->rep->name, "text/plain")) {
		strcat(fnam, ".txt");
	    } else if (!strcasecomp(pres->rep->name,
				    "application/octet-stream")) {
		strcat(fnam, ".bin");
	    } else if ((suffix = HTFileSuffix(pres->rep, anchor->content_encoding))
		       && *suffix == '.') {
		strcat(fnam, suffix);
		/*
		 *  It's not one of the suffixes checked for a
		 *  spoof in tempname(), so check it now. - FM
		 */
		if (strcmp(suffix, HTML_SUFFIX) &&
		    strcmp(suffix, ".txt") &&
		    strcmp(suffix, ".bin") &&
		    (fp = fopen(fnam, "r")) != NULL) {
		    fclose(fp);
		    fp = NULL;
		    goto SaveAndExecute_tempname;
		}
	    } else {
		*cp = '.';
	    }
	}
    }

    me->fp = LYNewBinFile (fnam);
    if (!me->fp) {
	HTAlert(CANNOT_OPEN_TEMP);
	FREE(me);
	return NULL;
    }

    StrAllocCopy(me->viewer_command, pres->command);
    /*
     *	Make command to process file.
     */
    me->end_command = (char *)calloc (
			(strlen (pres->command) + 10 + strlen(fnam))
			 * sizeof (char),1);
    if (me->end_command == NULL)
	outofmem(__FILE__, "HTSaveAndExecute");

    sprintf(me->end_command, pres->command, fnam, "", "", "", "", "", "");

    /*
     *	Make command to delete file.
     */
    me->remove_command = (char *)calloc (
			(strlen (REMOVE_COMMAND) + 10 + strlen(fnam))
			 * sizeof (char),1);
    if (me->remove_command == NULL)
	outofmem(__FILE__, "HTSaveAndExecute");

    sprintf(me->remove_command, REMOVE_COMMAND, fnam);

    StrAllocCopy(anchor->FileCache, fnam);
    return me;
}


/*	Format Converter using system command
**	-------------------------------------
*/

/* @@@@@@@@@@@@@@@@@@@@@@ */

/*	Save to a local file   LJM!!!
**	--------------------
**
**	usually a binary file that can't be displayed
**
**	originally from Ghostview handling by Marc Andreseen.
**	Asks the user if he wants to continue, creates a temporary
**	file, and writes to it.  In HTSaveToFile_Free
**	the user will see a list of choices for download
*/
PUBLIC HTStream* HTSaveToFile ARGS3(
	HTPresentation *,	pres,
	HTParentAnchor *,	anchor,
	HTStream *,		sink)
{
    HTStream * ret_obj;
    char fnam[256];
    CONST char * suffix;
    char *cp;
    int c=0;
    BOOL IsBinary = TRUE;
    FILE *fp = NULL;

    ret_obj = (HTStream*)calloc(sizeof(* ret_obj),1);
    if (ret_obj == NULL)
	outofmem(__FILE__, "HTSaveToFile");
    ret_obj->isa = &HTFWriter;
    ret_obj->remove_command = NULL;
    ret_obj->end_command = NULL;
    ret_obj->input_format = pres->rep;
    ret_obj->output_format = pres->rep_out;
    ret_obj->anchor = anchor;
    ret_obj->sink = sink;

    if (dump_output_immediately) {
	ret_obj->fp = stdout; /* stdout*/
	if (HTOutputFormat == HTAtom_for("www/download"))
	    goto Prepend_BASE;
	return ret_obj;
    }

    LYCancelDownload = FALSE;
    if (HTOutputFormat != HTAtom_for("www/download")) {
	if (traversal ||
	    (no_download && !override_no_download && no_disk_save)) {
	    if (!traversal) {
		_statusline(CANNOT_DISPLAY_FILE);
		sleep(AlertSecs);
	    }
	    LYCancelDownload = TRUE;
	    if (traversal)
		LYCancelledFetch = TRUE;
	    FREE(ret_obj);
	    return(NULL);
	}

	if (((cp=strchr((char *)pres->rep->name, ';')) != NULL) &&
	    strstr((cp+1), "charset") != NULL) {
	    _user_message(WRONG_CHARSET_D_OR_C, (char *)pres->rep->name);
	} else if (*((char *)pres->rep->name) != '\0')	{
	    _user_message(UNMAPPED_TYPE_D_OR_C, (char *)pres->rep->name);
	} else {
	    _statusline(CANNOT_DISPLAY_FILE_D_OR_C);
	}

	while(TOUPPER(c)!='C' && TOUPPER(c)!='D' && c!=7) {
	    c=LYgetch();
#ifdef VMS
	    /*
	     *	'C'ancel on Control-C or Control-Y and
	     *	a 'N'o to the "really exit" query. - FM
	     */
	    if (HadVMSInterrupt) {
		HadVMSInterrupt = FALSE;
		c = 'C';
	    }
#endif /* VMS */
	}

	/*
	 *  Cancel on 'C', 'c' or Control-G or Control-C.
	 */
	if (TOUPPER(c)=='C' || c==7 || c==3) {
	    _statusline(CANCELLING_FILE);
	    LYCancelDownload = TRUE;
	    FREE(ret_obj);
	    return(NULL);
	}
    }

    /*
     *	Set up a 'D'ownload.
     */
    if (anchor->FileCache) {
	strcpy(fnam, anchor->FileCache);
	FREE(anchor->FileCache);
	if ((fp = fopen(fnam, "r")) != NULL) {
	    fclose(fp);
	    fp = NULL;
	    remove(fnam);
	}
    } else {
	/*
	 *  Lynx routine to create a temporary filename
	 */
SaveToFile_tempname:
	tempname(fnam, NEW_FILE);
	/*
	 *  Check for a suffix.
	 */
	if (((cp=strrchr(fnam, '.')) != NULL) &&
#ifdef VMS
	    NULL == strchr(cp, ']') &&
#endif /* VMS */
	    NULL == strchr(cp, '/')) {
	    /*
	     *	Save the file under a suitably suffixed name.
	     */
	    *cp = '\0';
	    if (!strcasecomp(pres->rep->name, "text/html")) {
		strcat(fnam, HTML_SUFFIX);
	    } else if (!strcasecomp(pres->rep->name, "text/plain")) {
		strcat(fnam, ".txt");
	    } else if (!strcasecomp(pres->rep->name,
				    "application/octet-stream")) {
		strcat(fnam, ".bin");
	    } else if ((suffix = HTFileSuffix(pres->rep,
					      anchor->content_encoding)) && *suffix == '.') {
		strcat(fnam, suffix);
		/*
		 *  It's not one of the suffixes checked for a
		 *  spoof in tempname(), so check it now. - FM
		 */
		if (strcmp(suffix, HTML_SUFFIX) &&
		    strcmp(suffix, ".txt") &&
		    strcmp(suffix, ".bin") &&
		    (fp = fopen(fnam, "r")) != NULL) {
		    fclose(fp);
		    fp = NULL;
		    goto SaveToFile_tempname;
		}
	    } else {
		*cp = '.';
	    }
	}
    }

    if (0==strncasecomp(pres->rep->name, "text/", 5) ||
	0==strcasecomp(pres->rep->name, "application/postscript") ||
	0==strcasecomp(pres->rep->name, "application/x-RUNOFF-MANUAL"))
	/*
	 *  It's a text file requested via 'd'ownload.
	 *  Keep adding others to the above list, 'til
	 *  we add a configurable procedure. - FM
	 */
	IsBinary = FALSE;

    ret_obj->fp = LYNewBinFile (fnam);
    if (!ret_obj->fp) {
	HTAlert(CANNOT_OPEN_OUTPUT);
	FREE(ret_obj);
	return NULL;
    }

    /*
     *	Any "application/foo" or other non-"text/foo" types that
     *	are actually text but not checked, above, will be treated
     *	as binary, so show the type to help sort that out later.
     *	Unix folks don't need to know this, but we'll show it to
     *	them, too. - FM
     */
    user_message("Content-type: %s", pres->rep->name);
    sleep(MessageSecs);

    StrAllocCopy(WWW_Download_File,fnam);

    /*
     *	Make command to delete file.
     */
    ret_obj->remove_command = (char *)calloc (
			(strlen (REMOVE_COMMAND) + 10+ strlen(fnam))
			 * sizeof (char),1);
    if (ret_obj->remove_command == NULL)
	outofmem(__FILE__, "HTSaveToFile");

    sprintf(ret_obj->remove_command, REMOVE_COMMAND, fnam);

#ifdef VMS
    if (IsBinary && UseFixedRecords) {
	ret_obj->end_command = (char *)calloc (sizeof(char)*20,1);
	if (ret_obj->end_command == NULL)
	    outofmem(__FILE__, "HTSaveToFile");
	sprintf(ret_obj->end_command, "SaveVMSBinaryFile");
	FIXED_RECORD_COMMAND = (char *)calloc (
		(strlen (FIXED_RECORD_COMMAND_MASK) + 10 + strlen(fnam))
		* sizeof (char),1);
	if (FIXED_RECORD_COMMAND == NULL)
	    outofmem(__FILE__, "HTSaveToFile");
	sprintf(FIXED_RECORD_COMMAND,
		FIXED_RECORD_COMMAND_MASK, fnam, "", "", "", "", "", "");
    } else {
#endif /* VMS */
    ret_obj->end_command = (char *)calloc (sizeof(char)*12,1);
    if (ret_obj->end_command == NULL)
	outofmem(__FILE__, "HTSaveToFile");
    sprintf(ret_obj->end_command, "SaveToFile");
#ifdef VMS
    }
#endif /* VMS */

    _statusline(RETRIEVING_FILE);

    StrAllocCopy(anchor->FileCache, fnam);
Prepend_BASE:
    if (LYPrependBaseToSource &&
	!strncasecomp(pres->rep->name, "text/html", 9) &&
	!anchor->content_encoding) {
	/*
	 *  Add the document's base as a BASE tag at the top of the file,
	 *  so that any partial or relative URLs within it will be resolved
	 *  relative to that if no BASE tag is present and replaces it.
	 *  Note that the markup will be technically invalid if a DOCTYPE
	 *  declaration, or HTML or HEAD tags, are present, and thus the
	 *  file may need editing for perfection. - FM
	 */
	char *temp = NULL;

	if (anchor->content_base && *anchor->content_base) {
	    StrAllocCopy(temp, anchor->content_base);
	} else if (anchor->content_location && *anchor->content_location) {
	    StrAllocCopy(temp, anchor->content_location);
	}
	if (temp) {
	    collapse_spaces(temp);
	    if (!is_url(temp)) {
		FREE(temp);
	    }
	}

	fprintf(ret_obj->fp,
		"<!-- X-URL: %s -->\n<BASE HREF=\"%s\">\n\n",
		anchor->address, (temp ? temp : anchor->address));
	FREE(temp);
    }
    if (LYPrependCharsetToSource &&
	!strncasecomp(pres->rep->name, "text/html", 9) &&
	!anchor->content_encoding) {
	/*
	 *  Add the document's charset as a META CHARSET tag
	 *  at the top of the file, so HTTP charset header
	 *  will not be forgotten when a document saved as local file.
	 *  We add this line only(!) if HTTP charset present. - LP
	 *  Note that the markup will be technically invalid if a DOCTYPE
	 *  declaration, or HTML or HEAD tags, are present, and thus the
	 *  file may need editing for perfection. - FM
	 */
	char *temp = NULL;

	if (anchor->charset && *anchor->charset) {
	    StrAllocCopy(temp, anchor->charset);
	    collapse_spaces(temp);
		fprintf(ret_obj->fp,
		"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=%s\">\n\n",
		temp);
	}
	FREE(temp);
    }
    return ret_obj;
}

/*	Set up stream for uncompressing - FM
**	-------------------------------
*/
PUBLIC HTStream* HTCompressed ARGS3(
	HTPresentation *,	pres,
	HTParentAnchor *,	anchor,
	HTStream *,		sink)
{
    HTStream* me;
    HTFormat format;
    char *type = NULL;
    HTPresentation *Pres = NULL;
    int n, i;
    BOOL can_present = FALSE;
    char fnam[256];
    CONST char *suffix;
    char *uncompress_mask = NULL;
    char *compress_suffix = "";
    char *cp;
    CONST char *middle;
    FILE *fp = NULL;

    /*
     *	Deal with any inappropriate invokations of this function,
     *	or a download request, in which case we won't bother to
     *	uncompress the file. - FM
     */
    if (!(anchor && anchor->content_encoding && anchor->content_type)) {
	/*
	 *  We have no idea what we're dealing with,
	 *  so treat it as a binary stream. - FM
	 */
	format = HTAtom_for("application/octet-stream");
	me = HTStreamStack(format, pres->rep_out, sink, anchor);
	return me;
    }
    n = HTList_count(HTPresentations);
    for (i = 0; i < n; i++) {
	Pres = (HTPresentation *)HTList_objectAt(HTPresentations, i);
	if (!strcasecomp(Pres->rep->name, anchor->content_type) &&
	    Pres->rep_out == WWW_PRESENT) {
	    /*
	     *	We have a presentation mapping for it. - FM
	     */
	    can_present = TRUE;
	    if (!strcasecomp(anchor->content_encoding, "x-gzip") ||
		!strcasecomp(anchor->content_encoding, "gzip")) {
		/*
		 *  It's compressed with the modern gzip. - FM
		 */
		StrAllocCopy(uncompress_mask, GZIP_PATH);
		StrAllocCat(uncompress_mask, " -d --no-name %s");
		compress_suffix = "gz";
	    } else if (!strcasecomp(anchor->content_encoding, "x-compress") ||
		       !strcasecomp(anchor->content_encoding, "compress")) {
		/*
		 *  It's compressed the old fashioned Unix way. - FM
		 */
		StrAllocCopy(uncompress_mask, UNCOMPRESS_PATH);
		StrAllocCat(uncompress_mask, " %s");
		compress_suffix = "Z";
	    }
	    break;
	}
    }
    if (can_present == FALSE || 		 /* no presentation mapping */
	uncompress_mask == NULL ||		    /* not gzip or compress */
	strchr(anchor->content_type, ';') ||		   /* wrong charset */
	HTOutputFormat == HTAtom_for("www/download") || 	/* download */
	!strcasecomp(pres->rep_out->name, "www/download") ||	/* download */
	(traversal &&	   /* only handle html or plain text for traversals */
	 strcasecomp(anchor->content_type, "text/html") &&
	 strcasecomp(anchor->content_type, "text/plain"))) {
	/*
	 *  Cast the Content-Encoding to a Content-Type
	 *  and pass it back to be handled as that type. - FM
	 */
	if (strchr(anchor->content_encoding, '/') == NULL) {
	    StrAllocCopy(type, "application/");
	    StrAllocCat(type, anchor->content_encoding);
	} else {
	    StrAllocCopy(type, anchor->content_encoding);
	}
	format = HTAtom_for(type);
	FREE(type)
	FREE(uncompress_mask);
	me = HTStreamStack(format, pres->rep_out, sink, anchor);
	return me;
    }

    /*
     *	Set up the stream structure for uncompressing and then
     *	handling based on the uncompressed Content-Type.- FM
     */
    me = (HTStream*)calloc(sizeof(*me),1);
    if (me == NULL)
	outofmem(__FILE__, "HTCompressed");
    me->isa = &HTFWriter;
    me->input_format = pres->rep;
    me->output_format = pres->rep_out;
    me->anchor = anchor;
    me->sink = sink;

    /*
     *	Remove any old versions of the file. - FM
     */
    if (anchor->FileCache) {
	while ((fp = fopen(anchor->FileCache, "r")) != NULL) {
	    fclose(fp);
	    remove(anchor->FileCache);
	}
	FREE(anchor->FileCache);
    }

    /*
     *	Get a new temporary filename and substitute a suitable suffix. - FM
     */
Compressed_tempname:
    tempname(fnam, NEW_FILE);
    if ((cp = strrchr(fnam, '.')) != NULL) {
	middle = NULL;
	if (!strcasecomp(anchor->content_type, "text/html")) {
	    middle = HTML_SUFFIX;
	    middle++;		/* point to 'h' of .htm(l) - kw */
	} else if (!strcasecomp(anchor->content_type, "text/plain")) {
	    middle = "txt";
	} else if (!strcasecomp(anchor->content_type,
				"application/octet-stream")) {
	    middle = "bin";
	} else if ((suffix =
		    HTFileSuffix(HTAtom_for(anchor->content_type), NULL)) &&
		   *suffix == '.') {
#if defined(VMS) || defined(FNAMES_8_3)
	    if (strchr(suffix + 1, '.') == NULL)
#endif
		middle = suffix + 1;
	}
	if (middle) {
	    *cp = '\0';
#ifdef FNAMES_8_3
	    me->idash = strlen(fnam);	  /* remember position of '-'  - kw */
	    strcat(fnam, "-");	/* NAME-htm,  NAME-txt, etc. - hack for DOS */
#else
	    strcat(fnam, ".");	/* NAME.html, NAME-txt etc. */
#endif /* FNAMES_8_3 */
	    strcat(fnam, middle);
#ifdef VMS
	    strcat(fnam, "-");	/* NAME.html-gz, NAME.txt-gz, NAME.txt-Z etc.*/
#else
	    strcat(fnam, ".");	/* NAME-htm.gz (DOS), NAME.html.gz (UNIX)etc.*/
#endif /* VMS */
	} else {
	    *(cp + 1) = '\0';
	}
    } else {
	strcat(fnam, ".");
    }
    strcat(fnam, compress_suffix);
    /*
     *	It's not one of the suffixes checked for a
     *	spoof in tempname(), so check it now. - FM
     */
    if ((fp = fopen(fnam, "r")) != NULL) {
	fclose(fp);
	fp = NULL;
	goto Compressed_tempname;
    }

    /*
     *	Open the file for receiving the compressed input stream. - FM
     */
    me->fp = LYNewBinFile (fnam);
    if (!me->fp) {
	HTAlert(CANNOT_OPEN_TEMP);
	FREE(uncompress_mask);
	FREE(me);
	return NULL;
    }

    /*
     *	me->viewer_command will be NULL if the converter Pres found above
     *	is not for an external viewer but an internal HTStream converter.
     *	We also don't set it under conditions where HTSaveAndExecute would
     *	disallow execution of the command. - KW
     */
    if (!dump_output_immediately && !traversal
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
	&& (Pres->quality != 999.0 ||
	    (!no_exec &&	/* allowed exec link or script ? */
	     (local_exec ||
	      (local_exec_on_local_files &&
	       (LYJumpFileURL ||
		!strncmp(anchor->address,"file://localhost",16))))))
#endif /* EXEC_LINKS || EXEC_SCRIPTS */
	) {
	StrAllocCopy(me->viewer_command, Pres->command);
    }

    /*
     *	Make command to process file. - FM
     */
#ifdef USE_ZLIB
    if (compress_suffix[0] == 'g' && /* must be gzip */
	!me->viewer_command) {
	/*
	 *  We won't call gzip externally, so we don't need to supply
	 *  a command for it. - kw
	 */
	StrAllocCopy(me->end_command, "");
    } else
#endif /* USE_ZLIB */
    {
	me->end_command = (char *)calloc(1, (strlen(uncompress_mask) + 10 +
					     strlen(fnam)) * sizeof(char));
	if (me->end_command == NULL)
	    outofmem(__FILE__, "HTCompressed");
	sprintf(me->end_command, uncompress_mask, fnam, "", "", "", "", "", "");
    }
    FREE(uncompress_mask);

    /*
     *	Make command to delete file. - FM
     */
    me->remove_command = (char *)calloc(1, (strlen(REMOVE_COMMAND) + 10 +
					    strlen(fnam)) * sizeof(char));
    if (me->remove_command == NULL)
	outofmem(__FILE__, "HTCompressed");
    sprintf(me->remove_command, REMOVE_COMMAND, fnam);

    /*
     *	Save the filename and return the structure. - FM
     */
    StrAllocCopy(anchor->FileCache, fnam);
    return me;
}

/*	Dump output to stdout - LJM & FM
**	---------------------
**
*/
PUBLIC HTStream* HTDumpToStdout ARGS3(
	HTPresentation *,	pres GCC_UNUSED,
	HTParentAnchor *,	anchor,
	HTStream *,		sink GCC_UNUSED)
{
    HTStream * ret_obj;
    ret_obj = (HTStream*)calloc(sizeof(* ret_obj),1);
    if (ret_obj == NULL)
	outofmem(__FILE__, "HTDumpToStdout");
    ret_obj->isa = &HTFWriter;
    ret_obj->remove_command = NULL;
    ret_obj->end_command = NULL;
    ret_obj->anchor = anchor;

    ret_obj->fp = stdout; /* stdout*/
    return ret_obj;
}

#if defined(VMS) && !defined(USE_COMMAND_FILE)
#include <fab.h>
#include <rmsdef.h>		/* RMS status codes */
#include <iodef.h>		/* I/O function codes */
#include <fibdef.h>		/* file information block defs */
#include <atrdef.h>		/* attribute request codes */
#ifdef NOTDEFINED /*** Not all versions of VMS compilers have these.	 ***/
#include <fchdef.h>		/* file characteristics */
#include <fatdef.h>		/* file attribute defs */
#else		  /*** So we'll define what we need from them ourselves. ***/
#define FCH$V_CONTIGB	0x005			/* pos of cont best try bit */
#define FCH$M_CONTIGB	(1 << FCH$V_CONTIGB)	/* contig best try bit mask */
/* VMS I/O User's Reference Manual: Part I (V5.x doc set) */
struct fatdef {
    unsigned char	fat$b_rtype,	fat$b_rattrib;
    unsigned short	fat$w_rsize;
    unsigned long	fat$l_hiblk,	fat$l_efblk;
    unsigned short	fat$w_ffbyte;
    unsigned char	fat$b_bktsize,	fat$b_vfcsize;
    unsigned short	fat$w_maxrec,	fat$w_defext,	fat$w_gbc;
    unsigned	: 16, : 32, : 16;   /* 6 bytes reserved, 2 bytes not used */
    unsigned short	fat$w_versions;
};
#endif /* NOTDEFINED */

/* arbitrary descriptor without type and class info */
typedef struct dsc { unsigned short len, mbz; void *adr; } Desc;

extern unsigned long	sys$open(),  sys$qiow(),  sys$dassgn();

#define syswork(sts)	((sts) & 1)
#define sysfail(sts)	(!syswork(sts))


/*
 *  25-Jul-1995 - Pat Rankin (rankin@eql.caltech.edu)
 *
 *  Force a file to be marked as having fixed-length, 512 byte records
 *  without implied carriage control, and with best_try_contiguous set.
 */
PUBLIC unsigned long LYVMS_FixedLengthRecords ARGS1(char *, filename)
{
    struct FAB	    fab;		/* RMS file access block */
    struct fibdef   fib;		/* XQP file information block */
    struct fatdef   recattr;		/* XQP file "record" attributes */
    struct atrdef   attr_rqst_list[3];	/* XQP attribute request itemlist */

    Desc	    fib_dsc;
    unsigned short  channel,  iosb[4];
    unsigned long   fchars,  sts,  tmp;

    /* initialize file access block */
    fab = cc$rms_fab;
    fab.fab$l_fna = filename;
    fab.fab$b_fns = (unsigned char) strlen(filename);
    fab.fab$l_fop = FAB$M_UFO;	/* user file open; no further RMS processing */
    fab.fab$b_fac = FAB$M_PUT;	/* need write access */
    fab.fab$b_shr = FAB$M_NIL;	/* exclusive access */

    sts = sys$open(&fab);	/* channel in stv; $dassgn to close */
    if (sts == RMS$_FLK) {
	/* For MultiNet, at least, if the file was just written by a remote
	   NFS client, the local NFS server might still have it open, and the
	   failed access attempt will provoke it to be closed, so try again. */
	sts = sys$open(&fab);
    }
    if (sysfail(sts)) return sts;

    /* RMS supplies a user-mode channel (see FAB$L_FOP FAB$V_UFO doc) */
    channel = (unsigned short) fab.fab$l_stv;

    /* set up ACP interface strutures */
    /* file information block, passed by descriptor; it's okay to start with
       an empty FIB after RMS has accessed the file for us */
    fib_dsc.len = sizeof fib;
    fib_dsc.mbz = 0;
    fib_dsc.adr = &fib;
    memset((void *)&fib, 0, sizeof fib);
    /* attribute request list */
    attr_rqst_list[0].atr$w_size = sizeof recattr;
    attr_rqst_list[0].atr$w_type = ATR$C_RECATTR;
    *(void **)&attr_rqst_list[0].atr$l_addr = (void *)&recattr;
    attr_rqst_list[1].atr$w_size = sizeof fchars;
    attr_rqst_list[1].atr$w_type = ATR$C_UCHAR;
    *(void **)&attr_rqst_list[1].atr$l_addr = (void *)&fchars;
    attr_rqst_list[2].atr$w_size = attr_rqst_list[2].atr$w_type = 0;
    attr_rqst_list[2].atr$l_addr = 0;
    /* file "record" attributes */
    memset((void *)&recattr, 0, sizeof recattr);
    fchars = 0; 	/* file characteristics */

    /* get current attributes */
    sts = sys$qiow(0, channel, IO$_ACCESS, iosb, (void(*)())0, 0,
		   &fib_dsc, 0, 0, 0, attr_rqst_list, 0);
    if (syswork(sts))
	sts = iosb[0];

    /* set desired attributes */
    if (syswork(sts)) {
	recattr.fat$b_rtype = FAB$C_SEQ | FAB$C_FIX;	/* org=seq, rfm=fix */
	recattr.fat$w_rsize = recattr.fat$w_maxrec = 512;   /* lrl=mrs=512 */
	recattr.fat$b_rattrib = 0;			/* rat=none */
	fchars |= FCH$M_CONTIGB;		/* contiguous-best-try */
	sts = sys$qiow(0, channel, IO$_DEACCESS, iosb, (void(*)())0, 0,
		       &fib_dsc, 0, 0, 0, attr_rqst_list, 0);
	if (syswork(sts))
	    sts = iosb[0];
    }

    /* all done */
    tmp = sys$dassgn(channel);
    if (syswork(sts))
	sts = tmp;
    return sts;
}
#endif /* VMS && !USE_COMMAND_FILE */
