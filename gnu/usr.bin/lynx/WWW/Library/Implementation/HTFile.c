/*			File Access				HTFile.c
**			===========
**
**	This is unix-specific code in general, with some VMS bits.
**	These are routines for file access used by browsers.
**	Development of this module for Unix DIRED_SUPPORT in Lynx
**	 regrettably has has been conducted in a manner with now
**	 creates a major impediment for hopes of adapting Lynx to
**	 a newer version of the library.
**
**  History:
**	   Feb 91	Written Tim Berners-Lee CERN/CN
**	   Apr 91	vms-vms access included using DECnet syntax
**	26 Jun 92 (JFG) When running over DECnet, suppressed FTP.
**			Fixed access bug for relative names on VMS.
**	   Sep 93 (MD)	Access to VMS files allows sharing.
**	15 Nov 93 (MD)	Moved HTVMSname to HTVMSUTILS.C
**	27 Dec 93 (FM)	FTP now works with VMS hosts.
**			FTP path must be Unix-style and cannot include
**			the device or top directory.
*/

#include <HTUtils.h>

#ifndef VMS
#if defined(DOSPATH)
#undef LONG_LIST
#define LONG_LIST  /* Define this for long style unix listings (ls -l),
		     the actual style is configurable from lynx.cfg */
#endif
/* #define NO_PARENT_DIR_REFERENCE */ /* Define this for no parent links */
#endif /* !VMS */

#if defined(DOSPATH)
#define HAVE_READDIR 1
#define USE_DIRENT
#endif

#if defined(USE_DOS_DRIVES)
#include <HTDOS.h>
#endif

#include <HTFile.h>		/* Implemented here */

#ifdef VMS
#include <stat.h>
#endif /* VMS */

#if defined (USE_ZLIB) || defined (USE_BZLIB)
#include <GridText.h>
#endif

#define MULTI_SUFFIX ".multi"	/* Extension for scanning formats */

#include <HTParse.h>
#include <HTTCP.h>
#ifndef DECNET
#include <HTFTP.h>
#endif /* !DECNET */
#include <HTAnchor.h>
#include <HTAtom.h>
#include <HTAAProt.h>
#include <HTFWriter.h>
#include <HTInit.h>
#include <HTBTree.h>
#include <HTAlert.h>
#include <HTCJK.h>
#include <UCDefs.h>
#include <UCMap.h>
#include <UCAux.h>

#include <LYexit.h>
#include <LYCharSets.h>
#include <LYGlobalDefs.h>
#include <LYStrings.h>
#include <LYUtils.h>
#include <LYLeaks.h>

typedef struct _HTSuffix {
	char *		suffix;
	HTAtom *	rep;
	HTAtom *	encoding;
	char *		desc;
	float		quality;
} HTSuffix;

typedef struct {
    struct stat file_info;
    char sort_tags;
    char file_name[1];	/* on the end of the struct, since its length varies */
} DIRED;

#ifndef NGROUPS
#ifdef NGROUPS_MAX
#define NGROUPS NGROUPS_MAX
#else
#define NGROUPS 32
#endif /* NGROUPS_MAX */
#endif /* NGROUPS */

#ifndef GETGROUPS_T
#define GETGROUPS_T int
#endif

#include <HTML.h>		/* For directory object building */

#define PUTC(c) (*target->isa->put_character)(target, c)
#define PUTS(s) (*target->isa->put_string)(target, s)
#define START(e) (*target->isa->start_element)(target, e, 0, 0, -1, 0)
#define END(e) (*target->isa->end_element)(target, e, 0)
#define MAYBE_END(e) if (HTML_dtd.tags[e].contents != SGML_EMPTY) \
			(*target->isa->end_element)(target, e, 0)
#define FREE_TARGET (*target->isa->_free)(target)
#define ABORT_TARGET (*targetClass._abort)(target, NULL);

struct _HTStructured {
	CONST HTStructuredClass *	isa;
	/* ... */
};

/*
**  Controlling globals.
*/
PUBLIC int HTDirAccess = HT_DIR_OK;

#ifdef DIRED_SUPPORT
PUBLIC int HTDirReadme = HT_DIR_README_NONE;
#else
PUBLIC int HTDirReadme = HT_DIR_README_TOP;
#endif /* DIRED_SUPPORT */

PRIVATE char *HTMountRoot = "/Net/";		/* Where to find mounts */
#ifdef VMS
PRIVATE char *HTCacheRoot = "/WWW$SCRATCH";	/* Where to cache things */
#else
PRIVATE char *HTCacheRoot = "/tmp/W3_Cache_";	/* Where to cache things */
#endif /* VMS */

/*
**  Suffix registration.
*/
PRIVATE HTList * HTSuffixes = 0;
PRIVATE HTSuffix no_suffix = { "*", NULL, NULL, NULL, 1.0 };
PRIVATE HTSuffix unknown_suffix = { "*.*", NULL, NULL, NULL, 1.0};


/*	To free up the suffixes at program exit.
**	----------------------------------------
*/
#ifdef LY_FIND_LEAKS
PRIVATE void free_suffixes NOPARAMS;
#endif

#ifdef LONG_LIST
PRIVATE char *FormatStr ARGS3(
    char **,	bufp,
    char *,	start,
    CONST char *,	entry)
{
    char fmt[512];
    if (*start) {
	sprintf(fmt, "%%%.*ss", (int) sizeof(fmt) - 3, start);
	HTSprintf0(bufp, fmt, entry);
    } else if (*bufp && !(entry && *entry)) {
	**bufp = '\0';
    } else if (entry) {
	StrAllocCopy(*bufp, entry);
    }
    return *bufp;
}

PRIVATE char *FormatNum ARGS3(
    char **,	bufp,
    char *,	start,
    int,	entry)
{
    char fmt[512];
    if (*start) {
	sprintf(fmt, "%%%.*sd", (int) sizeof(fmt) - 3, start);
	HTSprintf0(bufp, fmt, entry);
    } else {
	sprintf(fmt, "%d", entry);
	StrAllocCopy(*bufp, fmt);
    }
    return *bufp;
}

PRIVATE void LYListFmtParse ARGS5(
	char *,		fmtstr,
	DIRED *,	data,
	char *,		file,
	HTStructured *, target,
	char *,		tail)
{
	char c;
	char *s;
	char *end;
	char *start;
	char *str = NULL;
	char *buf = NULL;
	char tmp[LY_MAXPATH];
	char type;
#ifndef NOUSERS
	char *name;
#endif
	time_t now;
	char *datestr;
#ifdef S_IFLNK
	int len;
#endif
#define SEC_PER_YEAR	(60 * 60 * 24 * 365)

#ifdef _WINDOWS	/* 1998/01/06 (Tue) 21:20:53 */
	static char *pbits[] = {
		"---", "--x", "-w-", "-wx",
		"r--", "r-x", "rw-", "rwx",
		0 };
#define PBIT(a, n, s)  pbits[((a) >> (n)) & 0x7]

#else
	static char *pbits[] = { "---", "--x", "-w-", "-wx",
		"r--", "r-x", "rw-", "rwx", 0 };
	static char *psbits[] = { "--S", "--s", "-wS", "-ws",
		"r-S", "r-s", "rwS", "rws", 0 };
#define PBIT(a, n, s)  (s) ? psbits[((a) >> (n)) & 0x7] : \
	pbits[((a) >> (n)) & 0x7]
#endif
#ifdef S_ISVTX
	static char *ptbits[] = { "--T", "--t", "-wT", "-wt",
		"r-T", "r-t", "rwT", "rwt", 0 };
#define PTBIT(a, s)  (s) ? ptbits[(a) & 0x7] : pbits[(a) & 0x7]
#else
#define PTBIT(a, s)  PBIT(a, 0, 0)
#endif

	if (data->file_info.st_mode == 0)
		fmtstr = "    %a";	/* can't stat so just do anchor */

	StrAllocCopy(str, fmtstr);
	s = str;
	end = str + strlen(str);
	START(HTML_PRE);
	while (*s) {
		start = s;
		while (*s) {
			if (*s == '%') {
				if (*(s+1) == '%') /* literal % */
					s++;
				else
					break;
			}
			s++;
		}
		/* s is positioned either at a % or at \0 */
		*s = '\0';
		if (s > start) {	/* some literal chars. */
			PUTS(start);
		}
		if (s == end)
			break;
		start = ++s;
		while (isdigit(UCH(*s)) || *s == '.' || *s == '-' || *s == ' ' ||
		    *s == '#' || *s == '+' || *s == '\'')
			s++;
		c = *s;		/* the format char. or \0 */
		*s = '\0';

		switch (c) {
		case '\0':
			PUTS(start);
			continue;

		case 'A':
		case 'a':	/* anchor */
			HTDirEntry(target, tail, data->file_name);
			FormatStr(&buf, start, data->file_name);
			PUTS(buf);
			END(HTML_A);
			*buf = '\0';
#ifdef S_IFLNK
			if (c != 'A' && S_ISLNK(data->file_info.st_mode) &&
			    (len = readlink(file, tmp, sizeof(tmp) - 1)) >= 0) {
				PUTS(" -> ");
				tmp[len] = '\0';
				PUTS(tmp);
			}
#endif
			break;

		case 'T':	/* MIME type */
		case 't':	/* MIME type description */
		    if (S_ISDIR(data->file_info.st_mode)) {
			if (c != 'T') {
			    FormatStr(&buf, start, ENTRY_IS_DIRECTORY);
			} else {
			    FormatStr(&buf, start, "");
			}
		    } else {
			CONST char *cp2;
			HTFormat format;
			format = HTFileFormat(file, NULL, &cp2);

			if (c != 'T') {
			    if (cp2 == NULL) {
				if (!strncmp(HTAtom_name(format),
					     "application",11)) {
				    cp2 = HTAtom_name(format) + 12;
				    if (!strncmp(cp2,"x-",2))
					cp2 += 2;
				} else {
				    cp2 = HTAtom_name(format);
				}
			    }
			    FormatStr(&buf, start, cp2);
			} else {
			    FormatStr(&buf, start, HTAtom_name(format));
			}
		    }
		    break;

		case 'd':	/* date */
			now = time(0);
			datestr = ctime(&data->file_info.st_mtime);
			if ((now - data->file_info.st_mtime) < SEC_PER_YEAR/2)
				/*
				**  MMM DD HH:MM
				*/
				sprintf(tmp, "%.12s", datestr + 4);
			else
				/*
				**  MMM DD  YYYY
				*/
				sprintf(tmp, "%.7s %.4s ", datestr + 4,
					datestr + 20);
			FormatStr(&buf, start, tmp);
			break;

		case 's':	/* size in bytes */
			FormatNum(&buf, start, (int) data->file_info.st_size);
			break;

		case 'K':	/* size in Kilobytes but not for directories */
			if (S_ISDIR(data->file_info.st_mode)) {
				FormatStr(&buf, start, "");
				StrAllocCat(buf, " ");
				break;
			}
			/* FALL THROUGH */
		case 'k':	/* size in Kilobytes */
			FormatNum(&buf, start, (int)((data->file_info.st_size+1023)/1024));
			StrAllocCat(buf, "K");
			break;

		case 'p':	/* unix-style permission bits */
			switch(data->file_info.st_mode & S_IFMT) {
#if defined(_MSC_VER) && defined(_S_IFIFO)
			case _S_IFIFO: type = 'p'; break;
#else
			case S_IFIFO: type = 'p'; break;
#endif
			case S_IFCHR: type = 'c'; break;
			case S_IFDIR: type = 'd'; break;
			case S_IFREG: type = '-'; break;
#ifdef S_IFBLK
			case S_IFBLK: type = 'b'; break;
#endif
#ifdef S_IFLNK
			case S_IFLNK: type = 'l'; break;
#endif
#ifdef S_IFSOCK
# ifdef S_IFIFO		/* some older machines (e.g., apollo) have a conflict */
#  if S_IFIFO != S_IFSOCK
			case S_IFSOCK: type = 's'; break;
#  endif
# else
			case S_IFSOCK: type = 's'; break;
# endif
#endif /* S_IFSOCK */
			default: type = '?'; break;
			}
#ifdef _WINDOWS
			sprintf(tmp, "%c%s", type,
				PBIT(data->file_info.st_mode, 6, data->file_info.st_mode & S_IRWXU));
#else
			sprintf(tmp, "%c%s%s%s", type,
				PBIT(data->file_info.st_mode, 6, data->file_info.st_mode & S_ISUID),
				PBIT(data->file_info.st_mode, 3, data->file_info.st_mode & S_ISGID),
				PTBIT(data->file_info.st_mode,   data->file_info.st_mode & S_ISVTX));
#endif
			FormatStr(&buf, start, tmp);
			break;

		case 'o':	/* owner */
#ifndef NOUSERS
			name = HTAA_UidToName (data->file_info.st_uid);
			if (*name) {
				FormatStr(&buf, start, name);
			} else {
				FormatNum(&buf, start, (int) data->file_info.st_uid);
			}
#endif
			break;

		case 'g':	/* group */
#ifndef NOUSERS
			name = HTAA_GidToName(data->file_info.st_gid);
			if (*name) {
				FormatStr(&buf, start, name);
			} else {
				FormatNum(&buf, start, (int) data->file_info.st_gid);
			}
#endif
			break;

		case 'l':	/* link count */
			FormatNum(&buf, start, (int) data->file_info.st_nlink);
			break;

		case '%':	/* literal % with flags/width */
			FormatStr(&buf, start, "%");
			break;

		default:
			fprintf(stderr,
			"Unknown format character `%c' in list format\n", c);
			break;
		}
		if (buf)
		    PUTS(buf);

		s++;
	}
	FREE(buf);
	END(HTML_PRE);
	PUTC('\n');
	FREE(str);
}
#endif /* LONG_LIST */

/*	Define the representation associated with a file suffix.
**	--------------------------------------------------------
**
**	Calling this with suffix set to "*" will set the default
**	representation.
**	Calling this with suffix set to "*.*" will set the default
**	representation for unknown suffix files which contain a ".".
**
**	The encoding parameter can give a trivial (8bit, 7bit, binary)
**	or real (gzip, compress) encoding.
**
**	If filename suffix is already defined with the same encoding
**	its previous definition is overridden.
*/
PUBLIC void HTSetSuffix5 ARGS5(
	CONST char *,	suffix,
	CONST char *,	representation,
	CONST char *,	encoding,
	CONST char *,	desc,
	double,		value)
{
    HTSuffix * suff;
    BOOL trivial_enc = (BOOL) IsUnityEncStr(encoding);

    if (strcmp(suffix, "*") == 0)
	suff = &no_suffix;
    else if (strcmp(suffix, "*.*") == 0)
	suff = &unknown_suffix;
    else {
	HTList *cur = HTSuffixes;

	while (NULL != (suff = (HTSuffix*)HTList_nextObject(cur))) {
	    if (suff->suffix && 0 == strcmp(suff->suffix, suffix) &&
		((trivial_enc && IsUnityEnc(suff->encoding)) ||
		 (!trivial_enc && !IsUnityEnc(suff->encoding) &&
		     strcmp(encoding, HTAtom_name(suff->encoding)) == 0)))
		break;
	}
	if (!suff) { /* Not found -- create a new node */
	    suff = typecalloc(HTSuffix);
	    if (suff == NULL)
		outofmem(__FILE__, "HTSetSuffix");

	    /*
	    **	Memory leak fixed.
	    **	05-28-94 Lynx 2-3-1 Garrett Arch Blythe
	    */
	    if (!HTSuffixes)	{
		HTSuffixes = HTList_new();
#ifdef LY_FIND_LEAKS
		atexit(free_suffixes);
#endif
	    }

	    HTList_addObject(HTSuffixes, suff);

	    StrAllocCopy(suff->suffix, suffix);
	}
    }

    if (representation)
	suff->rep = HTAtom_for(representation);

    /*
    **	Memory leak fixed.
    **	05-28-94 Lynx 2-3-1 Garrett Arch Blythe
    **	Invariant code removed.
    */
    suff->encoding = HTAtom_for(encoding);

    StrAllocCopy(suff->desc, desc);

    suff->quality = (float) value;
}

#ifdef LY_FIND_LEAKS
/*
**	Purpose:	Free all added suffixes.
**	Arguments:	void
**	Return Value:	void
**	Remarks/Portability/Dependencies/Restrictions:
**		To be used at program exit.
**	Revision History:
**		05-28-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PRIVATE void free_suffixes NOARGS
{
    HTSuffix * suff = NULL;

    /*
    **	Loop through all suffixes.
    */
    while (!HTList_isEmpty(HTSuffixes)) {
	/*
	**  Free off each item and its members if need be.
	*/
	suff = (HTSuffix *)HTList_removeLastObject(HTSuffixes);
	FREE(suff->suffix);
	FREE(suff->desc);
	FREE(suff);
    }
    /*
    **	Free off the list itself.
    */
    HTList_delete(HTSuffixes);
    HTSuffixes = NULL;
}
#endif /* LY_FIND_LEAKS */


/*	Make the cache file name for a W3 document.
**	-------------------------------------------
**	Make up a suitable name for saving the node in
**
**	E.g.	/tmp/WWW_Cache_news/1234@cernvax.cern.ch
**		/tmp/WWW_Cache_http/crnvmc/FIND/xx.xxx.xx
**
**  On exit:
**	Returns a malloc'ed string which must be freed by the caller.
*/
PUBLIC char * HTCacheFileName ARGS1(
	CONST char *,	name)
{
    char * acc_method = HTParse(name, "", PARSE_ACCESS);
    char * host = HTParse(name, "", PARSE_HOST);
    char * path = HTParse(name, "", PARSE_PATH+PARSE_PUNCTUATION);
    char * result = NULL;

    HTSprintf0(&result, "%s/WWW/%s/%s%s", HTCacheRoot, acc_method, host, path);

    FREE(path);
    FREE(acc_method);
    FREE(host);
    return result;
}

/*	Open a file for write, creating the path.
**	-----------------------------------------
*/
#ifdef NOT_IMPLEMENTED
PRIVATE int HTCreatePath ARGS1(CONST char *,path)
{
    return -1;
}
#endif /* NOT_IMPLEMENTED */

/*	Convert filename from URL-path syntax to local path format
**	----------------------------------------------------------
**	Input name is assumed to be the URL-path of a local file
**      URL, i.e. what comes after the "file://localhost".
**      '#'-fragments to be treated as such must already be stripped.
**      If expand_all is FALSE, unescape only escaped '/'. - kw
**
**  On exit:
**	Returns a malloc'ed string which must be freed by the caller.
*/
PUBLIC char * HTURLPath_toFile ARGS3(
	CONST char *,	name,
	BOOL,		expand_all,
	BOOL,		is_remote GCC_UNUSED)
{
    char * path = NULL;
    char * result = NULL;

    StrAllocCopy(path, name);
    if (expand_all)
	HTUnEscape(path);		/* Interpret all % signs */
    else
	HTUnEscapeSome(path, "/");	/* Interpret % signs for path delims */

    CTRACE((tfp, "URLPath `%s' means path `%s'\n", name, path));
#if defined(USE_DOS_DRIVES)
    StrAllocCopy(result, is_remote ? path : HTDOS_name(path));
#else
    StrAllocCopy(result, path);
#endif

    FREE(path);

    return result;
}
/*	Convert filenames between local and WWW formats.
**	------------------------------------------------
**	Make up a suitable name for saving the node in
**
**	E.g.	$(HOME)/WWW/news/1234@cernvax.cern.ch
**		$(HOME)/WWW/http/crnvmc/FIND/xx.xxx.xx
**
**  On exit:
**	Returns a malloc'ed string which must be freed by the caller.
*/
/* NOTE: Don't use this function if you know that the input is a URL path
	 rather than a full URL, use HTURLPath_toFile instead.  Otherwise
	 this function will return the wrong thing for some unusual
	 paths (like ones containing "//", possibly escaped). - kw
*/
PUBLIC char * HTnameOfFile_WWW ARGS3(
	CONST char *,	name,
	BOOL,		WWW_prefix,
	BOOL,		expand_all)
{
    char * acc_method = HTParse(name, "", PARSE_ACCESS);
    char * host = HTParse(name, "", PARSE_HOST);
    char * path = HTParse(name, "", PARSE_PATH+PARSE_PUNCTUATION);
    char * home;
    char * result = NULL;

    if (expand_all) {
	HTUnEscape(path);		/* Interpret all % signs */
    } else
	HTUnEscapeSome(path, "/");	/* Interpret % signs for path delims */

    if (0 == strcmp(acc_method, "file")	/* local file */
     || !*acc_method) {			/* implicitly local? */
	if ((0 == strcasecomp(host, HTHostName())) ||
	    (0 == strcasecomp(host, "localhost")) || !*host) {
	    CTRACE((tfp, "Node `%s' means path `%s'\n", name, path));
	    StrAllocCopy(result, HTSYS_name(path));
	} else if (WWW_prefix) {
	    HTSprintf0(&result, "%s%s%s", "/Net/", host, path);
	    CTRACE((tfp, "Node `%s' means file `%s'\n", name, result));
	} else {
	    StrAllocCopy(result, path);
	}
    } else if (WWW_prefix) {  /* other access */
#ifdef VMS
	if ((home = LYGetEnv("HOME")) == 0)
	    home = HTCacheRoot;
	else
	    home = HTVMS_wwwName(home);
#else
#if defined(_WINDOWS)	/* 1997/10/16 (Thu) 20:42:51 */
	home =  (char *)Home_Dir();
#else
	home = LYGetEnv("HOME");
#endif
	if (home == 0)
	    home = "/tmp";
#endif /* VMS */
	HTSprintf0(&result, "%s/WWW/%s/%s%s", home, acc_method, host, path);
    } else {
	StrAllocCopy(result, path);
    }

    FREE(host);
    FREE(path);
    FREE(acc_method);

    CTRACE((tfp, "HTnameOfFile_WWW(%s,%d,%d) = %s\n",
	    name, WWW_prefix, expand_all, result));

    return result;
}

/*	Make a WWW name from a full local path name.
**	--------------------------------------------
**
**  Bugs:
**	At present, only the names of two network root nodes are hand-coded
**	in and valid for the NeXT only.  This should be configurable in
**	the general case.
*/
PUBLIC char * WWW_nameOfFile ARGS1(
	CONST char *,	name)
{
    char * result = NULL;
#ifdef NeXT
    if (0 == strncmp("/private/Net/", name, 13)) {
	HTSprintf0(&result, "%s//%s", STR_FILE_URL, name+13);
    } else
#endif /* NeXT */
    if (0 == strncmp(HTMountRoot, name, 5)) {
	HTSprintf0(&result, "%s//%s", STR_FILE_URL, name+5);
    } else {
	HTSprintf0(&result, "%s//%s%s", STR_FILE_URL, HTHostName(), name);
    }
    CTRACE((tfp, "File `%s'\n\tmeans node `%s'\n", name, result));
    return result;
}

/*	Determine a suitable suffix, given the representation.
**	------------------------------------------------------
**
**  On entry,
**	rep	is the atomized MIME style representation
**	enc	is an encoding, trivial (8bit, binary, etc.) or gzip etc.
**
**  On exit:
**	Returns a pointer to a suitable suffix string if one has been
**	found, else "".
*/
PUBLIC CONST char * HTFileSuffix ARGS2(
	HTAtom*,	rep,
	CONST char *,	enc)
{
    HTSuffix * suff;
#ifdef FNAMES_8_3
    HTSuffix * first_found = NULL;
#endif
    BOOL trivial_enc;
    int n;
    int i;

#define NO_INIT  /* don't init anymore since I do it in Lynx at startup */
#ifndef NO_INIT
    if (!HTSuffixes)
	HTFileInit();
#endif /* !NO_INIT */

    trivial_enc = (BOOL) IsUnityEncStr(enc);
    n = HTList_count(HTSuffixes);
    for (i = 0; i < n; i++) {
	suff = (HTSuffix *)HTList_objectAt(HTSuffixes, i);
	if (suff->rep == rep &&
#if defined(VMS) || defined(FNAMES_8_3)
	    /*	Don't return a suffix whose first char is a dot, and which
		has more dots or asterisks after that, for
		these systems - kw */
	    (!suff->suffix || !suff->suffix[0] || suff->suffix[0] != '.' ||
	     (strchr(suff->suffix + 1, '.') == NULL &&
	      strchr(suff->suffix + 1, '*') == NULL)) &&
#endif
	    ((trivial_enc && IsUnityEnc(suff->encoding)) ||
	     (!trivial_enc && !IsUnityEnc(suff->encoding) &&
	      strcmp(enc, HTAtom_name(suff->encoding)) == 0))) {
#ifdef FNAMES_8_3
	    if (suff->suffix && (strlen(suff->suffix) <= 4)) {
		/*
		 *  If length of suffix (including dot) is 4 or smaller,
		 *  return this one even if we found a longer one
		 *  earlier - kw
		 */
		return suff->suffix;
	    } else if (!first_found) {
		first_found = suff;		/* remember this one */
	    }
#else
	    return suff->suffix;		/* OK -- found */
#endif
	}
    }
#ifdef FNAMES_8_3
    if (first_found)
	return first_found->suffix;
#endif
    return "";		/* Dunno */
}

/*	Determine file format from file name.
**	-------------------------------------
**
**	This version will return the representation and also set
**	a variable for the encoding.
**
**	Encoding may be a unity encoding (binary, 8bit, etc.) or
**	a content-coding like gzip, compress.
**
**	It will handle for example  x.txt, x.txt,Z, x.Z
*/
PUBLIC HTFormat HTFileFormat ARGS3(
	CONST char *,	filename,
	HTAtom **,	pencoding,
	CONST char**,	pdesc)
{
    HTSuffix * suff;
    int n;
    int i;
    int lf;
#ifdef VMS
    char *semicolon = NULL;
#endif /* VMS */

    if (pencoding)
	*pencoding = NULL;
    if (pdesc)
	*pdesc = NULL;
    if (LYforce_HTML_mode) {
	if (pencoding)
	    *pencoding = WWW_ENC_8BIT;
	return WWW_HTML;
    }

#ifdef VMS
    /*
    **	Trim at semicolon if a version number was
    **	included, so it doesn't interfere with the
    **	code for getting the MIME type. - FM
    */
    if ((semicolon = strchr(filename, ';')) != NULL)
	*semicolon = '\0';
#endif /* VMS */

#ifndef NO_INIT
    if (!HTSuffixes)
	HTFileInit();
#endif /* !NO_INIT */
    lf	= strlen(filename);
    n = HTList_count(HTSuffixes);
    for (i = 0; i < n; i++) {
	int ls;
	suff = (HTSuffix *)HTList_objectAt(HTSuffixes, i);
	ls = strlen(suff->suffix);
	if ((ls <= lf) && 0 == strcasecomp(suff->suffix, filename + lf - ls)) {
	    int j;
	    if (pencoding)
		*pencoding = suff->encoding;
	    if (pdesc)
		*pdesc = suff->desc;
	    if (suff->rep) {
#ifdef VMS
		if (semicolon != NULL)
		    *semicolon = ';';
#endif /* VMS */
		return suff->rep;		/* OK -- found */
	    }
	    for (j = 0; j < n; j++) {  /* Got encoding, need representation */
		int ls2;
		suff = (HTSuffix *)HTList_objectAt(HTSuffixes, j);
		ls2 = strlen(suff->suffix);
		if ((ls + ls2 <= lf) && 0 == strncasecomp(
			suff->suffix, filename + lf - ls -ls2, ls2)) {
		    if (suff->rep) {
			if (pdesc && !(*pdesc))
			    *pdesc = suff->desc;
			if (pencoding && IsUnityEnc(*pencoding) &&
			    *pencoding != WWW_ENC_7BIT &&
			    !IsUnityEnc(suff->encoding))
			    *pencoding = suff->encoding;
#ifdef VMS
			if (semicolon != NULL)
			    *semicolon = ';';
#endif /* VMS */
			return suff->rep;
		    }
		}
	    }

	}
    }

    /* defaults tree */

    suff = strchr(filename, '.') ?	/* Unknown suffix */
	 ( unknown_suffix.rep ? &unknown_suffix : &no_suffix)
	 : &no_suffix;

    /*
    **	Set default encoding unless found with suffix already.
    */
    if (pencoding && !*pencoding)
	*pencoding = suff->encoding ? suff->encoding
				    : HTAtom_for("binary");
#ifdef VMS
    if (semicolon != NULL)
	*semicolon = ';';
#endif /* VMS */
    return suff->rep ? suff->rep : WWW_BINARY;
}

/*	Revise the file format in relation to the Lynx charset. - FM
**	-------------------------------------------------------
**
**	This checks the format associated with an anchor for
**	an extended MIME Content-Type, and if a charset is
**	indicated, sets Lynx up for proper handling in relation
**	to the currently selected character set. - FM
*/
PUBLIC HTFormat HTCharsetFormat ARGS3(
	HTFormat,		format,
	HTParentAnchor *,	anchor,
	int,			default_LYhndl)
{
    char *cp = NULL, *cp1, *cp2, *cp3 = NULL, *cp4;
    BOOL chartrans_ok = FALSE;
    int chndl = -1;

    FREE(anchor->charset);
    StrAllocCopy(cp, format->name);
    LYLowerCase(cp);
    if (((cp1 = strchr(cp, ';')) != NULL) &&
	(cp2 = strstr(cp1, "charset")) != NULL) {
	CTRACE((tfp, "HTCharsetFormat: Extended MIME Content-Type is %s\n",
		    format->name));
	cp2 += 7;
	while (*cp2 == ' ' || *cp2 == '=')
	    cp2++;
	StrAllocCopy(cp3, cp2); /* copy to mutilate more */
	for (cp4 = cp3; (*cp4 != '\0' && *cp4 != '"' &&
			 *cp4 != ';'  && *cp4 != ':' &&
			 !WHITE(*cp4)); cp4++) {
	    ; /* do nothing */
	}
	*cp4 = '\0';
	cp4 = cp3;
	chndl = UCGetLYhndl_byMIME(cp3);
	if (UCCanTranslateFromTo(chndl, current_char_set)) {
	    chartrans_ok = YES;
	    *cp1 = '\0';
	    format = HTAtom_for(cp);
	    StrAllocCopy(anchor->charset, cp4);
	    HTAnchor_setUCInfoStage(anchor, chndl,
				    UCT_STAGE_MIME,
				    UCT_SETBY_MIME);
	} else if (chndl < 0) {
	    /*
	    **	Got something but we don't recognize it.
	    */
	    chndl = UCLYhndl_for_unrec;
	    if (chndl < 0)
	    /*
	    **  UCLYhndl_for_unrec not defined :-(
	    **  fallback to UCLYhndl_for_unspec which always valid.
	    */
	    chndl = UCLYhndl_for_unspec;  /* always >= 0 */
	    if (UCCanTranslateFromTo(chndl, current_char_set)) {
		chartrans_ok = YES;
		HTAnchor_setUCInfoStage(anchor, chndl,
					UCT_STAGE_MIME,
					UCT_SETBY_DEFAULT);
	    }
	}
	if (chartrans_ok) {
	    LYUCcharset *p_in = HTAnchor_getUCInfoStage(anchor,
							UCT_STAGE_MIME);
	    LYUCcharset *p_out = HTAnchor_setUCInfoStage(anchor,
							 current_char_set,
							 UCT_STAGE_HTEXT,
							 UCT_SETBY_DEFAULT);
	    if (!p_out) {
		/*
		**  Try again.
		*/
		p_out = HTAnchor_getUCInfoStage(anchor, UCT_STAGE_HTEXT);
	    }
	    if (!strcmp(p_in->MIMEname, "x-transparent")) {
		HTPassEightBitRaw = TRUE;
		HTAnchor_setUCInfoStage(anchor,
					HTAnchor_getUCLYhndl(anchor,
							     UCT_STAGE_HTEXT),
					UCT_STAGE_MIME,
					UCT_SETBY_DEFAULT);
	    }
	    if (!strcmp(p_out->MIMEname, "x-transparent")) {
		HTPassEightBitRaw = TRUE;
		HTAnchor_setUCInfoStage(anchor,
					HTAnchor_getUCLYhndl(anchor,
							     UCT_STAGE_MIME),
					UCT_STAGE_HTEXT,
					UCT_SETBY_DEFAULT);
	    }
	    if (p_in->enc != UCT_ENC_CJK) {
		HTCJK = NOCJK;
		if (!(p_in->codepoints &
		      UCT_CP_SUBSETOF_LAT1) &&
		    chndl == current_char_set) {
		    HTPassEightBitRaw = TRUE;
		}
	    } else if (p_out->enc == UCT_ENC_CJK) {
		Set_HTCJK(p_in->MIMEname, p_out->MIMEname);
	    }
	} else {
	    /*
	    **  Cannot translate.
	    **  If according to some heuristic the given
	    **  charset and the current display character
	    **  both are likely to be like ISO-8859 in
	    **  structure, pretend we have some kind
	    **  of match.
	    */
	    BOOL given_is_8859
		= (BOOL) (!strncmp(cp4, "iso-8859-", 9) &&
		   isdigit(UCH(cp4[9])));
	    BOOL given_is_8859like
		= (BOOL) (given_is_8859 ||
		   !strncmp(cp4, "windows-", 8) ||
		   !strncmp(cp4, "cp12", 4) ||
		   !strncmp(cp4, "cp-12", 5));
	    BOOL given_and_display_8859like
		= (BOOL) (given_is_8859like &&
		   (strstr(LYchar_set_names[current_char_set],
			   "ISO-8859") ||
		    strstr(LYchar_set_names[current_char_set],
			   "windows-")));

	    if (given_and_display_8859like) {
		*cp1 = '\0';
		format = HTAtom_for(cp);
	    }
	    if (given_is_8859) {
		cp1 = &cp4[10];
		while (*cp1 &&
		       isdigit(UCH(*cp1)))
		    cp1++;
		*cp1 = '\0';
	    }
	    if (given_and_display_8859like) {
		StrAllocCopy(anchor->charset, cp4);
		HTPassEightBitRaw = TRUE;
	    }
	    HTAlert(*cp4 ? cp4 : anchor->charset);
	}
	FREE(cp3);
    } else if (cp1 != NULL) {
	/*
	**  No charset parameter is present.
	**  Ignore all other parameters, as
	**  we do when charset is present. - FM
	*/
	*cp1 = '\0';
	format = HTAtom_for(cp);
    }
    FREE(cp);

    /*
    **	Set up defaults, if needed. - FM
    */
    if (!chartrans_ok && !anchor->charset && default_LYhndl >= 0) {
	HTAnchor_setUCInfoStage(anchor, default_LYhndl,
				UCT_STAGE_MIME,
				UCT_SETBY_DEFAULT);
    }
    HTAnchor_copyUCInfoStage(anchor,
			    UCT_STAGE_PARSER,
			    UCT_STAGE_MIME,
			    -1);

    return format;
}



/*	Get various pieces of meta info from file name.
**	-----------------------------------------------
**
**  LYGetFileInfo fills in information that can be determined without
**  an actual (new) access to the filesystem, based on current suffix
**  and character set configuration.  If the file has been loaded and
**  parsed before  (with the same URL generated here!) and the anchor
**  is still around, some results may be influenced by that (in
**  particular, charset info from a META tag - this is not actually
**  tested!).
**  The caller should not keep pointers to the returned objects around
**  for too long, the valid lifetimes vary. In particular, the returned
**  charset string should be copied if necessary.  If return of the
**  file_anchor is requested, that one can be used to retrieve
**  additional bits of info that are stored in the anchor object and
**  are not covered here; as usual, don't keep pointers to the
**  file_anchor longer than necessary since the object may disappear
**  through HTuncache_current_document or at the next document load.
**  - kw
*/
PUBLIC void LYGetFileInfo ARGS7(
	CONST char *,		filename,
	HTParentAnchor **,	pfile_anchor,
	HTFormat *,		pformat,
	HTAtom **,		pencoding,
	CONST char**,		pdesc,
	CONST char**,		pcharset,
	int *,			pfile_cs)
{
	char *Afn;
	char *Aname = NULL;
	HTFormat format;
	HTAtom * myEnc = NULL;
	HTParentAnchor *file_anchor;
	CONST char *file_csname;
	int file_cs;

	/*
	 *  Convert filename to URL.  Note that it is always supposed to
	 *  be a filename, not maybe-filename-maybe-URL, so we don't
	 *  use LYFillLocalFileURL and LYEnsureAbsoluteURL. - kw
	 */
	Afn = HTEscape(filename, URL_PATH);
	LYLocalFileToURL(&Aname, Afn);
	file_anchor = HTAnchor_findSimpleAddress(Aname);

	file_csname = file_anchor->charset;
	format = HTFileFormat(filename, &myEnc, pdesc);
	format = HTCharsetFormat(format, file_anchor, UCLYhndl_HTFile_for_unspec);
	file_cs = HTAnchor_getUCLYhndl(file_anchor, UCT_STAGE_MIME);
	if (!file_csname) {
	    if (file_cs >= 0)
		file_csname = LYCharSet_UC[file_cs].MIMEname;
	    else file_csname = "display character set";
	}
	CTRACE((tfp, "GetFileInfo: '%s' is a%s %s %s file, charset=%s (%d).\n",
	       filename,
	       ((myEnc && *HTAtom_name(myEnc) == '8') ? "n" : myEnc ? "" :
		*HTAtom_name(format) == 'a' ? "n" : ""),
	       myEnc ? HTAtom_name(myEnc) : "",
	       HTAtom_name(format),
	       file_csname,
	       file_cs));
	FREE(Afn);
	FREE(Aname);
	if (pfile_anchor)
	    *pfile_anchor = file_anchor;
	if (pformat)
	    *pformat = format;
	if (pencoding)
	    *pencoding = myEnc;
	if (pcharset)
	    *pcharset = file_csname;
	if (pfile_cs)
	    *pfile_cs = file_cs;
    }

/*	Determine value from file name.
**	-------------------------------
**
*/
PUBLIC float HTFileValue ARGS1(
	CONST char *,	filename)
{
    HTSuffix * suff;
    int n;
    int i;
    int lf = strlen(filename);

#ifndef NO_INIT
    if (!HTSuffixes)
	HTFileInit();
#endif /* !NO_INIT */
    n = HTList_count(HTSuffixes);
    for (i = 0; i < n; i++) {
	int ls;
	suff = (HTSuffix *)HTList_objectAt(HTSuffixes, i);
	ls = strlen(suff->suffix);
	if ((ls <= lf) && 0==strcmp(suff->suffix, filename + lf - ls)) {
	    CTRACE((tfp, "File: Value of %s is %.3f\n",
			filename, suff->quality));
	    return suff->quality;		/* OK -- found */
	}
    }
    return (float)0.3;		/* Dunno! */
}

/*
**  Determine compression type from file name, by looking at its suffix.
**  Sets as side-effect a pointer to the "dot" that begins the suffix.
*/
PUBLIC CompressFileType HTCompressFileType ARGS3(
	char *,		filename,
	char *,		dots,
	char **,	suffix)
{
    CompressFileType result = cftNone;
    size_t len = strlen(filename);
    char *ftype = filename + len;

    if ((len > 4)
     && !strcasecomp((ftype - 3), "bz2")
     && strchr(dots, ftype[-4]) != 0) {
	result = cftBzip2;
	ftype -= 4;
    } else if ((len > 3)
     && !strcasecomp((ftype - 2), "gz")
     && strchr(dots, ftype[-3]) != 0) {
	result = cftGzip;
	ftype -= 3;
    } else if ((len > 2)
     && !strcmp((ftype - 1), "Z")
     && strchr(dots, ftype[-2]) != 0) {
	result = cftCompress;
	ftype -= 2;
    }

    *suffix = ftype;
    CTRACE((tfp, "HTCompressFileType(%s) returns %d:%s\n",
		 filename, result, *suffix));
    return result;
}

/*	Determine write access to a file.
**	---------------------------------
**
**  On exit:
**	Returns YES if file can be accessed and can be written to.
**
**  Bugs:
**	1.	No code for non-unix systems.
**	2.	Isn't there a quicker way?
*/
PUBLIC BOOL HTEditable ARGS1(
	CONST char *,	filename)
{
#ifndef NO_GROUPS
    GETGROUPS_T groups[NGROUPS];
    uid_t	myUid;
    int		ngroups;			/* The number of groups	 */
    struct stat fileStatus;
    int		i;

    if (stat(filename, &fileStatus))		/* Get details of filename */
	return NO;				/* Can't even access file! */

    ngroups = getgroups(NGROUPS, groups);	/* Groups to which I belong  */
    myUid = geteuid();				/* Get my user identifier */

    if (TRACE) {
	int i2;
	fprintf(tfp,
	    "File mode is 0%o, uid=%d, gid=%d. My uid=%d, %d groups (",
	    (unsigned int) fileStatus.st_mode,
	    (int) fileStatus.st_uid,
	    (int) fileStatus.st_gid,
	    (int) myUid,
	    (int) ngroups);
	for (i2 = 0; i2 < ngroups; i2++)
	    fprintf(tfp, " %d", (int) groups[i2]);
	fprintf(tfp, ")\n");
    }

    if (fileStatus.st_mode & 0002)		/* I can write anyway? */
	return YES;

    if ((fileStatus.st_mode & 0200)		/* I can write my own file? */
     && (fileStatus.st_uid == myUid))
	return YES;

    if (fileStatus.st_mode & 0020)		/* Group I am in can write? */
    {
	for (i = 0; i < ngroups; i++) {
	    if (groups[i] == fileStatus.st_gid)
		return YES;
	}
    }
    CTRACE((tfp, "\tFile is not editable.\n"));
#endif /* NO_GROUPS */
    return NO;					/* If no excuse, can't do */
}

/*	Make a save stream.
**	-------------------
**
**	The stream must be used for writing back the file.
**	@@@ no backup done
*/
PUBLIC HTStream * HTFileSaveStream ARGS1(
	HTParentAnchor *,	anchor)
{
    CONST char * addr = anchor->address;
    char * localname = HTLocalName(addr);
    FILE * fp = fopen(localname, BIN_W);

    FREE(localname);
    if (!fp)
	return NULL;

    return HTFWriter_new(fp);
}

/*	Output one directory entry.
**	---------------------------
*/
PUBLIC void HTDirEntry ARGS3(
	HTStructured *, target,
	CONST char *,	tail,
	CONST char *,	entry)
{
    char * relative = NULL;
    char * stripped = NULL;
    char * escaped = NULL;
    int len;

    StrAllocCopy(escaped, entry);
    LYTrimPathSep(escaped);
    if (strcmp(escaped, "..") != 0) {
	stripped = escaped;
	escaped = HTEscape(stripped, URL_XPALPHAS);
	if (((len = strlen(escaped)) > 2) &&
	    escaped[(len - 3)] == '%' &&
	    escaped[(len - 2)] == '2' &&
	    TOUPPER(escaped[(len - 1)]) == 'F') {
	    escaped[(len - 3)] = '\0';
	}
    }

    if (tail == NULL || *tail == '\0') {
	/*
	**  Handle extra slash at end of path.
	*/
	HTStartAnchor(target, NULL, (escaped[0] != '\0' ? escaped : "/"));
    } else {
	/*
	**  If empty tail, gives absolute ref below.
	*/
	relative = 0;
	HTSprintf0(&relative, "%s%s%s",
			   tail,
			   (*escaped != '\0' ? "/" : ""),
			   escaped);
	HTStartAnchor(target, NULL, relative);
	FREE(relative);
    }
    FREE(stripped);
    FREE(escaped);
}

/*	Output parent directory entry.
**	------------------------------
**
**    This gives the TITLE and H1 header, and also a link
**    to the parent directory if appropriate.
**
**  On exit:
**	Returns TRUE if an "Up to <parent>" link was not created
**	for a readable local directory because LONG_LIST is defined
**	and NO_PARENT_DIR_REFERENCE is not defined, so that the
**	calling function should use LYListFmtParse() to create a link
**	to the parent directory.  Otherwise, it returns FALSE. - FM
*/
PUBLIC BOOL HTDirTitles ARGS3(
	HTStructured *, target,
	HTParentAnchor *, anchor,
	BOOL,		tildeIsTop)
{
    CONST char * logical = anchor->address;
    char * path = HTParse(logical, "", PARSE_PATH + PARSE_PUNCTUATION);
    char * current;
    char * cp = NULL;
    BOOL need_parent_link = FALSE;
    int i;
#if defined(USE_DOS_DRIVES)
    BOOL local_link = (strlen(logical) > 18
		     && !strncasecomp(logical, "file://localhost/", 17)
		     && LYIsDosDrive(logical + 17));
    BOOL is_remote = !local_link;
#else
#define is_remote TRUE
#endif

    /*
    **	Check tildeIsTop for treating home directory as Welcome
    **	(assume the tilde is not followed by a username). - FM
    */
    if (tildeIsTop && !strncmp(path, "/~", 2)) {
	if (path[2] == '\0') {
	    path[1] = '\0';
	} else {
	    for (i = 0; path[(i + 2)]; i++) {
		path[i] = path[(i + 2)];
	    }
	    path[i] = '\0';
	}
    }

    /*
    **	Trim out the ;type= parameter, if present. - FM
    */
    if ((cp = strrchr(path, ';')) != NULL) {
	if (!strncasecomp((cp+1), "type=", 5)) {
	    if (TOUPPER(*(cp+6)) == 'D' ||
		TOUPPER(*(cp+6)) == 'A' ||
		TOUPPER(*(cp+6)) == 'I')
		*cp = '\0';
	}
	cp = NULL;
    }
    current = LYPathLeaf (path);	/* last part or "" */

    {
      char * printable = NULL;

#ifdef DIRED_SUPPORT
      printable = HTURLPath_toFile(
	    (0 == strncasecomp(path, "/%2F", 4))	/* "//" ? */
	    ? (path+1)
	    : path,
	    TRUE,
	    is_remote);
      if (0 == strncasecomp(printable, "/vmsysu:", 8) ||
	  0 == strncasecomp(printable, "/anonymou.", 10)) {
	  StrAllocCopy(cp, (printable+1));
	  StrAllocCopy(printable, cp);
	  FREE(cp);
      }
#else
      StrAllocCopy(printable, current);
      HTUnEscape(printable);
#endif /* DIRED_SUPPORT */

      START(HTML_HEAD);
      PUTC('\n');
      START(HTML_TITLE);
      PUTS(*printable ? printable : WELCOME_MSG);
      PUTS(SEGMENT_DIRECTORY);
      END(HTML_TITLE);
      PUTC('\n');
      END(HTML_HEAD);
      PUTC('\n');

#ifdef DIRED_SUPPORT
      START(HTML_H2);
      PUTS(*printable ? SEGMENT_CURRENT_DIR : "");
      PUTS(*printable ? printable : WELCOME_MSG);
      END(HTML_H2);
      PUTC('\n');
#else
      START(HTML_H1);
      PUTS(*printable ? printable : WELCOME_MSG);
      END(HTML_H1);
      PUTC('\n');
#endif /* DIRED_SUPPORT */
      if (((0 == strncasecomp(printable, "vmsysu:", 7)) &&
	   (cp = strchr(printable, '.')) != NULL &&
	   strchr(cp, '/') == NULL) ||
	  (0 == strncasecomp(printable, "anonymou.", 9) &&
	   strchr(printable, '/') == NULL)) {
	  FREE(printable);
	  FREE(path);
	  return(need_parent_link);
      }
      FREE(printable);
    }

#ifndef NO_PARENT_DIR_REFERENCE
    /*
    **	Make link back to parent directory.
    */
    if (current - path > 0
      && LYIsPathSep(current[-1])
      && current[0] != '\0') {	/* was a slash AND something else too */
	char * parent = NULL;
	char * relative = NULL;

	current[-1] = '\0';
	parent = strrchr(path, '/');  /* penultimate slash */

	if ((parent &&
	     (!strcmp(parent, "/..") ||
	      !strncasecomp(parent, "/%2F", 4))) ||
	    !strncasecomp(current, "%2F", 3)) {
	    FREE(path);
	    return(need_parent_link);
	}

	relative = 0;
	HTSprintf0(&relative, "%s/..", current);

#if defined(DOSPATH) || defined(__EMX__)
	if (local_link) {
	    if (parent != 0 && strlen(parent) == 3 ) {
		StrAllocCat(relative, "/.");
	    }
	}
	else
#endif

#if !defined (VMS)
	{
	    /*
	    **	On Unix, if it's not ftp and the directory cannot
	    **	be read, don't put out a link.
	    **
	    **	On VMS, this problem is dealt with internally by
	    **	HTVMSBrowseDir().
	    */
	    DIR  * dp = NULL;

	    if (LYisLocalFile(logical)) {
		/*
		**  We need an absolute file path for the opendir.
		**  We also need to unescape for this test.
		**  Don't worry about %2F now, they presumably have been
		**  dealt with above, and shouldn't appear for local
		**  files anyway...  Assume OS / filesystem will just
		**  ignore superfluous slashes. - KW
		*/
		char * fullparentpath = NULL;

		/*
		**  Path has been shortened above.
		*/
		StrAllocCopy(fullparentpath, *path ? path : "/");

		/*
		**  Guard against weirdness.
		*/
		if (0 == strcmp(current,"..")) {
		    StrAllocCat(fullparentpath,"/../..");
		} else if (0 == strcmp(current,".")) {
		    StrAllocCat(fullparentpath,"/..");
		}

		HTUnEscape(fullparentpath);
		if ((dp = opendir(fullparentpath)) == NULL) {
		    FREE(fullparentpath);
		    FREE(relative);
		    FREE(path);
		    return(need_parent_link);
		}
		closedir(dp);
		FREE(fullparentpath);
#ifdef LONG_LIST
		need_parent_link = TRUE;
		FREE(path);
		FREE(relative);
		return(need_parent_link);
#endif /* LONG_LIST */
	    }
	}
#endif /* !VMS */
	HTStartAnchor(target, "", relative);
	FREE(relative);

	PUTS(SEGMENT_UP_TO);
	if (parent) {
	    if ((0 == strcmp(current,".")) ||
		(0 == strcmp(current,".."))) {
		/*
		**  Should not happen, but if it does,
		**  at least avoid giving misleading info. - KW
		*/
		PUTS("..");
	    } else {
		char * printable = NULL;
		StrAllocCopy(printable, parent + 1);
		HTUnEscape(printable);
		PUTS(printable);
		FREE(printable);
	    }
	} else {
	    PUTC('/');
	}
	END(HTML_A);
	PUTC('\n');
    }
#endif /* !NO_PARENT_DIR_REFERENCE */

    FREE(path);
    return(need_parent_link);
}

#if defined HAVE_READDIR
/*	Send README file.
**	-----------------
**
**  If a README file exists, then it is inserted into the document here.
*/
PRIVATE void do_readme ARGS2(HTStructured *, target, CONST char *, localname)
{
    FILE * fp;
    char * readme_file_name = NULL;
    int ch;

    HTSprintf0(&readme_file_name, "%s/%s", localname, HT_DIR_README_FILE);

    fp = fopen(readme_file_name, "r");

    if (fp) {
	HTStructuredClass targetClass;

	targetClass =  *target->isa;	/* (Can't init agregate in K&R) */
	START(HTML_PRE);
	while ((ch = fgetc(fp)) != EOF) {
	    PUTC((char)ch);
	}
	END(HTML_PRE);
	HTDisplayPartial();
	fclose(fp);
    }
    FREE(readme_file_name);
}

#define DIRED_BLOK(obj) (((DIRED *)(obj))->sort_tags)
#define DIRED_NAME(obj) (((DIRED *)(obj))->file_name)

#define NM_cmp(a,b) ((a) < (b) ? -1 : ((a) > (b) ? 1 : 0))

#if defined(LONG_LIST) && defined(DIRED_SUPPORT)
PRIVATE char *file_type ARGS1(char *, path)
{
    char *type;
    while (*path == '.')
	++path;
    type = strchr(path, '.');
    if (type == NULL)
	type = "";
    return type;
}
#endif /* LONG_LIST && DIRED_SUPPORT */

PRIVATE int dired_cmp ARGS2(void *, a, void *, b)
{
    DIRED *p = (DIRED *)a;
    DIRED *q = (DIRED *)b;
    int code = p->sort_tags - q->sort_tags;
#if defined(LONG_LIST) && defined(DIRED_SUPPORT)
    if (code == 0) {
	switch (dir_list_order) {
	case ORDER_BY_SIZE:
	    code = -NM_cmp(p->file_info.st_size, q->file_info.st_size);
	    break;
	case ORDER_BY_DATE:
	    code = -NM_cmp(p->file_info.st_mtime, q->file_info.st_mtime);
	    break;
	case ORDER_BY_MODE:
	    code = NM_cmp(p->file_info.st_mode, q->file_info.st_mode);
	    break;
	case ORDER_BY_USER:
	    code = NM_cmp(p->file_info.st_uid, q->file_info.st_uid);
	    break;
	case ORDER_BY_GROUP:
	    code = NM_cmp(p->file_info.st_gid, q->file_info.st_gid);
	    break;
	case ORDER_BY_TYPE:
	    code = AS_cmp(file_type(p->file_name), file_type(q->file_name));
	    break;
	default:
	    code = 0;
	    break;
	}
    }
#endif /* LONG_LIST && DIRED_SUPPORT */
    if (code == 0)
	code = AS_cmp(p->file_name, q->file_name);
#if 0
    CTRACE((tfp, "dired_cmp(%d) ->%d\n\t%c:%s (%s)\n\t%c:%s (%s)\n",
	    dir_list_order,
	    code,
	    p->sort_tags, p->file_name, file_type(p->file_name),
	    q->sort_tags, q->file_name, file_type(q->file_name)));
#endif
    return code;
}

PRIVATE int print_local_dir ARGS5(
	DIR  *,			dp,
	char *,			localname,
	HTParentAnchor *,	anchor,
	HTFormat,		format_out,
	HTStream *,		sink)
{
    HTStructured *target;	/* HTML object */
    HTStructuredClass targetClass;
    STRUCT_DIRENT * dirbuf;
    char *pathname = NULL;
    char *tail = NULL;
    BOOL present[HTML_A_ATTRIBUTES];
    char * tmpfilename = NULL;
    BOOL need_parent_link = FALSE;
    int status;
    int i;

    CTRACE((tfp, "print_local_dir() started\n"));

    pathname = HTParse(anchor->address, "",
		       PARSE_PATH + PARSE_PUNCTUATION);

    if (!strcmp(pathname,"/")) {
	/*
	**  Root path.
	*/
	StrAllocCopy (tail, "/foo/..");
    } else {
	char *p = strrchr(pathname, '/');  /* find last slash */

	if (!p) {
	    /*
	    **	This probably should not happen,
	    **	but be prepared if it does. - KW
	    */
	    StrAllocCopy (tail, "/foo/..");
	} else {
	    /*
	    **	Take slash off the beginning.
	    */
	    StrAllocCopy(tail, (p + 1));
	}
    }
    FREE(pathname);

    if (UCLYhndl_HTFile_for_unspec >= 0) {
	HTAnchor_setUCInfoStage(anchor,
				UCLYhndl_HTFile_for_unspec,
				UCT_STAGE_PARSER,
				UCT_SETBY_DEFAULT);
    }

    target = HTML_new(anchor, format_out, sink);
    targetClass = *target->isa;	    /* Copy routine entry points */

    for (i = 0; i < HTML_A_ATTRIBUTES; i++)
	present[i] = (BOOL) (i == HTML_A_HREF);

    /*
    **	The need_parent_link flag will be set if an
    **	"Up to <parent>" link was not created for a
    **	readable parent in HTDirTitles() because
    **	LONG_LIST is defined and NO_PARENT_DIR_REFERENCE
    **	is not defined so that need we to create the
    **	link via an LYListFmtParse() call. - FM
    */
    need_parent_link = HTDirTitles(target, anchor, FALSE);

#ifdef DIRED_SUPPORT
    if (!isLYNXCGI(anchor->address)) {
	HTAnchor_setFormat(anchor, WWW_DIRED);
	lynx_edit_mode = TRUE;
    }
#endif /* DIRED_SUPPORT */
    if (HTDirReadme == HT_DIR_README_TOP)
	do_readme(target, localname);

    {
	HTBTree * bt = HTBTree_new(dired_cmp);
	int num_of_entries = 0;	    /* lines counter */

	_HTProgress (READING_DIRECTORY);
	status = HT_LOADED; /* assume we don't get interrupted */
	while ((dirbuf = readdir(dp)) != NULL) {
	    /*
	    **	While there are directory entries to be read...
	    */
	    DIRED *data = NULL;

#if !(defined(DOSPATH) || defined(__EMX__))
	    if (dirbuf->d_ino == 0)
		/*
		**  If the entry is not being used, skip it.
		*/
		continue;
#endif
	    /*
	    **	Skip self, parent if handled in HTDirTitles()
	    **	or if NO_PARENT_DIR_REFERENCE is not defined,
	    **	and any dot files if no_dotfiles is set or
	    **	show_dotfiles is not set. - FM
	    */
	    if (!strcmp(dirbuf->d_name, ".")   /* self	 */ ||
		(!strcmp(dirbuf->d_name, "..") /* parent */ &&
		 need_parent_link == FALSE) ||
		((strcmp(dirbuf->d_name, "..")) &&
		 (dirbuf->d_name[0] == '.' &&
		  (no_dotfiles || !show_dotfiles))))
		continue;

	    StrAllocCopy(tmpfilename, localname);
	    /*
	    **  If filename is not root directory, add trailing separator.
	    */
	    LYAddPathSep(&tmpfilename);

	    StrAllocCat(tmpfilename, dirbuf->d_name);
	    data = (DIRED *)malloc(sizeof(DIRED) + strlen(dirbuf->d_name) + 4);
	    if (data == NULL) {
		/* FIXME */
	    }
	    LYTrimPathSep (tmpfilename);
	    if (lstat(tmpfilename, &(data->file_info)) < 0)
		data->file_info.st_mode = 0;

	    strcpy(data->file_name, dirbuf->d_name);
#ifndef DIRED_SUPPORT
	    if (S_ISDIR(data->file_info.st_mode)) {
		data->sort_tags = 'D';
	    } else {
		data->sort_tags = 'F';
		/* D & F to have first directories, then files */
	    }
#else
	    if (S_ISDIR(data->file_info.st_mode)) {
		if (dir_list_style == MIXED_STYLE) {
		    data->sort_tags = ' ';
		    LYAddPathSep0(data->file_name);
		} else if (!strcmp(dirbuf->d_name, "..")) {
		    data->sort_tags = 'A';
		} else {
		    data->sort_tags = 'D';
		}
	    } else if (dir_list_style == MIXED_STYLE) {
		data->sort_tags = ' ';
	    } else if (dir_list_style == FILES_FIRST) {
		data->sort_tags = 'C';
		/* C & D to have first files, then directories */
	    } else {
		data->sort_tags = 'F';
	    }
#endif /* !DIRED_SUPPORT */
	    /*
	    **	Sort dirname in the tree bt.
	    */
	    HTBTree_add(bt, data);

#ifdef DISP_PARTIAL
	    /* optimize for expensive operation: */
	    if (num_of_entries % (partial_threshold > 0  ?
				  partial_threshold : display_lines)
			       == 0) {
		if (HTCheckForInterrupt()) {
		    status = HT_PARTIAL_CONTENT;
		    break;
		}
	    }
	    num_of_entries++;
#endif /* DISP_PARTIAL */

	}   /* end while directory entries left to read */

	if (status != HT_PARTIAL_CONTENT)
	    _HTProgress (OPERATION_OK);
	else
	    CTRACE((tfp, "Reading the directory interrupted by user\n"));


	/*
	**  Run through tree printing out in order.
	*/
	{
	    HTBTElement * next_element = HTBTree_next(bt,NULL);
		/* pick up the first element of the list */
	    int num_of_entries_output = 0; /* lines counter */

	    char state;
		/* I for initial (.. file),
		   D for directory file,
		   F for file */

#ifdef DIRED_SUPPORT
	    char test;
#endif /* DIRED_SUPPORT */
	    state = 'I';

	    while (next_element != NULL) {
		DIRED *entry;

#ifndef DISP_PARTIAL
		if (num_of_entries_output % HTMAX(display_lines,10) == 0) {
		    if (HTCheckForInterrupt()) {
			_HTProgress (TRANSFER_INTERRUPTED);
			status = HT_PARTIAL_CONTENT;
			break;
		    }
		}
#endif
		StrAllocCopy(tmpfilename, localname);
		/*
		**	If filename is not root directory.
		*/
		LYAddPathSep(&tmpfilename);

		entry = (DIRED *)(HTBTree_object(next_element));
		/*
		**  Append the current entry's filename
		**  to the path.
		*/
		StrAllocCat(tmpfilename, entry->file_name);
		HTSimplify(tmpfilename);
		/*
		**  Output the directory entry.
		*/
		if (strcmp(DIRED_NAME(HTBTree_object(next_element)), "..")) {
#ifdef DIRED_SUPPORT
		    test = (DIRED_BLOK(HTBTree_object(next_element))
			    == 'D' ? 'D' : 'F');
		    if (state != test) {
#ifndef LONG_LIST
			if (dir_list_style == FILES_FIRST) {
			    if (state == 'F') {
				END(HTML_DIR);
				PUTC('\n');
			    }
			} else if (dir_list_style != MIXED_STYLE)
			    if (state == 'D') {
				END(HTML_DIR);
				PUTC('\n');
			    }
#endif /* !LONG_LIST */
			state =
			   (char) (DIRED_BLOK(HTBTree_object(next_element))
			    == 'D' ? 'D' : 'F');
			START(HTML_H2);
			if (dir_list_style != MIXED_STYLE) {
			   START(HTML_EM);
			   PUTS(state == 'D'
			      ? LABEL_SUBDIRECTORIES
			      : LABEL_FILES);
			   END(HTML_EM);
			}
			END(HTML_H2);
			PUTC('\n');
#ifndef LONG_LIST
			START(HTML_DIR);
			PUTC('\n');
#endif /* !LONG_LIST */
		    }
#else
		    if (state != DIRED_BLOK(HTBTree_object(next_element))) {
#ifndef LONG_LIST
			if (state == 'D') {
			    END(HTML_DIR);
			    PUTC('\n');
			}
#endif /* !LONG_LIST */
			state =
			  (char) (DIRED_BLOK(HTBTree_object(next_element))
			   == 'D' ? 'D' : 'F');
			START(HTML_H2);
			START(HTML_EM);
			PUTS(state == 'D'
			    ? LABEL_SUBDIRECTORIES
			    : LABEL_FILES);
			END(HTML_EM);
			END(HTML_H2);
			PUTC('\n');
#ifndef LONG_LIST
			START(HTML_DIR);
			PUTC('\n');
#endif /* !LONG_LIST */
		    }
#endif /* DIRED_SUPPORT */
#ifndef LONG_LIST
		    START(HTML_LI);
#endif /* !LONG_LIST */
		}

#ifdef LONG_LIST
		LYListFmtParse(list_format, entry, tmpfilename, target, tail);
#else
		HTDirEntry(target, tail, entry->file_name);
		PUTS(entry->file_name);
		END(HTML_A);
		MAYBE_END(HTML_LI);
		PUTC('\n');
#endif /* LONG_LIST */

		next_element = HTBTree_next(bt, next_element);
		    /* pick up the next element of the list;
		     if none, return NULL*/

		/* optimize for expensive operation: */
#ifdef DISP_PARTIAL
		if (num_of_entries_output %
		    (partial_threshold > 0 ? partial_threshold : display_lines)
		    == 0) {
		    /* num_of_entries, num_of_entries_output... */
		    /* HTReadProgress...(bytes, 0); */
		    HTDisplayPartial();

		    if (HTCheckForInterrupt()) {
			_HTProgress (TRANSFER_INTERRUPTED);
			status = HT_PARTIAL_CONTENT;
			break;
		    }
		}
		num_of_entries_output++;
#endif /* DISP_PARTIAL */

	    } /* end while next_element */

	    if (status == HT_LOADED) {
		if (state == 'I') {
		    START(HTML_P);
		    PUTS("Empty Directory");
		}
#ifndef LONG_LIST
		else
		    END(HTML_DIR);
#endif /* !LONG_LIST */
	    }
	} /* end printing out the tree in order */

	FREE(tmpfilename);
	FREE(tail);
	HTBTreeAndObject_free(bt);

	if (status == HT_LOADED) {
	    if (HTDirReadme == HT_DIR_README_BOTTOM)
		do_readme(target, localname);
	    FREE_TARGET;
	} else {
	    ABORT_TARGET;
	}
    }
    HTFinishDisplayPartial();
    return status;  /* document loaded, maybe partial */
}
#endif /* HAVE_READDIR */


#ifndef VMS
PUBLIC int HTStat ARGS2(
	CONST char *,	filename,
	struct stat *,	data)
{
    int result = -1;
    size_t len = strlen(filename);

    if (len != 0 && LYIsPathSep(filename[len-1])) {
	char *temp_name = NULL;
	HTSprintf0(&temp_name, "%s.", filename);
	result = HTStat(temp_name, data);
	FREE(temp_name);
    } else {
	result = stat(filename, data);
#ifdef _WINDOWS
	/*
	 * Someone claims that stat() doesn't give the proper result for a
	 * directory on Windows.
	 */
	if (result == -1
	 && access(filename, 0) == 0) {
	    data->st_mode = S_IFDIR;
	    result = 0;
	}
#endif
    }
    return result;
}
#endif

/*	Load a document.
**	----------------
**
**  On entry:
**	addr		must point to the fully qualified hypertext reference.
**			This is the physical address of the file
**
**  On exit:
**	returns		<0		Error has occurred.
**			HTLOADED	OK
**
*/
PUBLIC int HTLoadFile ARGS4(
	CONST char *,		addr,
	HTParentAnchor *,	anchor,
	HTFormat,		format_out,
	HTStream *,		sink)
{
    char * filename = NULL;
    char * acc_method = NULL;
    char * ftp_newhost;
    HTFormat format;
    char * nodename = NULL;
    char * newname = NULL;	/* Simplified name of file */
    HTAtom * encoding;		/* @@ not used yet */
    HTAtom * myEncoding = NULL; /* enc of this file, may be gzip etc. */
    int status = -1;
    char *dot;
#ifdef VMS
    struct stat stat_info;
#endif /* VMS */
#ifdef USE_ZLIB
    gzFile gzfp = 0;
#endif /* USE_ZLIB */
#ifdef USE_BZLIB
    BZFILE *bzfp = 0;
#endif /* USE_ZLIB */
#if defined(USE_ZLIB) || defined(USE_BZLIB)
    CompressFileType internal_decompress = cftNone;
    BOOL failed_decompress = NO;
#endif

    /*
    **	Reduce the filename to a basic form (hopefully unique!).
    */
    StrAllocCopy(newname, addr);
    filename=HTParse(newname, "", PARSE_PATH|PARSE_PUNCTUATION);
    nodename=HTParse(newname, "", PARSE_HOST);

    /*
    **	If access is ftp, or file is on another host, invoke ftp now.
    */
    acc_method = HTParse(newname, "", PARSE_ACCESS);
    if (strcmp("ftp", acc_method) == 0 ||
       (!LYSameHostname("localhost", nodename) &&
	!LYSameHostname(nodename, HTHostName()))) {
	status = -1;
	FREE(newname);
	FREE(filename);
	FREE(nodename);
	FREE(acc_method);
#ifndef DISABLE_FTP
	ftp_newhost = HTParse(addr, "", PARSE_HOST);
	if (strcmp(ftp_lasthost, ftp_newhost))
	    ftp_local_passive = ftp_passive;

	status = HTFTPLoad(addr, anchor, format_out, sink);

	if ( ftp_passive == ftp_local_passive ) {
	    if (( status >= 400 ) || ( status < 0 )) {
		ftp_local_passive = !ftp_passive;
		status = HTFTPLoad(addr, anchor, format_out, sink);
	    }
	}

	free(ftp_lasthost);
	ftp_lasthost = ftp_newhost;
#endif /* DISABLE_FTP */
	return status;
    } else {
	FREE(newname);
	FREE(acc_method);
    }
#if defined(VMS) || defined(USE_DOS_DRIVES)
    HTUnEscape(filename);
#endif /* VMS */

    /*
    **	Determine the format and encoding mapped to any suffix.
    */
    if (anchor->content_type && anchor->content_encoding) {
	/*
	 *  If content_type and content_encoding are BOTH already set
	 *  in the anchor object, we believe it and don't try to
	 *  derive format and encoding from the filename. - kw
	 */
	format = HTAtom_for(anchor->content_type);
	myEncoding = HTAtom_for(anchor->content_encoding);
    } else {
	int default_UCLYhndl = UCLYhndl_HTFile_for_unspec;

	if (force_old_UCLYhndl_on_reload) {
	    force_old_UCLYhndl_on_reload = FALSE;
	    default_UCLYhndl = forced_UCLYhdnl;
	}

	format = HTFileFormat(filename, &myEncoding, NULL);

    /*
    **	Check the format for an extended MIME charset value, and
    **	act on it if present.  Otherwise, assume what is indicated
    **	by the last parameter (fallback will effectively be
    **	UCLYhndl_for_unspec, by default ISO-8859-1). - kw
    */
	format = HTCharsetFormat(format, anchor, default_UCLYhndl );
    }

#ifdef VMS
    /*
    **	Check to see if the 'filename' is in fact a directory.	If it is
    **	create a new hypertext object containing a list of files and
    **	subdirectories contained in the directory.  All of these are links
    **	to the directories or files listed.
    */
    if (HTStat(filename, &stat_info) == -1) {
	CTRACE((tfp, "HTLoadFile: Can't stat %s\n", filename));
    } else {
	if (S_ISDIR(stat_info.st_mode)) {
	    if (HTDirAccess == HT_DIR_FORBID) {
		FREE(filename);
		FREE(nodename);
		return HTLoadError(sink, 403, DISALLOWED_DIR_SCAN);
	    }

	    if (HTDirAccess == HT_DIR_SELECTIVE) {
		char * enable_file_name = NULL;

		HTSprintf0(&enable_file_name, "%s/%s", filename, HT_DIR_ENABLE_FILE);
		if (HTStat(enable_file_name, &stat_info) == -1) {
		    FREE(filename);
		    FREE(nodename);
		    FREE(enable_file_name);
		    return HTLoadError(sink, 403, DISALLOWED_SELECTIVE_ACCESS);
		}
	    }

	    FREE(filename);
	    FREE(nodename);
	    return HTVMSBrowseDir(addr, anchor, format_out, sink);
	}
    }

    /*
    **	Assume that the file is in Unix-style syntax if it contains a '/'
    **	after the leading one. @@
    */
    {
	FILE * fp;
	char * vmsname = strchr(filename + 1, '/') ?
		    HTVMS_name(nodename, filename) : filename + 1;
	fp = fopen(vmsname, "r", "shr=put", "shr=upd");

	/*
	**  If the file wasn't VMS syntax, then perhaps it is Ultrix.
	*/
	if (!fp) {
	    char * ultrixname = 0;
	    CTRACE((tfp, "HTLoadFile: Can't open as %s\n", vmsname));
	    HTSprintf0(&ultrixname, "%s::\"%s\"", nodename, filename);
	    fp = fopen(ultrixname, "r", "shr=put", "shr=upd");
	    if (!fp) {
		CTRACE((tfp, "HTLoadFile: Can't open as %s\n",
			    ultrixname));
	    }
	    FREE(ultrixname);
	}
	if (fp) {
	    char *semicolon = NULL;

	    if (HTEditable(vmsname)) {
		HTAtom * put = HTAtom_for("PUT");
		HTList * methods = HTAnchor_methods(anchor);
		if (HTList_indexOf(methods, put) == (-1)) {
		    HTList_addObject(methods, put);
		}
	    }
	    /*
	    **	Trim vmsname at semicolon if a version number was
	    **	included, so it doesn't interfere with the check
	    **	for a compressed file. - FM
	    */
	    if ((semicolon = strchr(vmsname, ';')) != NULL)
		*semicolon = '\0';
	    /*
	    **	Fake a Content-Encoding for compressed files. - FM
	    */
	    if (!IsUnityEnc(myEncoding)) {
		/*
		 *  We already know from the call to HTFileFormat above
		 *  that this is a compressed file, no need to look at
		 *  the filename again. - kw
		 */
#ifdef USE_ZLIB
		if (strcmp(format_out->name, "www/download") != 0 &&
		    (!strcmp(HTAtom_name(myEncoding), "gzip") ||
		     !strcmp(HTAtom_name(myEncoding), "x-gzip"))) {
		    fclose(fp);
		    if (semicolon != NULL)
			*semicolon = ';';
		    gzfp = gzopen(vmsname, BIN_R);

		    CTRACE((tfp, "HTLoadFile: gzopen of `%s' gives %p\n",
				vmsname, (void*)gzfp));
		    internal_decompress = cftGzip;
		} else
#endif	/* USE_ZLIB */
#ifdef USE_BZLIB
		if (strcmp(format_out->name, "www/download") != 0 &&
		    (!strcmp(HTAtom_name(myEncoding), "bzip2") ||
		     !strcmp(HTAtom_name(myEncoding), "x-bzip2"))) {
		    fclose(fp);
		    if (semicolon != NULL)
			*semicolon = ';';
		    bzfp = BZ2_bzopen(vmsname, BIN_R);

		    CTRACE((tfp, "HTLoadFile: bzopen of `%s' gives %p\n",
				vmsname, (void*)bzfp));
		    use_zread = YES;
		} else
#endif	/* USE_BZLIB */
		{
		    StrAllocCopy(anchor->content_type, format->name);
		    StrAllocCopy(anchor->content_encoding, HTAtom_name(myEncoding));
		    format = HTAtom_for("www/compressed");
		}
	    } else {
		/* FIXME: should we check if suffix is after ']' or ':' ? */
		CompressFileType cft = HTCompressFileType(vmsname, "._-", &dot);

		if (cft != cftNone) {
		    char *cp = NULL;

		    StrAllocCopy(cp, vmsname);
		    cp[dot - vmsname] = '\0';
		    format = HTFileFormat(cp, &encoding, NULL);
		    FREE(cp);
		    format = HTCharsetFormat(format, anchor,
					     UCLYhndl_HTFile_for_unspec);
		    StrAllocCopy(anchor->content_type, format->name);
		}

		switch (cft) {
		case cftCompress:
		    StrAllocCopy(anchor->content_encoding, "x-compress");
		    format = HTAtom_for("www/compressed");
		    break;
		case cftGzip:
		    StrAllocCopy(anchor->content_encoding, "x-gzip");
#ifdef USE_ZLIB
		    if (strcmp(format_out->name, "www/download") != 0) {
			fclose(fp);
			if (semicolon != NULL)
			    *semicolon = ';';
			gzfp = gzopen(vmsname, BIN_R);

			CTRACE((tfp, "HTLoadFile: gzopen of `%s' gives %p\n",
				    vmsname, (void*)gzfp));
			internal_decompress = cftGzip;
		    }
#else  /* USE_ZLIB */
		    format = HTAtom_for("www/compressed");
#endif	/* USE_ZLIB */
		    break;
		case cftBzip2:
		    StrAllocCopy(anchor->content_encoding, "x-bzip2");
#ifdef USE_BZLIB
		    if (strcmp(format_out->name, "www/download") != 0) {
			fclose(fp);
			if (semicolon != NULL)
			    *semicolon = ';';
			bzfp = BZ2_bzopen(vmsname, BIN_R);

			CTRACE((tfp, "HTLoadFile: bzopen of `%s' gives %p\n",
				    vmsname, (void*)bzfp));
			internal_decompress = cfgBzip2;
		    }
#else  /* USE_BZLIB */
		    format = HTAtom_for("www/compressed");
#endif	/* USE_BZLIB */
		    break;
		case cftNone:
		    break;
		}
	    }
	    if (semicolon != NULL)
		*semicolon = ';';
	    FREE(filename);
	    FREE(nodename);
#if defined(USE_ZLIB) || defined(USE_BZLIB)
	    if (internal_decompress != cftNone) {
		switch (internal_decompress) {
#ifdef USE_ZLIB
		case cftCompress:
		case cftGzip:
		    failed_decompress = (gzfp == 0);
		    break;
#endif
#ifdef USE_BZLIB
		case cftBzip2:
		    failed_decompress = (bzfp == 0);
		    break;
#endif
		default:
		    failed_decompress = YES;
		    break;
		}
		if (failed_decompress) {
		    status = HTLoadError(NULL,
					 -(HT_ERROR),
					 FAILED_OPEN_COMPRESSED_FILE);
		} else {
		    char * sugfname = NULL;
		    if (anchor->SugFname) {
			StrAllocCopy(sugfname, anchor->SugFname);
		    } else {
			char * anchor_path = HTParse(anchor->address, "",
						     PARSE_PATH + PARSE_PUNCTUATION);
			char * lastslash;
			HTUnEscape(anchor_path);
			lastslash = strrchr(anchor_path, '/');
			if (lastslash)
			    StrAllocCopy(sugfname, lastslash + 1);
			FREE(anchor_path);
		    }
		    FREE(anchor->content_encoding);
		    if (sugfname && *sugfname)
			HTCheckFnameForCompression(&sugfname, anchor,
						   TRUE);
		    if (sugfname && *sugfname)
			StrAllocCopy(anchor->SugFname, sugfname);
		    FREE(sugfname);
#ifdef USE_BZLIB
		    if (bzfp)
			status = HTParseBzFile(format, format_out,
					       anchor,
					       bzfp, sink);
#endif
#ifdef USE_ZLIB
		    if (gzfp)
			status = HTParseGzFile(format, format_out,
					       anchor,
					       gzfp, sink);
#endif
		}
	    } else
#endif /* USE_ZLIB || USE_BZLIB */
	    {
		status = HTParseFile(format, format_out, anchor, fp, sink);
		fclose(fp);
	    }
	    return status;
	}  /* If successful open */
	FREE(filename);
    }

#else /* not VMS: */

    FREE(filename);

    /*
    **	For unix, we try to translate the name into the name of a
    **	transparently mounted file.
    **
    **	Not allowed in secure (HTClientHost) situations. TBL 921019
    */
#ifndef NO_UNIX_IO
    /*	Need protection here for telnet server but not httpd server. */

    if (!HTSecure) {		/* try local file system */
	char * localname = HTLocalName(addr);
	struct stat dir_info;

#ifdef HAVE_READDIR
	/*
	**  Multiformat handling.
	**
	**  If needed, scan directory to find a good file.
	**  Bug:  We don't stat the file to find the length.
	*/
	if ((strlen(localname) > strlen(MULTI_SUFFIX)) &&
	    (0 == strcmp(localname + strlen(localname) - strlen(MULTI_SUFFIX),
			 MULTI_SUFFIX))) {
	    DIR *dp = 0;
	    BOOL forget_multi = NO;

	    STRUCT_DIRENT * dirbuf;
	    float best = (float) NO_VALUE_FOUND; /* So far best is bad */
	    HTFormat best_rep = NULL;	/* Set when rep found */
	    HTAtom * best_enc = NULL;
	    char * best_name = NULL;	/* Best dir entry so far */

	    char *base = strrchr(localname, '/');
	    int baselen = 0;

	    if (!base || base == localname) {
		forget_multi = YES;
	    } else {
		*base++ = '\0';		/* Just got directory name */
		baselen = strlen(base)- strlen(MULTI_SUFFIX);
		base[baselen] = '\0';	/* Chop off suffix */

		dp = opendir(localname);
	    }
	    if (forget_multi || !dp) {
		FREE(localname);
		FREE(nodename);
		return HTLoadError(sink, 500, FAILED_DIR_SCAN);
	    }

	    while ((dirbuf = readdir(dp)) != NULL) {
		/*
		**  While there are directory entries to be read...
		*/
#if !(defined(DOSPATH) || defined(__EMX__))
		if (dirbuf->d_ino == 0)
		    continue;	/* if the entry is not being used, skip it */
#endif
		if ((int)strlen(dirbuf->d_name) > baselen &&	 /* Match? */
		    !strncmp(dirbuf->d_name, base, baselen)) {
		    HTAtom * enc;
		    HTFormat rep = HTFileFormat(dirbuf->d_name, &enc, NULL);
		    float filevalue = HTFileValue(dirbuf->d_name);
		    float value = HTStackValue(rep, format_out,
						filevalue,
						0L  /* @@@@@@ */);
		    if (value <= 0.0) {
			char *atomname = NULL;
			CompressFileType cft = HTCompressFileType(dirbuf->d_name, ".", &dot);
			char * cp = NULL;

			enc = NULL;
			if (cft != cftNone) {
			    StrAllocCopy(cp, dirbuf->d_name);
			    cp[dot - dirbuf->d_name] = '\0';
			    format = HTFileFormat(cp, NULL, NULL);
			    FREE(cp);
			    value = HTStackValue(format, format_out,
						 filevalue, 0);
			    switch (cft) {
			    case cftCompress:
				atomname = "application/x-compressed";
				break;
			    case cftGzip:
				atomname = "application/x-gzip";
				break;
			    case cftBzip2:
				atomname = "application/x-bzip2";
				break;
			    case cftNone:
				break;
			    }
			}

			if (atomname != NULL) {
			    value = HTStackValue(format, format_out,
						 filevalue, 0);
			    if (value <= 0.0) {
				format = HTAtom_for(atomname);
				value = HTStackValue(format, format_out,
						     filevalue, 0);
			    }
			    if (value <= 0.0) {
				format = HTAtom_for("www/compressed");
				value = HTStackValue(format, format_out,
						     filevalue, 0);
			    }
			}
		    }
		    if (value != NO_VALUE_FOUND) {
			CTRACE((tfp, "HTLoadFile: value of presenting %s is %f\n",
				    HTAtom_name(rep), value));
			if  (value > best) {
			    best_rep = rep;
			    best_enc = enc;
			    best = value;
			    StrAllocCopy(best_name, dirbuf->d_name);
			}
		    }	/* if best so far */
		 } /* if match */

	    } /* end while directory entries left to read */
	    closedir(dp);

	    if (best_rep) {
		format = best_rep;
		myEncoding = best_enc;
		base[-1] = '/';		/* Restore directory name */
		base[0] = '\0';
		StrAllocCat(localname, best_name);
		FREE(best_name);
	    } else {			/* If not found suitable file */
		FREE(localname);
		FREE(nodename);
		return HTLoadError(sink, 403, FAILED_NO_REPRESENTATION);
	    }
	    /*NOTREACHED*/
	} /* if multi suffix */

	/*
	**  Check to see if the 'localname' is in fact a directory.  If it
	**  is create a new hypertext object containing a list of files and
	**  subdirectories contained in the directory.	All of these are
	**  links to the directories or files listed.
	**  NB This assumes the existence of a type 'STRUCT_DIRENT', which
	**  will hold the directory entry, and a type 'DIR' which is used
	**  to point to the current directory being read.
	*/
#if defined(USE_DOS_DRIVES)
	if (strlen(localname) == 2 && LYIsDosDrive(localname))
	    LYAddPathSep(&localname);
#endif
	if (HTStat(localname,&dir_info) == -1)	   /* get file information */
	{
				/* if can't read file information */
	    CTRACE((tfp, "HTLoadFile: can't stat %s\n", localname));

	}  else {		/* Stat was OK */

	    if (S_ISDIR(dir_info.st_mode)) {
		/*
		**  If localname is a directory.
		*/
		DIR *dp;
		struct stat file_info;

		CTRACE((tfp, "%s is a directory\n", localname));

		/*
		**  Check directory access.
		**  Selective access means only those directories containing
		**  a marker file can be browsed.
		*/
		if (HTDirAccess == HT_DIR_FORBID) {
		    FREE(localname);
		    FREE(nodename);
		    return HTLoadError(sink, 403, DISALLOWED_DIR_SCAN);
		}

		if (HTDirAccess == HT_DIR_SELECTIVE) {
		    char * enable_file_name = NULL;

		    HTSprintf0(&enable_file_name, "%s/%s", localname, HT_DIR_ENABLE_FILE);
		    if (stat(enable_file_name, &file_info) != 0) {
			FREE(localname);
			FREE(nodename);
			FREE(enable_file_name);
			return HTLoadError(sink, 403, DISALLOWED_SELECTIVE_ACCESS);
		    }
		}

		CTRACE((tfp, "Opening directory %s\n", localname));
		dp = opendir(localname);
		if (!dp) {
		    FREE(localname);
		    FREE(nodename);
		    return HTLoadError(sink, 403, FAILED_DIR_UNREADABLE);
		}

		/*
		**  Directory access is allowed and possible.
		*/

		status = print_local_dir(dp, localname,
					anchor, format_out, sink);
		closedir(dp);
		FREE(localname);
		FREE(nodename);
		return status;	/* document loaded, maybe partial */

	    } /* end if localname is a directory */

	    if (S_ISREG(dir_info.st_mode)) {
#ifdef INT_MAX
		if (dir_info.st_size <= INT_MAX)
#endif
		    anchor->content_length = dir_info.st_size;
	    }

	} /* end if file stat worked */

/* End of directory reading section
*/
#endif /* HAVE_READDIR */
	{
	    int bin = HTCompressFileType(localname, ".", &dot) != cftNone;
	    FILE * fp = fopen(localname, (bin ? BIN_R : "r"));

	    CTRACE((tfp, "HTLoadFile: Opening `%s' gives %p\n",
				 localname, (void*)fp));
	    if (fp) {		/* Good! */
		if (HTEditable(localname)) {
		    HTAtom * put = HTAtom_for("PUT");
		    HTList * methods = HTAnchor_methods(anchor);
		    if (HTList_indexOf(methods, put) == (-1)) {
			HTList_addObject(methods, put);
		    }
		}
		/*
		**  Fake a Content-Encoding for compressed files. - FM
		*/
		if (!IsUnityEnc(myEncoding)) {
		    /*
		     *	We already know from the call to HTFileFormat above
		     *	that this is a compressed file, no need to look at
		     *	the filename again. - kw
		     */
#ifdef USE_ZLIB
		    if (strcmp(format_out->name, "www/download") != 0 &&
			(!strcmp(HTAtom_name(myEncoding), "gzip") ||
			 !strcmp(HTAtom_name(myEncoding), "x-gzip"))) {
			fclose(fp);
			gzfp = gzopen(localname, BIN_R);

			CTRACE((tfp, "HTLoadFile: gzopen of `%s' gives %p\n",
				    localname, (void*)gzfp));
			internal_decompress = cftGzip;
		    } else
#endif	/* USE_ZLIB */
#ifdef USE_BZLIB
		    if (strcmp(format_out->name, "www/download") != 0 &&
			(!strcmp(HTAtom_name(myEncoding), "bzip2") ||
			 !strcmp(HTAtom_name(myEncoding), "x-bzip2"))) {
			fclose(fp);
			bzfp = BZ2_bzopen(localname, BIN_R);

			CTRACE((tfp, "HTLoadFile: bzopen of `%s' gives %p\n",
				    localname, (void*)bzfp));
			internal_decompress = cftBzip2;
		    } else
#endif	/* USE_BZLIB */
		    {
			StrAllocCopy(anchor->content_type, format->name);
			StrAllocCopy(anchor->content_encoding, HTAtom_name(myEncoding));
			format = HTAtom_for("www/compressed");
		    }
		} else {
		    CompressFileType cft = HTCompressFileType(localname, ".", &dot);

		    if (cft != cftNone) {
			char *cp = NULL;

			StrAllocCopy(cp, localname);
			cp[dot - localname] = '\0';
			format = HTFileFormat(cp, &encoding, NULL);
			FREE(cp);
			format = HTCharsetFormat(format, anchor,
						 UCLYhndl_HTFile_for_unspec);
			StrAllocCopy(anchor->content_type, format->name);
		    }

		    switch (cft) {
		    case cftCompress:
			StrAllocCopy(anchor->content_encoding, "x-compress");
			format = HTAtom_for("www/compressed");
			break;
		    case cftGzip:
			StrAllocCopy(anchor->content_encoding, "x-gzip");
#ifdef USE_ZLIB
			if (strcmp(format_out->name, "www/download") != 0) {
			    fclose(fp);
			    gzfp = gzopen(localname, BIN_R);

			    CTRACE((tfp, "HTLoadFile: gzopen of `%s' gives %p\n",
					localname, (void*)gzfp));
			    internal_decompress = cftGzip;
			}
#else  /* USE_ZLIB */
			format = HTAtom_for("www/compressed");
#endif	/* USE_ZLIB */
			break;
		    case cftBzip2:
			StrAllocCopy(anchor->content_encoding, "x-bzip2");
#ifdef USE_BZLIB
			if (strcmp(format_out->name, "www/download") != 0) {
			    fclose(fp);
			    bzfp = BZ2_bzopen(localname, BIN_R);

			    CTRACE((tfp, "HTLoadFile: bzopen of `%s' gives %p\n",
					localname, (void*)bzfp));
			    internal_decompress = cftBzip2;
			}
#else  /* USE_BZLIB */
			format = HTAtom_for("www/compressed");
#endif	/* USE_BZLIB */
			break;
		    case cftNone:
			break;
		    }
		}
		FREE(localname);
		FREE(nodename);
#if defined(USE_ZLIB) || defined(USE_BZLIB)
		if (internal_decompress != cftNone) {
		    switch (internal_decompress) {
#ifdef USE_ZLIB
		    case cftGzip:
			failed_decompress = (gzfp == 0);
			break;
#endif
#ifdef USE_BZLIB
		    case cftBzip2:
			failed_decompress = (bzfp == 0);
			break;
#endif
		    default:
			failed_decompress = YES;
			break;
		    }
		    if (failed_decompress) {
			status = HTLoadError(NULL,
					     -(HT_ERROR),
					     FAILED_OPEN_COMPRESSED_FILE);
		    } else {
			char * sugfname = NULL;
			if (anchor->SugFname) {
			    StrAllocCopy(sugfname, anchor->SugFname);
			} else {
			    char * anchor_path = HTParse(anchor->address, "",
							 PARSE_PATH + PARSE_PUNCTUATION);
			    char * lastslash;
			    HTUnEscape(anchor_path);
			    lastslash = strrchr(anchor_path, '/');
			    if (lastslash)
				StrAllocCopy(sugfname, lastslash + 1);
			    FREE(anchor_path);
			}
			FREE(anchor->content_encoding);
			if (sugfname && *sugfname)
			    HTCheckFnameForCompression(&sugfname, anchor,
						       TRUE);
			if (sugfname && *sugfname)
			    StrAllocCopy(anchor->SugFname, sugfname);
			FREE(sugfname);
#ifdef USE_BZLIB
			if (bzfp)
			    status = HTParseBzFile(format, format_out,
						   anchor,
						   bzfp, sink);
#endif
#ifdef USE_ZLIB
			if (gzfp)
			    status = HTParseGzFile(format, format_out,
						   anchor,
						   gzfp, sink);
#endif
		    }
		} else
#endif /* USE_ZLIB */
		{
		    status = HTParseFile(format, format_out, anchor, fp, sink);
		    fclose(fp);
		}
		return status;
	    }  /* If successful open */
	    FREE(localname);
	}  /* scope of fp */
    }  /* local unix file system */
#endif /* !NO_UNIX_IO */
#endif /* VMS */

#ifndef DECNET
    /*
    **	Now, as transparently mounted access has failed, we try FTP.
    */
    {
	/*
	**  Deal with case-sensitivity differences on VMS versus Unix.
	*/
#ifdef VMS
	if (strcasecomp(nodename, HTHostName()) != 0)
#else
	if (strcmp(nodename, HTHostName()) != 0)
#endif /* VMS */
	{
	    status = -1;
	    FREE(nodename);
	    if (strncmp(addr, "file://localhost", 16)) {
		/* never go to ftp site when URL
		 * is file://localhost
		 */
#ifndef DISABLE_FTP
		status = HTFTPLoad(addr, anchor, format_out, sink);
#endif /* DISABLE_FTP */
	    }
	    return status;
	}
	FREE(nodename);
    }
#endif /* !DECNET */

    /*
    **	All attempts have failed.
    */
    {
	CTRACE((tfp, "Can't open `%s', errno=%d\n", addr, SOCKET_ERRNO));

	return HTLoadError(sink, 403, FAILED_FILE_UNREADABLE);
    }
}

static CONST char *program_paths[pp_Last];

/*
 * Given a program number, return its path
 */
PUBLIC CONST char * HTGetProgramPath ARGS1(
	ProgramPaths,	code)
{
    CONST char *result = NULL;
    if (code > ppUnknown && code < pp_Last)
	result = program_paths[code];
    return result;
}

/*
 * Store a program's path.  The caller must allocate the string used for 'path',
 * since HTInitProgramPaths() may free it.
 */
PUBLIC void HTSetProgramPath ARGS2(
	ProgramPaths,	code,
	CONST char *,	path)
{
    if (code > ppUnknown && code < pp_Last) {
	program_paths[code] = isEmpty(path) ? 0 : path;
    }
}

/*
 * Reset the list of known program paths to the ones that are compiled-in
 */
PUBLIC void HTInitProgramPaths NOARGS
{
    int code;
    CONST char *path;
    CONST char *test;

    for (code = (int) ppUnknown + 1; code < (int) pp_Last; ++code) {
	switch (code) {
#ifdef BZIP2_PATH
	case ppBZIP2:
	    path = BZIP2_PATH;
	    break;
#endif
#ifdef CHMOD_PATH
	case ppCHMOD:
	    path = CHMOD_PATH;
	    break;
#endif
#ifdef COMPRESS_PATH
	case ppCOMPRESS:
	    path = COMPRESS_PATH;
	    break;
#endif
#ifdef COPY_PATH
	case ppCOPY:
	    path = COPY_PATH;
	    break;
#endif
#ifdef CSWING_PATH
	case ppCSWING:
	    path = CSWING_PATH;
	    break;
#endif
#ifdef GZIP_PATH
	case ppGZIP:
	    path = GZIP_PATH;
	    break;
#endif
#ifdef INSTALL_PATH
	case ppINSTALL:
	    path = INSTALL_PATH;
	    break;
#endif
#ifdef MKDIR_PATH
	case ppMKDIR:
	    path = MKDIR_PATH;
	    break;
#endif
#ifdef MV_PATH
	case ppMV:
	    path = MV_PATH;
	    break;
#endif
#ifdef RLOGIN_PATH
	case ppRLOGIN:
	    path = RLOGIN_PATH;
	    break;
#endif
#ifdef RM_PATH
	case ppRM:
	    path = RM_PATH;
	    break;
#endif
#ifdef RMDIR_PATH
	case ppRMDIR:
	    path = RMDIR_PATH;
	    break;
#endif
#ifdef TAR_PATH
	case ppTAR:
	    path = TAR_PATH;
	    break;
#endif
#ifdef TELNET_PATH
	case ppTELNET:
	    path = TELNET_PATH;
	    break;
#endif
#ifdef TN3270_PATH
	case ppTN3270:
	    path = TN3270_PATH;
	    break;
#endif
#ifdef TOUCH_PATH
	case ppTOUCH:
	    path = TOUCH_PATH;
	    break;
#endif
#ifdef UNCOMPRESS_PATH
	case ppUNCOMPRESS:
	    path = UNCOMPRESS_PATH;
	    break;
#endif
#ifdef UNZIP_PATH
	case ppUNZIP:
	    path = UNZIP_PATH;
	    break;
#endif
#ifdef UUDECODE_PATH
	case ppUUDECODE:
	    path = UUDECODE_PATH;
	    break;
#endif
#ifdef ZCAT_PATH
	case ppZCAT:
	    path = ZCAT_PATH;
	    break;
#endif
#ifdef ZIP_PATH
	case ppZIP:
	    path = ZIP_PATH;
	    break;
#endif
	default:
	    path = NULL;
	    break;
	}
	test = HTGetProgramPath(code);
	if (test != NULL && test != path) {
	    free((char *)test);
	}
	HTSetProgramPath(code, path);
    }
}

/*
**	Protocol descriptors
*/
#ifdef GLOBALDEF_IS_MACRO
#define _HTFILE_C_1_INIT { "ftp", HTLoadFile, 0 }
GLOBALDEF (HTProtocol,HTFTP,_HTFILE_C_1_INIT);
#define _HTFILE_C_2_INIT { "file", HTLoadFile, HTFileSaveStream }
GLOBALDEF (HTProtocol,HTFile,_HTFILE_C_2_INIT);
#else
GLOBALDEF PUBLIC HTProtocol HTFTP  = { "ftp", HTLoadFile, 0 };
GLOBALDEF PUBLIC HTProtocol HTFile = { "file", HTLoadFile, HTFileSaveStream };
#endif /* GLOBALDEF_IS_MACRO */
