/*
 * Lynx - Hypertext navigation system
 *
 *   (c) Copyright 1992, 1993, 1994 University of Kansas
 *	 1995, 1996: GNU General Public License
 */

/*******************************************************************
 * There are four sections to this document:
 *  Section 1.  Things you MUST verify.  Unix platforms use a configure
 *		script to provide sensible default values.  If your site
 *		has special requirements, that may not be sufficient.
 *		For non-Unix platforms (e.g., VMS), there is no
 *		configure script, so the defaults here are more
 *		critical.
 *	Section 1a)  VMS specific things
 *	Section 1b)  non-VMS specific things
 *	Section 1c)  ALL Platforms
 *
 *  Section 2.  Things you should probably check!
 *
 *  Section 3.  Things you should only change after you have a good
 *              understanding of the program!
 *
 *  Section 4.  Things you MUST check only if you plan to use Lynx in
 *              an anonymous account (allow public access to Lynx)!
 *
 */

#ifndef USERDEFS_H
#define USERDEFS_H

/*******************************************************************
 * Insure definition of NOT_ASCII, etc. precedes use below.
 */
#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef HAVE_CONFIG_H
#include <lynx_cfg.h>
#endif

/*******************************************************************
 * Things you must change
 *  Section 1.
 */

/*******************************************************************
 * Things you must change  -  VMS specific
 *  Section 1a).
 */
#ifdef VMS
/**************************
 * TEMP_SPACE is where Lynx temporary cache files will be placed.
 * Temporary files are removed automatically as long as nothing
 * goes terribly wrong :)  If you include "$USER" in the definition
 * (e.g., "device:[dir.$USER]"), Lynx will replace the "$USER" with
 * the username of the account which invoked the Lynx image.  Such
 * directories should already exist, and have protections/ACLs set
 * so that only the appropriate user(s) will have read/write access.
 * On VMS, "sys$scratch:" defaults to "sys$login:" if it has not been
 * defined externally, or you can use "sys$login:" explicitly here.
 * If the path has SHELL syntax and includes a tilde (e.g, "~/lynxtmp"),
 * Lynx will replace the tilde with the full path for the user's home
 * and convert the result to VMS syntax.
 * The definition here can be overridden at run time by defining a
 * "LYNX_TEMP_SPACE" VMS logical.
 */
#define TEMP_SPACE "sys$scratch:"

/**************************
 * LYNX_CFG_FILE is the location and name of the default lynx
 * global configuration file.  It is sought and processed at
 * startup of Lynx, followed by a seek and processing of a
 * personal RC file (.lynxrc in the user's HOME directory,
 * created if the user saves values in the 'o'ptions menu).
 * You also can define the location and name of the global
 * configuration file via a VMS logical, "LYNX_CFG", which
 * will override the "LYNX_CFG_FILE" definition here.  SYS$LOGIN:
 * can be used as the device in either or both definitions if
 * you want lynx.cfg treated as a personal configuration file.
 * You also can use Unix syntax with a '~' for a subdirectory
 * of the login directory, (e.g., ~/lynx/lynx.cfg).
 * The -cfg command line switch will override these definitions.
 * You can pass the compilation default via build.com or descrip.mms.
 *
 * Note that some implementations of telnet allow passing of
 * environment variables, which might be used by unscrupulous
 * people to modify the environment in anonymous accounts.  When
 * making Lynx and Web access publicly available via anonymous
 * accounts intended to run Lynx captively, be sure the wrapper
 * uses the -cfg switch and specifies the startfile, rather than
 * relying on the LYNX_CFG, LYNX_CFG_FILE, or WWW_HOME variables.
 *
 * Note that any SUFFIX or VIEWER mappings in the configuration
 * file will be overridden by any suffix or viewer mappings
 * that are established as defaults in src/HTInit.c.  You can
 * override the src/HTInit.c defaults via the mime.types and
 * mailcap files (see the examples in the samples directory).
 */
#ifndef LYNX_CFG_FILE
#define LYNX_CFG_FILE "Lynx_Dir:lynx.cfg"
#endif /* LYNX_CFG_FILE */

/**************************
 * The EXTENSION_MAP file allows you to map file suffixes to
 * mime types.
 * The file locations defined here can be overridden in lynx.cfg.
 * Mappings in these global and personal files override any SUFFIX
 * definitions in lynx.cfg and built-in defaults from src/HTInit.c.
 */
#define GLOBAL_EXTENSION_MAP "Lynx_Dir:mime.types"
#define PERSONAL_EXTENSION_MAP "mime.types"

/**************************
 * The MAILCAP file allows you to map file MIME types to
 * external viewers.
 * The file locations defined here can be overridden in lynx.cfg.
 * Mappings in these global and personal files override any VIEWER
 * definitions in lynx.cfg and built-in defaults from src/HTInit.c.
 */
#define GLOBAL_MAILCAP "Lynx_Dir:mailcap"
#define PERSONAL_MAILCAP ".mailcap"

/**************************
 * XLOADIMAGE_COMMAND will be used as a default in src/HTInit.c
 * for viewing image content types when the DECW$DISPLAY logical
 * is set.  Make it the foreign command for your system's X image
 * viewer (commonly, "xv").  It can be anything that will handle GIF,
 * TIFF and other popular image formats.  Freeware ports of xv for
 * VMS are available in the ftp://ftp.wku.edu/vms/unsupported and
 * http://www.openvms.digital.com/cd/XV310A/ subdirectories.  You
 * must also have a "%s" for the filename.  The default defined
 * here can be overridden in lynx.cfg, or via the global or personal
 * mailcap files.
 * Make this NULL if you don't have such a viewer or don't want to
 * use any default viewers for image types.
 */
#define XLOADIMAGE_COMMAND "xv %s"

/**************************
 * SYSTEM_MAIL must be defined here to your mail sending command,
 * and SYSTEM_MAIL_FLAGS to appropriate qualifiers.  They can be
 * changed in lynx.cfg.
 *
 * The mail command will be spawned as a subprocess of lynx
 * and used to send the email, with headers specified in a
 * temporary file for PMDF.  If you define SYSTEM_MAIL to the
 * "generic" MAIL utility for VMS, headers cannot be specified
 * via a header file (and thus may not be included), and the
 * subject line will be specified by use of the /subject="SUBJECT"
 * qualifier.
 *
 * If your mailer uses another syntax, some hacking of the
 * mailform(), mailmsg() and reply_by_mail() functions in
 * LYMail.c, and printfile() function in LYPrint.c, may be
 * required.
 */
#define SYSTEM_MAIL "PMDF SEND"
#define SYSTEM_MAIL_FLAGS "/headers"
/* #define SYSTEM_MAIL "MAIL"   */
/* #define SYSTEM_MAIL_FLAGS "" */

/*************************
 * Below is the argument for an sprintf command that will add
 * "IN%""ADDRESS""" to the Internet mail address given by the user.
 * It is structured for PMDF's IN%"INTERNET_ADDRESS" scheme.  The %s
 * is replaced with the address given by the user.  If you are using
 * a different Internet mail transport, change the IN appropriately
 * (e.g., to SMTP, MX, or WINS), here or in lynx.cfg.
 */
#define MAIL_ADRS "\"IN%%\"\"%s\"\"\""

/*********************************
 * On VMS, CSwing (an XTree emulation for VTxxx terminals) is intended for
 * use as the Directory/File Manager (sources, objects, or executables are
 * available from ftp://narnia.memst.edu/).  CSWING_PATH should be defined
 * here or in lynx.cfg to your foreign command for CSwing, with any
 * regulatory switches you want included.  If not defined, or defined as
 * a zero-length string ("") or "none" (case-insensitive), the support
 * will be disabled.  It will also be disabled if the -nobrowse or
 * -selective switches are used, or if the file_url restriction is set.
 *
 * When enabled, the DIRED_MENU command (normally 'f' or 'F') will invoke
 * CSwing, normally with the current default directory as an argument to
 * position the user on that node of the directory tree.  However, if the
 * current document is a local directory listing, or a local file and not
 * one of the temporary menu or list files, the associated directory will
 * be passed as an argument, to position the user on that node of the tree.
 */
/* #define CSWING_PATH "swing" */

/*********************************
 * If USE_FIXED_RECORDS is set to TRUE here and/or in lynx.cfg, Lynx will
 * convert 'd'ownloaded binary files to FIXED 512 record format before saving
 * them to disk or acting on a DOWNLOADER option.  If set to FALSE, the
 * headers of such files will indicate that they are Stream_LF with Implied
 * Carriage Control, which is incorrect, and can cause downloading software
 * to get confused and unhappy.  If you do set it FALSE, you can use the
 * FIXED512.COM command file, which is included in this distribution, to do
 * the conversion externally.
 */
#define USE_FIXED_RECORDS	TRUE	/* convert binaries to FIXED 512 */

/********************************
 * If NO_ANONYMOUS_EMAIL is defined, Lynx will not offer to insert X-From
 * and X_Personal_Name lines in the body of email messages.  On VMS, the
 * actual From and Personal Name (if defined for the account) headers always
 * are those of the account running the Lynx image.  If the account is not
 * the one to which the recipient should reply, you can indicate the alternate
 * address and personal name via the X-From and X_Personal_Name entries, but
 * the recipient must explicitly send the reply to the X_From address, rather
 * than using the VMS REPLY command (which will use the actual From address).
 *
 * This symbol constant might be defined on Unix for security reasons that
 * don't apply on VMS.  There is no security reason for defining this on VMS,
 * but if you have no anonymous accounts (i.e., the From always will point to
 * the actual user's email address, you can define it to avoid the bother of
 * X-From and X_Personal_Name offers.
 */
/*#define NO_ANONYMOUS_EMAIL TRUE */

/**************************
 * LYNX_LSS_FILE is the location and name of the default lynx
 * character style sheet file.  It is sought and processed at
 * startup of Lynx only if experimental character style code has
 * been compiled in, otherwise it will be ignored.  Note that use
 * of the character style option is _experimental_ AND _unsupported_.
 * There is no documentation other than a sample lynx.lss file in
 * the samples subdirectory.  This code probably won't even work on
 * VMS.  You can define the location and name of this file via an
 * environment variable, "lynx_lss", which will override the definition
 * here.  You can use '~' to refer to the user's home directory.  The
 * -lss command line switch will override these definitions.
 */
#ifndef LYNX_LSS_FILE
#define LYNX_LSS_FILE "Lynx_Dir:lynx.lss"
#endif /* LYNX_LSS_FILE */

/*******************************************************************
 * Things you must change  -  non-VMS specific
 *  Section 1b).
 */
#else     /* non-VMS: UNIX etc. */

/**************************
 * NOTE: This variable is set by the configure script; editing changes will
 * be ignored.
 *
 * LYNX_CFG_FILE is the location and name of the default lynx
 * global configuration file.  It is sought and processed at
 * startup of Lynx, followed by a seek and processing of a
 * personal RC file (.lynxrc in the user's HOME directory,
 * created if the user saves values in the 'o'ptions menu).
 * You also can define the location and name of the global
 * configuration file via an environment variable, "LYNX_CFG",
 * which will override the "LYNX_CFG_FILE" definition here.
 * You can use '~' in either or both definitions if you want
 * lynx.cfg treated as a personal configuration file.  The
 * -cfg command line switch will override these definitions.
 * You can pass the compilation default via the Makefile.
 *
 * If you are building Lynx using the configure script, you should specify
 * the default location of the configuration file via that script, since it
 * also generates the makefile and install-cfg rules.
 *
 * Note that many implementations of telnetd allow passing of
 * environment variables, which might be used by unscrupulous
 * people to modify the environment in anonymous accounts.  When
 * making Lynx and Web access publicly available via anonymous
 * accounts intended to run Lynx captively, be sure the wrapper
 * uses the -cfg switch and specifies the startfile, rather than
 * relying on the LYNX_CFG, LYNX_CFG_FILE, or WWW_HOME variables.
 *
 * Note that any SUFFIX or VIEWER mappings in the configuration
 * file will be overridden by any suffix or viewer mappings
 * that are established as defaults in src/HTInit.c.  You can
 * override the src/HTInit.c defaults via the mime.types and
 * mailcap files (see the examples in the samples directory).
 */
#ifndef HAVE_CONFIG_H
#ifndef LYNX_CFG_FILE
#ifdef DOSPATH
#define LYNX_CFG_FILE "./lynx.cfg"
#else
#define LYNX_CFG_FILE "/usr/local/lib/lynx.cfg"
#endif /* DOSPATH */
#endif /* LYNX_CFG_FILE */
#endif /* HAVE_CONFIG_H */

/**************************
 * The EXTENSION_MAP file allows you to map file suffixes to
 * mime types.
 * The file locations defined here can be overridden in lynx.cfg.
 * Mappings in these global and personal files override any SUFFIX
 * definitions in lynx.cfg and built-in defaults from src/HTInit.c.
 */
#define GLOBAL_EXTENSION_MAP "/usr/local/lib/mosaic/mime.types"
#define PERSONAL_EXTENSION_MAP ".mime.types"

/**************************
 * The MAILCAP file allows you to map file MIME types to
 * external viewers.
 * The file locations defined here can be overridden in lynx.cfg.
 * Mappings in these global and personal files override any VIEWER
 * definitions in lynx.cfg and built-in defaults from src/HTInit.c.
 */
#define GLOBAL_MAILCAP "/usr/local/lib/mosaic/mailcap"
#define PERSONAL_MAILCAP ".mailcap"

/**************************
 * XLOADIMAGE_COMMAND will be used as a default in src/HTInit.c for
 * viewing image content types when the DISPLAY environment variable
 * is set.  Make it the full path and name of the xli (also known as
 * xloadimage or xview) command, or other image viewer.  It can be
 * anything that will handle GIF, TIFF and other popular image formats
 * (xli does).  The freeware distribution of xli is available in the
 * ftp://ftp.x.org/contrib/ subdirectory.  The shareware, xv, also is
 * suitable.  You must also have a "%s" for the filename; "&" for
 * background is optional.  The default defined here can be overridden
 * in lynx.cfg, or via the global or personal mailcap files.
 * Make this NULL if you don't have such a viewer or don't want to
 * use any default viewers for image types.  Note that open is used as
 * the default for NeXT, instead of the XLOADIMAGE_COMMAND definition.
 */
#define XLOADIMAGE_COMMAND "xli %s &"

/**************************
 * For UNIX systems, SYSTEM_MAIL and SYSTEM_MAIL_FLAGS are set by the
 * configure-script.
 */

/**************************
 * A place to put temporary files, it is almost always in "/tmp/"
 * for UNIX systems.  If you include "$USER" in the definition
 * (e.g., "/tmp/$USER"), Lynx will replace the "$USER" with the
 * username of the account which invoked the Lynx image.  Such
 * directories should already exist, and have protections/ACLs set
 * so that only the appropriate user(s) will have read/write access.
 * If the path includes a tilde (e.g, "~" or "~/lynxtmp"), Lynx will
 * replace the tilde with the full path for the user's home.
 * The definition here can be overridden at run time by setting a
 * "LYNX_TEMP_SPACE" environment variable, or (if that is not set)
 * the "TMPDIR" (unix), or "TEMP" or "TMP" (Windows,DOS,OS/2)
 * variable.
 */
#define TEMP_SPACE "/tmp/"

/********************************
 * Comment this line out to disable code that implements command logging
 * and scripting.
 */
#define EXP_CMD_LOGGING 1

/********************************
 * Comment this line out to disable code that randomizes the names given to
 * temporary files.
 */
#define EXP_RAND_TEMPNAME 1

/********************************
 * Comment this line out to let the user enter his/her email address
 * when sending a message.  There should be no need to do this unless
 * your mailer agent does not put in the From: field for you.  (If your
 * mailer agent does not automatically put in the From: field, you should
 * upgrade, because anonymous mail makes it far too easy for a user to
 * spoof someone else's email address.)
 */
/*#define NO_ANONYMOUS_EMAIL TRUE */

/********************************
 * LIST_FORMAT defines the display for local files when LONG_LIST
 * is defined in the Makefile.  The default set here can be changed
 * in lynx.cfg.
 *
 * The percent items in the list are interpreted as follows:
 *
 *	%p	Unix-style permission bits
 *	%l	link count
 *	%o	owner of file
 *	%g	group of file
 *	%d	date of last modification
 *	%a	anchor pointing to file or directory
 *	%A	as above but don't show symbolic links
 *	%t	type of file (description derived from MIME type)
 *	%T	MIME type as known by Lynx (from mime.types or default)
 *	%k	size of file in Kilobytes
 *	%K	as above but omit size for directories
 *	%s	size of file in bytes
 *
 * Anything between the percent and the letter is passed on to sprintf.
 * A double percent yields a literal percent on output.  Other characters
 * are passed through literally.
 *
 * If you want only the filename:  "    %a"
 *
 * If you want a brief output:     "    %4K %-12.12d %a"
 *
 * For the Unix "ls -l" format:    "    %p %4l %-8.8o %-8.8g %7s %-12.12d %a"
 */
#ifdef DOSPATH
#define LIST_FORMAT "    %4K %-12.12d %a"
#else
#define LIST_FORMAT "    %p %4l %-8.8o %-8.8g %7s %-12.12d %a"
#endif

/*
 *  If NO_FORCED_CORE_DUMP is set to TRUE, Lynx will not force
 *  core dumps via abort() calls on fatal errors or assert()
 *  calls to check potentially fatal errors.  The default defined
 *  here can be changed in lynx.cfg, and the compilation or
 *  configuration default can be toggled via the -core command
 *  line switch.
 */
#define NO_FORCED_CORE_DUMP	FALSE

/**************************
 * LYNX_LSS_FILE is the location and name of the default lynx
 * character style sheet file.  It is sought and processed at
 * startup of Lynx only if experimental character style code
 * has been compiled in, otherwise it will be ignored.  Note
 * that use of the character style option is _experimental_ AND
 * _unsupported_.  There is no documentation other than a sample
 * lynx.lss file in the samples subdirectory.  You also can
 * define the location and name of this file via environment
 * variables "LYNX_LSS" or "lynx_lss" which will override the
 * "LYNX_LSS_FILE" definition here.  You can use '~' in either or
 * both definitions to refer to the user's home directory.  The
 * -lss command line switch will override these definitions.
 */
#ifndef LYNX_LSS_FILE
#define LYNX_LSS_FILE "/usr/local/lib/lynx.lss"
#endif /* LYNX_LSS_FILE */

#endif /* VMS OR UNIX */

/*************************************************************
 *  Section 1c)   Every platform must change or verify these
 *
 */

/*****************************
 * STARTFILE is the default starting URL if none is specified
 *   on the command line or via a WWW_HOME environment variable;
 *   Lynx will refuse to start without a starting URL of some kind.
 * STARTFILE can be remote, e.g., http://www.w3.org/default.html ,
 *                or local, e.g., file://localhost/PATH_TO/FILENAME ,
 *           where PATH_TO is replaced with the complete path to FILENAME
 *           using Unix shell syntax and including the device on VMS.
 *
 * Normally we expect you will connect to a remote site, e.g., the Lynx starting
 * site:
 */
#define STARTFILE "http://lynx.isc.org/"
/*
 * As an alternative, you may want to use a local URL.  A good choice for this
 * is the user's home directory:
 *#define STARTFILE "file://localhost/~/"
 *
 * Your choice of STARTFILE should reflect your site's needs, and be a URL that
 * you can connect to reliably.  Otherwise users will become confused and think
 * that they cannot run Lynx.
 */

/*****************************
 * HELPFILE must be defined as a URL and must have a
 * complete path if local:
 * file://localhost/PATH_TO/lynx_help/lynx_help_main.html
 *   Replace PATH_TO with the path to the lynx_help subdirectory
 *   for this distribution (use SHELL syntax including the device
 *   on VMS systems).
 * The default HELPFILE is:
 * http://www.subir.com/lynx/lynx_help/lynx_help_main.html
 *   This should be changed here or in lynx.cfg to the local path.
 */
#define HELPFILE "http://www.subir.com/lynx/lynx_help/lynx_help_main.html"
/* #define HELPFILE "file://localhost/PATH_TO/lynx_help/lynx_help_main.html" */

/*****************************
 * DEFAULT_INDEX_FILE is the default file retrieved when the
 * user presses the 'I' key when viewing any document.
 * An index to your CWIS can be placed here or a document containing
 * pointers to lots of interesting places on the web.
 */
#define DEFAULT_INDEX_FILE "http://www.ncsa.uiuc.edu/SDG/Software/Mosaic/MetaIndex.html"

/*****************************
 * If USE_TRACE_LOG is set FALSE, then when TRACE mode is invoked the
 * syserr messages will not be directed to a log file named Lynx.trace
 * in the account's HOME directory.  The default defined here can be
 * toggled via the -tlog command line switch.  Also, it is set FALSE
 * automatically when Lynx is executed in an anonymous or validation
 * account (if indicated via the -anonymous or -validate command line
 * switches, or via the check for the ANONYMOUS_USER, defined below).
 * When FALSE, the TRACE_LOG command (normally ';') cannot be used to
 * examine the Lynx Trace Log during the current session.  If left
 * TRUE, but you wish to use command line piping of stderr to a file
 * you specify, include the -tlog toggle on the command line.  Note
 * that once TRACE mode is turned on during a session and stderr is
 * directed to the log, all stderr messages will continue going to
 * the log, even if TRACE mode is turned off via the TOGGLE_TRACE
 * (Control-T) command.
 */
#define USE_TRACE_LOG	TRUE

/*******************************
 * If GOTOBUFFER is set to TRUE here or in lynx.cfg the last entered
 * goto URL, if any, will be offered as a default for reuse or editing
 * when the 'g'oto command is entered.  All previously used goto URLs
 * can be accessed for reuse or editing via a circular buffer invoked
 * with the Up-Arrow or Down-Arrow keys after entering the 'g'oto
 * command, whether or not a default is offered.
 */
#define GOTOBUFFER	  FALSE

/*****************************
 * If FTP_PASSIVE is set to TRUE here or in lynx.cfg, ftp transfers will
 * be done in passive mode.
 * Note: if passive transfers fail, lynx falls back to active mode, and
 * vice versa if active transfers fail at first.
 */
#define FTP_PASSIVE	  TRUE

/*****************************
 * JUMPFILE is the default local file checked for shortcut URLs when
 * the user presses the 'J' (JUMP) key.  The user will be prompted for
 * a shortcut entry (analogously to 'g'oto), and can enter one
 * or use '?' for a list of the shortcuts with associated links to
 * their actual URLs.  See the sample jumps files in the samples
 * subdirectory.  Make sure your jumps file includes a '?' shortcut
 * for a file://localhost URL to itself:
 *
 * <dt>?<dd><a href="file://localhost/path/jumps.html">This Shortcut List</a>
 *
 * If not defined here or in lynx.cfg, the JUMP command will invoke
 * the NO_JUMPFILE status line message (see LYMessages_en.h).  The prompt
 * associated with the default jumps file is defined as JUMP_PROMPT in
 * LYMessages_en.h and can be modified in lynx.cfg.  Additional, alternate
 * jumps files can be defined and mapped to keystrokes, and alternate
 * prompts can be set for them, in lynx.cfg, but at least one default
 * jumps file and associated prompt should be established before adding
 * others.
 *
 * On VMS, use Unix SHELL syntax (including a lead slash) to define it.
 *
 * Do not include "file://localhost" in the definition.
 */
/* #define JUMPFILE "/Lynx_Dir/jumps.html" */

/*******************************
 * If JUMPBUFFER is set to TRUE here or in lynx.cfg the last entered
 * jump shortcut, if any, will be offered as a default for reuse or
 * editing when the JUMP command is entered.  All previously used
 * shortcuts can be accessed for reuse or editing via a circular buffer
 * invoked with the Up-Arrow or Down-Arrow keys after entering the JUMP
 * command, whether or not a default is offered.  If you have multiple
 * jumps files and corresponding key mappings, each will have its own
 * circular buffer.
 */
#define JUMPBUFFER	  FALSE

/********************************
 * If PERMIT_GOTO_FROM_JUMP is defined, then a : or / in a jump target
 * will be treated as a full or partial URL (to be resolved versus the
 * startfile), and will be handled analogously to a 'g'oto command.
 * Such "random URLs" will be entered in the circular buffer for goto
 * URLs, not the buffer for jump targets (shortcuts).  If the target
 * is the single character ':', it will be treated equivalently to an
 * Up-Arrow or Down-Arrow following a 'g'oto command, for accessing the
 * circular buffer of goto URLs.
 */
/* #define PERMIT_GOTO_FROM_JUMP */

/*****************************
 * If LYNX_HOST_NAME is defined here and/or in lynx.cfg, it will be
 * treated as an alias for the local host name in checks for URLs on
 * the local host (e.g., when the -localhost switch is set), and this
 * host name, "localhost", and HTHostName (the fully qualified domain
 * name of the system on which Lynx is running) will all be passed as
 * local.  A different definition in lynx.cfg will override this one.
 */
/* #define LYNX_HOST_NAME "www.cc.ukans.edu" */

/*********************
 * LOCAL_DOMAIN is used for a tail match with the ut_host element of
 * the utmp or utmpx structure on systems with utmp capabilities, to
 * determine if a user is local to your campus or organization when
 * handling -restrictions=inside_foo or outside_foo settings for ftp,
 * news, telnet/tn3270 and rlogin URLs.  An "inside" user is assumed
 * if your system does not have utmp capabilities.  CHANGE THIS here
 * or in lynx.cfg.
 */
#define LOCAL_DOMAIN "ukans.edu"

/********************************
* The DEFAULT_CACHE_SIZE specifies the number of WWW documents to be
* cached in memory at one time.
*
* This so-called cache size (actually, number) may be modified in lynx.cfg
* and or with the command line argument -cache=NUMBER  The minimum allowed
* value is 2, for the current document and at least one to fetch, and there
* is no absolute maximum number of cached documents.  On Unix, and VMS not
* compiled with VAXC, whenever the number is exceeded the least recently
* displayed document will be removed from memory.
*
* On VMS compiled with VAXC, the DEFAULT_VIRTUAL_MEMORY_SIZE specifies the
* amount (bytes) of virtual memory that can be allocated and not yet be freed
* before previous documents are removed from memory.  If the values for both
* the DEFAULT_CACHE_SIZE and DEFAULT_VIRTUAL_MEMORY_SIZE are exceeded, then
* least recently displayed documents will be freed until one or the other
* value is no longer exceeded.  The value can be modified in lynx.cfg.
*
* The Unix and VMS but not VAXC implementations use the C library malloc's
* and calloc's for memory allocation, and procedures for taking the actual
* amount of cache into account still need to be developed.  They use only
* the DEFAULT_CACHE_SIZE value, and that specifies the absolute maximum
* number of documents to cache (rather than the maximum number only if
* DEFAULT_VIRTUAL_MEMORY_SIZE has been exceeded, as with VAXC/VAX).
*/
#define DEFAULT_CACHE_SIZE 10

#if defined(VMS) && defined(VAXC) && !defined(__DECC)
#define DEFAULT_VIRTUAL_MEMORY_SIZE 512000
#endif /* VMS && VAXC && !__DECC */

/********************************
 * If ALWAYS_RESUBMIT_POSTS is set TRUE, Lynx always will resubmit forms
 * with method POST, dumping any cache from a previous submission of the
 * form, including when the document returned by that form is sought with
 * the PREV_DOC command or via the history list.  Lynx always resubmits
 * forms with method POST when a submit button or a submitting text input
 * is activated, but normally retrieves the previously returned document
 * if it had links which you activated, and then go back with the PREV_DOC
 * command or via the history list.
 *
 * The default defined here can be changed in lynx.cfg, and can be toggled
 * via the -resubmit_posts command line switch.
 */
#define ALWAYS_RESUBMIT_POSTS FALSE

/********************************
 * CHARACTER_SET defines the default character set, i.e., that assumed
 * to be installed on the user's terminal.  It determines which characters
 * or strings will be used to represent 8-bit character entities within
 * HTML.  New character sets may be defined as explained in the README
 * files of the src/chrtrans directory in the Lynx source code distribution.
 * For Asian (CJK) character sets, it also determines how Kanji code will
 * be handled.  The default defined here can be changed in lynx.cfg, and
 * via the 'o'ptions menu.  The 'o'ptions menu setting will be stored in
 * the user's RC file whenever those settings are saved, and thereafter
 * will be used as the default.  Also see lynx.cfg for information about
 * the -raw switch and LYK_RAW_TOGGLE command.
 *
 * Since Lynx now supports a wide range of platforms it may be useful
 * to note that cpXXX codepages used by IBM PC compatible computers,
 * and windows-xxxx used by native MS-Windows apps.
 *
 *  Recognized character sets include:
 *
 *     string for 'O'ptions Menu          MIME name
 *     ===========================        =========
 *     7 bit approximations (US-ASCII)    us-ascii
 *     Western (ISO-8859-1)               iso-8859-1
 *     Western (cp850)                    cp850
 *     Western (windows-1252)             windows-1252
 *     IBM PC US codepage (cp437)         cp437
 *     DEC Multinational                  dec-mcs
 *     Macintosh (8 bit)                  macintosh
 *     NeXT character set                 next
 *     HP Roman8                          hp-roman8
 *     Chinese                            euc-cn
 *     Japanese (EUC-JP)                  euc-jp
 *     Japanese (Shift_JIS)               shift_jis
 *     Korean                             euc-kr
 *     Taipei (Big5)                      big5
 *     Vietnamese (VISCII)                viscii
 *     Eastern European (ISO-8859-2)      iso-8859-2
 *     Eastern European (cp852)           cp852
 *     Eastern European (windows-1250)    windows-1250
 *     Latin 3 (ISO-8859-3)               iso-8859-3
 *     Latin 4 (ISO-8859-4)               iso-8859-4
 *     Baltic Rim (cp775)                 cp775
 *     Baltic Rim (windows-1257)          windows-1257
 *     Cyrillic (ISO-8859-5)              iso-8859-5
 *     Cyrillic (cp866)                   cp866
 *     Cyrillic (windows-1251)            windows-1251
 *     Cyrillic (KOI8-R)                  koi8-r
 *     Arabic (ISO-8859-6)                iso-8859-6
 *     Arabic (cp864)                     cp864
 *     Arabic (windows-1256)              windows-1256
 *     Greek (ISO-8859-7)                 iso-8859-7
 *     Greek (cp737)                      cp737
 *     Greek2 (cp869)                     cp869
 *     Greek (windows-1253)               windows-1253
 *     Hebrew (ISO-8859-8)                iso-8859-8
 *     Hebrew (cp862)                     cp862
 *     Hebrew (windows-1255)              windows-1255
 *     Turkish (ISO-8859-9)               iso-8859-9
 *     ISO-8859-10                        iso-8859-10
 *     Ukrainian Cyrillic (cp866u)        cp866u
 *     Ukrainian Cyrillic (KOI8-U)        koi8-u
 *     UNICODE (UTF-8)                    utf-8
 *     RFC 1345 w/o Intro                 mnemonic+ascii+0
 *     RFC 1345 Mnemonic                  mnemonic
 *     Transparent                        x-transparent
 */
#define CHARACTER_SET "iso-8859-1"

/*****************************
 * PREFERRED_LANGUAGE is the language in MIME notation (e.g., "en",
 * "fr") which will be indicated by Lynx in its Accept-Language headers
 * as the preferred language.  If available, the document will be
 * transmitted in that language.  This definition can be overridden via
 * lynx.cfg.  Users also can change it via the 'o'ptions menu and save
 * that preference in their RC file.  This may be a comma-separated list
 * of languages in decreasing preference.
 */
#define PREFERRED_LANGUAGE "en"

/*****************************
 * PREFERRED_CHARSET specifies the character set in MIME notation (e.g.,
 * "ISO-8859-2", "ISO-8859-5") which Lynx will indicate you prefer in
 * requests to http servers using an Accept-Charsets header.
 * This definition can be overridden via lynx.cfg.  Users also can change it
 * via the 'o'ptions menu and save that preference in their RC file.
 * The value should NOT include "ISO-8859-1" or "US-ASCII", since those
 * values are always assumed by default.
 * If a file in that character set is available, the server will send it.
 * If no Accept-Charset header is present, the default is that any
 * character set is acceptable.  If an Accept-Charset header is present,
 * and if the server cannot send a response which is acceptable
 * according to the Accept-Charset header, then the server SHOULD send
 * an error response with the 406 (not acceptable) status code, though
 * the sending of an unacceptable response is also allowed. (RFC2068)
 */
#define PREFERRED_CHARSET ""

/*****************************
* If MULTI_BOOKMARK_SUPPORT is set to MBM_STANDARD or MBM_ADVANCED, and
* BLOCK_MULTI_BOOKMARKS (see below) is FALSE, and sub-bookmarks exist, all
* bookmark operations will first prompt the user to select an active
* sub-bookmark file or the default bookmark file.  MBM_OFF is the default so
* that one (the default) bookmark file will be available initially.  The
* default set here can be overridden in lynx.cfg.  The user can turn on
* multiple bookmark support via the 'o'ptions menu, and can save that choice as
* the startup default via the .lynxrc file.  When on, the setting can be
* STANDARD or ADVANCED.  If support is set to the latter, and the user mode
* also is ADVANCED, the VIEW_BOOKMARK command will invoke a status line prompt
* at which the user can enter the letter token (A - Z) of the desired bookmark,
* or '=' to get a menu of available bookmark files.  The menu always is
* presented in NOVICE or INTERMEDIATE mode, or if the support is set to
* STANDARD.  No prompting or menu display occurs if only one (the startup
* default) bookmark file has been defined (define additional ones via the
* 'o'ptions menu).  The startup default, however set, can be overridden on the
* command line via the -restrictions=multibook or the -anonymous or -validate
* switches.
*/
#ifndef MULTI_BOOKMARK_SUPPORT
#define MULTI_BOOKMARK_SUPPORT MBM_OFF
#endif /* MULTI_BOOKMARK_SUPPORT */

/*****************************
* If BLOCK_MULTI_BOOKMARKS is set TRUE, multiple bookmark support will
* be forced off, and cannot be toggled on via the 'o'ptions menu.  This
* compilation setting can be overridden via lynx.cfg.
*/
#ifndef BLOCK_MULTI_BOOKMARKS
#define BLOCK_MULTI_BOOKMARKS FALSE
#endif /* BLOCK_MULTI_BOOKMARKS */

/********************************
 * URL_DOMAIN_PREFIXES and URL_DOMAIN_SUFFIXES are strings which will be
 * prepended (together with a scheme://) and appended to the first element
 * of command line or 'g'oto arguments which are not complete URLs and
 * cannot be opened as a local file (file://localhost/string).  Both
 * can be comma-separated lists.  Each prefix must end with a dot, each
 * suffix must begin with a dot, and either may contain other dots (e.g.,
 * .co.jp).  The default lists are defined here, and can be changed
 * in lynx.cfg.  Each prefix will be used with each suffix, in order,
 * until a valid Internet host is created, based on a successful DNS
 * lookup (e.g., foo will be tested as www.foo.com and then www.foo.edu
 * etc.).  The first element can include a :port and/or /path which will
 * be restored with the expanded host (e.g., wfbr:8002/dir/lynx will
 * become http://www.wfbr.edu:8002/dir/lynx).  The prefixes will not be
 * used if the first element ends in a dot (or has a dot before the
 * :port or /path), and similarly the suffixes will not be used if the
 * the first element begins with a dot (e.g., .nyu.edu will become
 * http://www.nyu.edu without testing www.nyu.com).  Lynx will try to
 * guess the scheme based on the first field of the expanded host name,
 * and use "http://" as the default (e.g., gopher.wfbr.edu or gopher.wfbr.
 * will be made gopher://gopher.wfbr.edu).
 */
#define URL_DOMAIN_PREFIXES "www."
#define URL_DOMAIN_SUFFIXES ".com,.edu,.net,.org"

/********************************
 * If LIST_NEWS_NUMBERS is set TRUE, Lynx will use an ordered list
 * and include the numbers of articles in news listings, instead of
 * using an unordered list.
 *
 * The default defined here can be changed in lynx.cfg.
 */
#define LIST_NEWS_NUMBERS FALSE

/********************************
 * If LIST_NEWS_DATES is set TRUE, Lynx will include the dates of
 * articles in news listings.  The dates always are included in the
 * articles, themselves.
 *
 * The default defined here can be changed in lynx.cfg.
 */
#define LIST_NEWS_DATES FALSE

/*************************
 * Set NEWS_POSTING to FALSE if you do not want to support posting to
 * news groups via Lynx.  If left TRUE, Lynx will use its news gateway to
 * post new messages or followups to news groups, using the URL schemes
 * described in the "Supported URL" section of the online 'h'elp.  The
 * posts will be attempted via the nntp server specified in the URL, or
 * if none was specified, via the NNTPSERVER configuration or environment
 * variable.  Links with these URLs for posting or sending followups are
 * created by the news gateway when reading group listings or articles
 * from nntp servers if the server indicates that it permits posting.
 * The setting here can be changed in lynx.cfg.
 */
#define NEWS_POSTING TRUE

/*************************
 * Define LYNX_SIG_FILE to the name of a file containing a signature which
 * can be appended to email messages and news postings or followups.  The
 * user will be prompted whether to append it.  It is sought in the home
 * directory.  If it is in a subdirectory, begin it with a dot-slash
 * (e.g., ./lynx/.lynxsig).  The definition here can be changed in lynx.cfg.
 */
#define LYNX_SIG_FILE ".lynxsig"

/********************************
 * BIBP_GLOBAL_SERVER is the default global server for bibp: links, used
 * when a local bibhost or document-specified citehost is unavailable.
 */
#define BIBP_GLOBAL_SERVER "http://usin.org/"

/********************************
 * If USE_SELECT_POPUPS is set FALSE, Lynx will present a vertical list
 * of radio buttons for the OPTIONs in SELECT blocks which lack the
 * MULTIPLE attribute, instead of using a popup menu.  Note that if
 * the MULTIPLE attribute is present in the SELECT start tag, Lynx
 * always will create a vertical list of checkboxes for the OPTIONs.
 *
 * The default defined here can be changed in lynx.cfg.  It can be
 * set and saved via the 'o'ptions menu to override the compilation
 * and configuration defaults, and the default always can be toggled
 * via the -popup command line switch.
 */
#define USE_SELECT_POPUPS TRUE

/********************************
 * If COLLAPSE_BR_TAGS is set FALSE, Lynx will not collapse serial
 * BR tags.  If set TRUE, two or more concurrent BRs will be collapsed
 * into a single blank line.  Note that the valid way to insert extra
 * blank lines in HTML is via a PRE block with only newlines in the
 * block.
 *
 * The default defined here can be changed in lynx.cfg.
 */
#define COLLAPSE_BR_TAGS TRUE

/********************************
 * If SET_COOKIES is set FALSE, Lynx will ignore Set-Cookie headers
 * in http server replies.
 *
 * The default defined here can be changed in lynx.cfg, and can be toggled
 * via the -cookies command line switch.
 */
#define SET_COOKIES TRUE

/*******************************
 * If ACCEPT_ALL_COOKIES is set TRUE, and SET_COOKIES is TRUE, Lynx will
 * accept all cookies.
 *
 * The default defined here can be changed in lynx.cfg, and .lynxrc, or
 * toggled via the -accept_all_cookies command line switch.
 */
#define ACCEPT_ALL_COOKIES FALSE


/****************************************************************
 *   Section 2.   Things that you probably want to change or review
 *
 */

/*****************************
 * The following three definitions set the number of seconds for
 * pauses following status line messages that would otherwise be
 * replaced immediately, and are more important than the unpaused
 * progress messages.  Those set by INFOSECS are also basically
 * progress messages (e.g., that a prompted input has been canceled)
 * and should have the shortest pause.  Those set by MESSAGESECS are
 * informational (e.g., that a function is disabled) and should have
 * a pause of intermediate duration.  Those set by ALERTSECS typically
 * report a serious problem and should be paused long enough to read
 * whenever they appear (typically unexpectedly).  The default values
 * defined here can be modified via lynx.cfg, should longer pauses be
 * desired for braille-based access to Lynx.
 */
#define INFOSECS 1
#define MESSAGESECS 2
#define ALERTSECS 3

#define DEBUGSECS 0
#define REPLAYSECS 0

/******************************
 * SHOW_COLOR controls whether the program displays in color by default.
 */
#ifdef COLOR_CURSES
#define SHOW_COLOR TRUE
#else
#define SHOW_COLOR FALSE
#endif

/******************************
 * SHOW_CURSOR controls whether or not the cursor is hidden or appears
 * over the current link, or current option in select popup windows.
 * Showing the cursor is handy if you are a sighted user with a poor
 * terminal that can't do bold and reverse video at the same time or
 * at all.  It also can be useful to blind users, as an alternative
 * or supplement to setting LINKS_AND_FIELDS_ARE_NUMBERED or
 * LINKS_ARE_NUMBERED.
 *
 * The default defined here can be changed in lynx.cfg.  It can be
 * set and saved via the 'o'ptions menu to override the compilation
 * and configuration defaults, and the default always can be toggled
 * via the -show_cursor command line switch.
 */
#define SHOW_CURSOR FALSE

/******************************
* UNDERLINE_LINKS controls whether links are underlined by default, or shown
* in bold.  Normally this default is set from the configure script.
*/
#ifndef HAVE_CONFIG_H
#define UNDERLINE_LINKS FALSE
#endif

/******************************
* VERBOSE_IMAGES controls whether or not Lynx replaces the [LINK], [INLINE]
* and [IMAGE] comments (for images without ALT) with filenames of these
* images.  This is extremely useful because now we can determine immediately
* what images are just decorations (button.gif, line.gif) and what images are
* important.
*
* The default defined here can be changed in lynx.cfg.
*/
#define VERBOSE_IMAGES TRUE

/******************************
 * BOXVERT and BOXHORI control the layout of popup menus.  Set to 0 if your
 * curses supports line-drawing characters, set to '*' or any other character
 * to not use line-drawing (e.g., '|' for vertical and '-' for horizontal).
 */
#ifndef HAVE_CONFIG_H
#ifdef DOSPATH
#define BOXVERT 0
#define BOXHORI 0
#else
#define BOXVERT '|'
/* #define BOXVERT 0 */
#define BOXHORI '-'
/* #define BOXHORI 0 */
#endif /* DOSPATH */
#endif	/* !HAVE_CONFIG_H */

/******************************
 * LY_UMLAUT controls the 7-bit expansion of characters with dieresis or
 * umlaut.  If defined, a digraph is displayed, e.g., auml --> ae
 * Otherwise, a single character is displayed,  e.g., auml --> a
 * Note that this is currently not supported with the chartrans code,
 * or rather it doesn't have an effect if translations for a display
 * character set are taken from one of the *.tbl files in src/chrtrans.
 * One would have to modify the corresponding *.tbl file to change the
 # 7-bit replacements for these characters.
 */
#define LY_UMLAUT

/*******************************
 * Execution links/scripts configuration.
 *
 * Execution links and scripts allow you to run
 * local programs by activating links within Lynx.
 *
 * An execution link is of the form:
 *
 *     lynxexec:<COMMAND>
 * or:
 *     lynxexec://<COMMAND>
 * or:
 *     lynxprog:<COMMAND>
 * or:
 *     lynxprog://<COMMAND>
 *
 * where <COMMAND> is a command that Lynx will run when the link is
 * activated.  The double-slash should be included if the command begins
 * with an '@', as for executing VMS command files.  Otherwise, the double-
 * slash can be omitted.
 * Use lynxexec for commands or scripts that generate a screen output which
 * should be held via a prompt to press <return> before returning to Lynx
 * for display of the current document.
 * Use lynxprog for programs such as mail which do not require a pause before
 * Lynx restores the display of the current document.
 *
 * Execution scripts take the form of a standard
 * URL.  Extension mapping or MIME typing is used
 * to decide if the file is a script and should be
 * executed.  The current extensions are:
 * .csh, .ksh, and .sh on UNIX systems and .com on
 * VMS systems.  Any time a file of this type is
 * accessed Lynx will look at the user's options
 * settings to decide if the script can be executed.
 * Current options include: Only exec files that
 * reside on the local machine and are referenced
 * with a "file://localhost" URL, All execution
 * off, and all execution on.
 *
 * The following definitions will add execution
 * capabilities to Lynx.  You may define none, one
 * or both.
 *
 * I strongly recommend that you define neither one
 * of these since execution links/scripts can represent
 * very serious security risk to your system and its
 * users.  If you do define these I suggest that
 * you only allow users to execute files/scripts
 * that reside on your local machine.
 *
 * YOU HAVE BEEN WARNED!
 *
 * Note: if you are enabling execution scripts you should
 * also see src/HTInit.c to verify/change the execution
 * script extensions and/or commands.
 */
/* #define EXEC_LINKS  */
/* #define EXEC_SCRIPTS  */

#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)

/**********
 * if ENABLE_OPTS_CHANGE_EXEC is defined, the user will be able to change
 * the execution status within the Options Menu.
 */
/* #define ENABLE_OPTS_CHANGE_EXEC */

/**********
 * if NEVER_ALLOW_REMOTE_EXEC is defined,
 * local execution of scripts or lynxexec & lynxprog URLs will be implemented
 * only from HTML files that were accessed via a "file://localhost/" URL
 * and the Options Menu for "Local executions links" will allow toggling
 * only between "ALWAYS OFF" and "FOR LOCAL FILES ONLY".
 */
/* #define NEVER_ALLOW_REMOTE_EXEC */

/*****************************
 * These are for executable shell scripts and links.
 * Set to FALSE unless you really know what you're
 * doing.
 *
 * This only applies if you are compiling with EXEC_LINKS or
 * EXEC_SCRIPTS defined.
 *
 * The first two settings:
 * LOCAL_EXECUTION_LINKS_ALWAYS_ON
 * LOCAL_EXECUTION_LINKS_ON_BUT_NOT_REMOTE
 * specify the DEFAULT settings of the users execution link
 * options (they can also be overridden in lynx.cfg), but
 * the user may still change those options.
 * If you do not wish the user to be able to change the
 * execution link settings you may wish to use the command line option:
 *    -restrictions=exec_frozen
 *
 * LOCAL_EXECUTION_LINKS_ALWAYS_ON will be FALSE
 * if NEVER_ALLOW_REMOTE_EXEC has been defined.
 *
 * if LOCAL_EXECUTION_LINKS_ALWAYS_OFF_FOR_ANONYMOUS is true,
 * all execution links will be disabled when the -anonymous
 * command-line option is used.  Anonymous users are not allowed
 * to change the execution options from within the Lynx Options Menu,
 * so you might be able to use this option to enable execution links
 * and set LOCAL_EXECUTION_LINKS_ON_BUT_NOT_REMOTE to TRUE
 * to give anonymous execution-link capability without compromising
 * your system (see comments about TRUSTED_EXEC rules in lynx.cfg ).
 */

#define LOCAL_EXECUTION_LINKS_ALWAYS_ON          FALSE
#define LOCAL_EXECUTION_LINKS_ON_BUT_NOT_REMOTE  FALSE
#define LOCAL_EXECUTION_LINKS_ALWAYS_OFF_FOR_ANONYMOUS FALSE

#endif /*  defined(EXEC_LINKS) || defined(EXEC_SCRIPTS) */

/**********
 * *** This is for those -- e.g. DOS users -- who do not have configure;
 * *** others should use the configure switch --enable-lynxcgi-links .
 *
 * UNIX:
 * =====
 * CGI script support.  Defining LYNXCGI_LINKS allows you to use the
 *
 *   lynxcgi:path
 *
 * URL which allows lynx to access a cgi script directly without the need for
 * a http daemon.  Redirection is not supported but just about everything
 * else is.  If the path is not an executable file then the URL is
 * rewritten as file://localhost and passed to the file loader.  This means
 * that if your http:html files are currently set up to use relative
 * addressing, you should be able to fire up your main page with lynxcgi:path
 * and everything should work as if you were talking to the http daemon.
 *
 * Note that TRUSTED_LYNXCGI directives must be defined in your lynx.cfg file
 * if you wish to place restrictions on source documents and/or paths for
 * lynxcgi links.
 *
 * The cgi scripts are called with a fork()/execve() sequence so you don't
 * have to worry about people trying to abuse the code. :-)
 *
 *     George Lindholm (George.Lindholm@ubc.ca)
 *
 * VMS:
 * ====
 * The lynxcgi scheme, if enabled, yields an informational message regardless
 * of the path, and use of the freeware OSU DECthreads server as a local
 * script server is recommended instead of lynxcgi URLs.  Uncomment the
 * following line to define LYNXCGI_LINKS, and when running Lynx, enter
 * lynxcgi:advice  as a G)oto URL for more information and links to the
 * OSU server distribution.
 */
#ifndef HAVE_CONFIG_H
/* #define LYNXCGI_LINKS */
#endif

/*********************************
 *  MAIL_SYSTEM_ERROR_LOGGING will send a message to the owner of
 *  the information if there is one, every time
 *  that a document cannot be accessed!
 *  This is just the default, it can be changed in lynx.cfg, and error
 *  logging can be turned off with the -nolog command line option.
 *
 *  NOTE: This can generate A LOT of mail, be warned.
 */
#define MAIL_SYSTEM_ERROR_LOGGING   FALSE  /*mail a message for every error?*/

/*********************************
 *  If a document cannot be accessed, and MAIL_SYSTEM_ERROR_LOGGING
 *  is on and would send a message to the owner of the information,
 *  but no owner is known, then the message will be sent to ALERTMAIL
 *  instead - if it is defined as a non-empty email address.
 *
 *  NOTE: This can generate A REAL LOT of mail, be warned!!!
 */
/* #define ALERTMAIL "webmaster@localhost" */ /*error recipient if no owner*/

/*********************************
 * If CHECKMAIL is set to TRUE, the user will be informed (via a status line
 * message) about the existence of any unread mail at startup of Lynx, and
 * will get status line messages if subsequent new mail arrives.  If a jumps
 * file with a lynxprog URL for invoking mail is available, or your html
 * pages include an mail launch file URL, the user thereby can access mail
 * and read the messages.
 * This is just the default, it can be changed in lynx.cfg.  The checks and
 * status line reports will not be performed if Lynx has been invoked with
 * the -restrictions=mail switch.
 *
 *  VMS USERS !!!
 * New mail is normally broadcast as it arrives, via "unsolicited screen
 * broadcasts", which can be "wiped" from the Lynx display via the Ctrl-W
 * command.  You may prefer to disable the broadcasts and use CHECKMAIL
 * instead (e.g., in a public account which will be used by people who
 * are ignorant about VMS).
 */
#define CHECKMAIL	FALSE	/* report unread and new mail messages */

/*********************************
 * Vi or Emacs movement keys.  These are defaults,
 * which can be changed in lynx.cfg , the Options Menu or .lynxrc .
 */
#define VI_KEYS_ALWAYS_ON	FALSE /* familiar h j k l */
#define EMACS_KEYS_ALWAYS_ON	FALSE /* familiar ^N ^P ^F ^B */

/*********************************
 * DEFAULT_KEYPAD_MODE may be set to NUMBERS_AS_ARROWS
 *                                or LINKS_ARE_NUMBERED
 *                                or LINKS_AND_FIELDS_ARE_NUMBERED
 * to specify whether numbers (e.g. [10]) appear before all links,
 * allowing immediate access by entering the number on the keyboard,
 * or numbers on the numeric key-pad work like arrows;
 * the 3rd option causes form fields also to be preceded by numbers.
 * The first two options (but not the last) can be changed in lynx.cfg
 * and all three can be changed via the Options Menu.
 */
#define DEFAULT_KEYPAD_MODE	NUMBERS_AS_ARROWS

/********************************
 * The default search.
 * This is a default that can be overridden in lynx.cfg or by the user!
 */
#define CASE_SENSITIVE_ALWAYS_ON    FALSE /* case sensitive user search */

/********************************
 * If NO_DOT_FILES is set TRUE here or in lynx.cfg, the user will not be
 * allowed to specify files beginning with a dot in reply to output filename
 * prompts, and files beginning with a dot (e.g., file://localhost/foo/.lynxrc)
 * will not be included in the directory browser's listings.  The setting here
 * will be overridden by the setting in lynx.cfg.  If FALSE, you can force it
 * to be treated as TRUE via -restrictions=dotfiles (or -anonymous, which sets
 * this and most other restrictions).
 *
 * If it is FALSE at startup of Lynx, the user can regulate it via the
 * 'o'ptions menu, and may save the preference in the RC file.
 */
#define NO_DOT_FILES    TRUE  /* disallow access to dot files */

/********************************
 * If MAKE_LINKS_FOR_ALL_IMAGES is TRUE, all images will be given links
 * which can be ACTIVATEd.  For inlines, the ALT or pseudo-ALT ("[INLINE]")
 * strings will be links for the resolved SRC rather than just text.  For
 * ISMAP or other graphic links, the ALT or pseudo-ALT ("[ISMAP]" or "[LINK]")
 * strings will have '-' and a link labeled "[IMAGE]" for the resolved SRC
 * appended. See also VERBOSE_IMAGES flag.
 *
 * The default defined here can be changed in lynx.cfg, and the user can
 * use LYK_IMAGE_TOGGLE to toggle the feature on or off at run time.
 *
 * The default also can be toggled via an "-image_links" command line switch.
 */
#define MAKE_LINKS_FOR_ALL_IMAGES	FALSE /* inlines cast to links */

/********************************
 * If MAKE_PSEUDO_ALTS_FOR_INLINES is FALSE, inline images which do not
 * specify an ALT string will not have "[INLINE]" inserted as a pseudo-ALT,
 * i.e., they'll be treated as having ALT="".  If MAKE_LINKS_FOR_ALL_IMAGES
 * is defined or toggled to TRUE, however, the pseudo-ALTs will be created
 * for inlines, so that they can be used as links to the SRCs.
 * See also VERBOSE_IMAGES flag.
 *
 * The default defined here can be changed in lynx.cfg, and the user can
 * use LYK_INLINE_TOGGLE to toggle the feature on or off at run time.
 *
 * The default also can be toggled via a "-pseudo_inlines" command line
 * switch.
 */
#define MAKE_PSEUDO_ALTS_FOR_INLINES	TRUE /* Use "[INLINE]" pseudo-ALTs */

/********************************
 * If SUBSTITUTE_UNDERSCORES is TRUE, the _underline_ format will be used
 * for emphasis tags in dumps.
 *
 * The default defined here can be changed in lynx.cfg, and the user can
 * toggle the default via a "-underscore" command line switch.
 */
#define SUBSTITUTE_UNDERSCORES	FALSE /* Use _underline_ format in dumps */

/********************************
 * If QUIT_DEFAULT_YES is defined as TRUE then when the QUIT command
 * is entered, any response other than n or N will confirm.  Define it
 * as FALSE if you prefer the more conservative action of requiring an
 * explicit Y or y to confirm.  The default defined here can be changed
 * in lynx.cfg.
 */
#define QUIT_DEFAULT_YES	TRUE

/********************************
 * If TEXT_SUBMIT_CONFIRM_WANTED is defined (to anything), the user will be
 * prompted for confirmation before Lynx submits a form with only one input
 * field (of type text) to the server, after the user has pressed <return>
 * or <enter> on the field.  Since the is no other way such as a "submit"
 * button to submit, normally the form gets submitted automatically in this
 * case, but some users may find this surprising and expect <return> to just
 * move to the next link as for other text entry fields.
 */
/* #define TEXT_SUBMIT_CONFIRM_WANTED */

/********************************
 * If TEXTFIELDS_MAY_NEED_ACTIVATION is defined (to anything),
 * the option TEXTFIELDS_NEED_ACTIVATION in lynx.cfg or the command
 * line option -tna can be used to require explicit activation
 * before text input fields can be changed with the built-in line
 * editor.
 */

#define TEXTFIELDS_MAY_NEED_ACTIVATION

/********************************
 * The following three definitions control some aspects of extended
 * textarea handling.  TEXTAREA_EXPAND_SIZE is the number of new empty
 * lines that get appended at the end of a textarea by a GROWTEXTAREA
 * key.  If TEXTAREA_AUTOGROW is defined (to anything), <return> or
 * <enter> in the last line of a textarea automatically extends the
 * area by adding a new line.  If TEXTAREA_AUTOEXTEDIT is defined (to
 * anything), a key mapped to DWIMEDIT will invoke the external editor
 * like EDITTEXTAREA when used in a text input field.  Comment those
 * last two definitions out to disable the corresponding behavior.
 * See under KEYMAP in lynx.cfg for mapping keys to GROWTEXTAREA or
 * DWIMEDIT actions.
 */
#define TEXTAREA_EXPAND_SIZE  5
#define TEXTAREA_AUTOGROW
#define TEXTAREA_AUTOEXTEDIT

/********************************
 * If BUILTIN_SUFFIX_MAPS is defined (to anything), default mappings
 * for file extensions (aka suffixes) will be compiled in (see
 * src/HTInit.c).  By removing the definition, the default mappings
 * are suppressed except for a few very basic ones for text/html.
 * See GLOBAL_EXTENSION_MAP, PERSONAL_EXTENSION_MAP above and SUFFIX,
 * SUFFIX_ORDER in lynx.cfg for other ways to map file extensions.
 */

#define BUILTIN_SUFFIX_MAPS

/********************************
 * These definitions specify files created or used in conjunction
 * with traversals.  See CRAWL.ANNOUNCE for more information.
 */
#define TRAVERSE_FILE "traverse.dat"
#define TRAVERSE_FOUND_FILE "traverse2.dat"
#define TRAVERSE_REJECT_FILE "reject.dat"
#define TRAVERSE_ERRORS "traverse.errors"

/****************************************************************
 * The LYMessages_en.h header defines default, English strings
 * used in status line prompts, messages, and warnings during
 * program execution.  See the comments in LYMessages_en.h for
 * information on translating or customizing them for your site.
 */
#ifndef LYMESSAGES_EN_H
#include <LYMessages_en.h>
#endif /* !LYMESSAGES_EN_H */


/****************************************************************
 * DEFAULT_VISITED_LINKS may be set to one or more of
 *					VISITED_LINKS_AS_FIRST_V
 *					VISITED_LINKS_AS_TREE
 *					VISITED_LINKS_AS_LATEST
 *					VISITED_LINKS_REVERSE
 * to change the organization of the Visited Links page.
 *
 * (Not all combinations are meaningful; see src/LYrcFile.c for a list
 * in the visited_links_tbl table).
 */
#define DEFAULT_VISITED_LINKS (VISITED_LINKS_AS_LATEST | VISITED_LINKS_REVERSE)


/****************************************************************
 *   Section 3.   Things that you should not change until you
 *  		  have a good knowledge of the program
 */

#define LYNX_NAME "Lynx"
/* The strange-looking comments on the next line tell PRCS to replace
 * the version definition with the Project Version on checkout.  Just
 * ignore it. - kw */
/* $Format: "#define LYNX_VERSION \"$ProjectVersion$\""$ */
#define LYNX_VERSION "2.8.5rel.2"
#define LYNX_WWW_HOME "http://lynx.isc.org/"
#define LYNX_WWW_DIST "http://lynx.isc.org/current/"
/* $Format: "#define LYNX_DATE \"$ProjectDate$\""$ */
#define LYNX_DATE "Thu, 22 Apr 2004 16:08:10 -0700"
#define LYNX_DATE_OFF 5		/* truncate the automatically-generated date */
#define LYNX_DATE_LEN 11	/* truncate the automatically-generated date */

#define LINESIZE 1024		/* max length of line to read from file */
#define MAXHIST  1024		/* max links we remember in history */
#define MAXLINKS 1024		/* max links on one screen */

#ifndef SEARCH_GOAL_LINE
#define SEARCH_GOAL_LINE 4	/* try to position search target there */
#endif

#define MAXCHARSETS 60		/* max character sets supported */
#define TRST_MAXROWSPAN 10000	/* max rowspan accepted by TRST code */
#define TRST_MAXCOLSPAN 1000	/* max colspan and COL/COLGROUP span accepted */
#define SAVE_TIME_NOT_SPACE	/* minimize number of some malloc calls */

/* Win32 may support more, but old win16 helper apps may not. */
#if defined(__DJGPP__) || defined(_WINDOWS)
#define FNAMES_8_3
#endif

#ifdef FNAMES_8_3
#define HTML_SUFFIX ".htm"
#else
#define HTML_SUFFIX ".html"
#endif

#define BIN_SUFFIX  ".bin"
#define TEXT_SUFFIX ".txt"

#ifdef VMS
/*
**  Use the VMS port of gzip for uncompressing both .Z and .gz files.
*/
#define UNCOMPRESS_PATH "gzip -d"
#define COPY_PATH	"copy/nolog/noconf"
#define GZIP_PATH       "gzip"
#define BZIP2_PATH      "bzip2"
#define TELNET_PATH     "telnet"
#define TN3270_PATH     "tn3270"
#define RLOGIN_PATH     "rlogin"

#else

#ifdef DOSPATH

#ifdef _WINDOWS
#ifdef SYSTEM_MAIL
#undef SYSTEM_MAIL
#endif
#ifdef SYSTEM_MAIL_FLAGS
#undef SYSTEM_MAIL_FLAGS
#endif
#ifdef USE_ALT_BLAT_MAILER
#define SYSTEM_MAIL		"BLAT"
#define SYSTEM_MAIL_FLAGS	""
#else
#define SYSTEM_MAIL		"BLATJ"
#define SYSTEM_MAIL_FLAGS	""
#endif
#else
/* have to define something... */
#ifdef SYSTEM_MAIL
#undef SYSTEM_MAIL
#endif /* SYSTEM_MAIL */
#define SYSTEM_MAIL "sendmail"
#define SYSTEM_MAIL_FLAGS "-t -oi"
#endif

/*
**  The following executables may be used at run time.  Unless you change
**  the definitions to include the full directories, they will be sought
**  from your PATH at run-time; they should be available as "cp.exe",
**  "mv.exe" and so on.  To get those programs look for GNU-port stuff
**  elsewhere.
**  Currently, if compiled with -DUSE_ZLIB and without -DDIRED_SUPPORT
**  (default), the following from the list below are required:
**  MV_PATH   (mv.exe) - for bookmark handling (DEL_BOOKMARK command)
**  UNCOMPRESS_PATH    - for automatic decompression of files in Unix
**                       compress format
**  TELNET_PATH, TN3270_PATH, RLOGIN_PATH - for access to "telnet:",
**                                         "tn3270:", and "rlogin:" URLs.
**  If they are not defined right, the corresponding operations may fail
**  in unexpected and obscure ways!
**
**    WINDOWS/DOS
**    ===========
*/
#ifndef HAVE_CONFIG_H
#define COMPRESS_PATH   "compress"
#define UNCOMPRESS_PATH "uncompress"
#define UUDECODE_PATH   "uudecode"
#define ZCAT_PATH       "zcat"
#define GZIP_PATH       "gzip"
#define BZIP2_PATH      "bzip2"
#define MV_PATH         "mv"
#define INSTALL_PATH    "install"
#define TAR_PATH        "tar"
#define ZIP_PATH        "zip"
#define UNZIP_PATH      "unzip"
#define RM_PATH         "rm"
#define TELNET_PATH     "telnet"
#define TN3270_PATH     "tn3270"
#define RLOGIN_PATH     "rlogin"

/* see src/LYLocal.c for these */
#define TAR_UP_OPTIONS	 "-cf"
#define TAR_DOWN_OPTIONS "-xf"
#define TAR_PIPE_OPTIONS "-"
#define TAR_FILE_OPTIONS ""

/*
 * These are not used:
 * #define COPY_PATH       "cp"
 * #define CHMOD_PATH      "chmod"
 * #define MKDIR_PATH      "mkdir"
 * #define TOUCH_PATH      "touch"
 */
#endif /* HAVE_CONFIG_H */

#else	/* Unix */
	/* Standard locations are defined via the configure script.  When
	 * helper applications are in your home directory or other nonstandard
	 * locations, you probably will have to preset the path to them with
	 * environment variables (see INSTALLATION, Section II-1d).
	 */
#endif /* DOSPATH */
#endif /* VMS */


/***************************** 
 * I have not ported multibyte support for EBCDIC.  In fact, some multibyte
 * code in LYLowerCase() crashes on EBCDIC strings.  -- gil
 */
#if       ! defined(NOT_ASCII)
/***************************** 
 * SUPPORT_MULTIBYTE_EDIT provides better support of CJK characters to
 * Lynx's Line Editor.  JIS X0201 Kana is partially supported.  The
 * reason why I didn't support it fully is I think supporting it is not
 * required so much and I don't have an environment to test it. - TH
 */
#define SUPPORT_MULTIBYTE_EDIT
#endif /* ! defined(NOT_ASCII) */

/***************************** 
 * SUPPORT_CHDIR provides CD command (bound to 'C' by default).  It allows
 * changing directory to arbitrary location (if OS allows them).  If dired is
 * enabled, user will be able to visit any directory and view any file allowed
 * according to file permissions or ACLs.
 */
#define SUPPORT_CHDIR

/***************************** 
 * MARK_HIDDEN_LINKS controls whether hidden links are shown with the title
 * set by the HIDDEN_LINK_MARKER string in lynx.cfg
 */
#define MARK_HIDDEN_LINKS

/*****************************
 * USE_TH_JP_AUTO_DETECT, CONV_JISX0201KANA_JISX0208KANA,  
 * and KANJI_CODE_OVERRIDE are the macros for Japanese. - TH 
 */ 
/***************************** 
 * USE_TH_JP_AUTO_DETECT enables a new Japanese charset detection routine. 
 * With the old detection strategy, Lynx always thought a document was 
 * written in mixture of three kanji codes (JIS, EUC and SJIS).  The new 
 * strategy is for Lynx to first assume the document is written in one code 
 * or JIS + one other kanji code (JIS, EUC, SJIS, EUC+JIS and SJIS+JIS). 
 * The first assumption is usually correct, but if the assumption is wrong, 
 * Lynx falls back to the old assumption of the three kanji codes mixed. 
 */ 
#define USE_TH_JP_AUTO_DETECT 
 
/***************************** 
 * If CONV_JISX0201KANA_JISX0208KANA is set, Lynx will convert 
 * JIS X0201 Kana to JIS X0208 Kana, i.e., convert half-width kana 
 * to full-width. 
 */ 
#define CONV_JISX0201KANA_JISX0208KANA 
 
/***************************** 
 * Uncomment the following line to enable the kanji code override routine. 
 * The code can be changed by pressing ^L.  More precisely, this allows 
 * the user to override the assumption about the kanji code for the document 
 * which Lynx has made on the basis of a META tag and HTTP response. 
 */ 
/*#define KANJI_CODE_OVERRIDE */ 
 

/****************************************************************
 *  Section 4.  Things you MUST check only if you plan to use Lynx
 *              in an anonymous account (allow public access to Lynx).
 *              This section may be skipped by those people building
 *              Lynx for private use only.
 *
 */

/*****************************
 * Enter the name of your anonymous account if you have one
 * as ANONYMOUS_USER.  UNIX systems will use a cuserid
 * or get_login call to determine if the current user is
 * the ANONYMOUS_USER.  VMS systems will use getenv("USER").
 *
 * You may use the "-anonymous" option for multiple accounts,
 * or for precautionary reasons in the anonymous account, as well.
 *
 * Specify privileges for the anonymous account below.
 *
 * It is very important to have this correctly defined or include
 * the "-anonymous" command line option for invocation of Lynx
 * in an anonymous account!  If you do not you will be putting
 * yourself at GREAT security risk!
 */
#define ANONYMOUS_USER ""

/*******************************
 * In the following four pairs of defines,
 * INSIDE_DOMAIN means users connecting from inside your local domain,
 * OUTSIDE_DOMAIN means users connecting from outside your local domain.
 *
 * set to FALSE if you don't want users of your anonymous
 * account to be able to telnet back out
 */
#define CAN_ANONYMOUS_INSIDE_DOMAIN_TELNET	TRUE
#define CAN_ANONYMOUS_OUTSIDE_DOMAIN_TELNET	FALSE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to use ftp
 */
#define CAN_ANONYMOUS_INSIDE_DOMAIN_FTP		TRUE
#define CAN_ANONYMOUS_OUTSIDE_DOMAIN_FTP	FALSE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to use rlogin
 */
#define CAN_ANONYMOUS_INSIDE_DOMAIN_RLOGIN	TRUE
#define CAN_ANONYMOUS_OUTSIDE_DOMAIN_RLOGIN	FALSE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to read news OR post news articles.
 * These flags apply to "news", "nntp", "newspost", and "newsreply"
 * URLs, but not to "snews", "snewspost", or "snewsreply"
 * in case they are supported.
 */
#define CAN_ANONYMOUS_INSIDE_DOMAIN_READ_NEWS	TRUE
#define CAN_ANONYMOUS_OUTSIDE_DOMAIN_READ_NEWS	FALSE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to goto random URLs. (The 'g' command)
 */
#define CAN_ANONYMOUS_GOTO		TRUE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to goto particular URLs.
 */
#define CAN_ANONYMOUS_GOTO_BIBP		TRUE    /* BIBP maps to HTTP */
#define CAN_ANONYMOUS_GOTO_CSO		FALSE
#define CAN_ANONYMOUS_GOTO_FILE		FALSE
#define CAN_ANONYMOUS_GOTO_FINGER	TRUE
#define CAN_ANONYMOUS_GOTO_FTP		FALSE
#define CAN_ANONYMOUS_GOTO_GOPHER	FALSE
#define CAN_ANONYMOUS_GOTO_HTTP		TRUE
#define CAN_ANONYMOUS_GOTO_HTTPS	FALSE
#define CAN_ANONYMOUS_GOTO_LYNXCGI	FALSE
#define CAN_ANONYMOUS_GOTO_LYNXEXEC	FALSE
#define CAN_ANONYMOUS_GOTO_LYNXPROG	FALSE
#define CAN_ANONYMOUS_GOTO_MAILTO	TRUE
#define CAN_ANONYMOUS_GOTO_NEWS		FALSE
#define CAN_ANONYMOUS_GOTO_NNTP		FALSE
#define CAN_ANONYMOUS_GOTO_RLOGIN	FALSE
#define CAN_ANONYMOUS_GOTO_SNEWS	FALSE
#define CAN_ANONYMOUS_GOTO_TELNET	FALSE
#define CAN_ANONYMOUS_GOTO_TN3270	FALSE
#define CAN_ANONYMOUS_GOTO_WAIS		TRUE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to specify a port in 'g'oto commands
 * for telnet URLs.
 */
#define CAN_ANONYMOUS_GOTO_TELNET_PORT	FALSE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to jump to URLs (The 'J' command)
 * via the shortcut entries in your JUMPFILE.
 */
#define CAN_ANONYMOUS_JUMP	FALSE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to mail
 */
#define CAN_ANONYMOUS_MAIL	TRUE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to print
 */
#define CAN_ANONYMOUS_PRINT	FALSE

/*******************************
 * set to FALSE if users with anonymous restrictions should
 * not be able to view configuration file (lynx.cfg) info
 * via special LYNXCFG: links.  (This does not control access
 * to lynx.cfg as a normal file, e.g., through a "file:" URL,
 * if other restrictions allow that.)
 */
#define CAN_ANONYMOUS_VIEW_LYNXCFG_INFO			FALSE

/*******************************
 * set to FALSE if users with anonymous restrictions should
 * not be able to view extended configuration file (lynx.cfg)
 * info @@@ or perform special config info functions (reloading
 * at run-time) via special LYNXCFG: links @@@.  This only applies
 * if the lynxcfg_info" restriction controlled by the previous
 * item is not in effect and if Lynx has been compiled without
 * NO_CONFIG_INFO defined (--disable-config-info wasn't used
 * if Lynx was built with the autoconf configure script).
 * The extended info may include details on configuration file
 * names and location and links for reading the files, as well
 * as information on nesting of included configuration files.
 */
#define CAN_ANONYMOUS_VIEW_LYNXCFG_EXTENDED_INFO	FALSE

/*******************************
 * set to FALSE if users with anonymous restrictions should
 * not be able to view information on compile time configuration
 * via special LYNXCOMPILEOPTS: links.  This only applies
 * if the autoconf configure script was used to build Lynx
 * AND --disable-config-info wasn't used, otherwise this
 * special URL scheme isn't recognized anyway.
 */
#define CAN_ANONYMOUS_VIEW_COMPILEOPTS_INFO		FALSE

/*******************************
 * set to FALSE if you don't want users of your anonymous
 * account to be able to 'g'oto special URLs for showing
 * configuration info (LYNXCFG: and LYNXCOMPILEOPTS:) if
 * they are otherwise allowed.
 */
#define CAN_ANONYMOUS_GOTO_CONFIGINFO		FALSE

/*****************************
 * Be sure you have read about and set defines above in Sections
 * 1, 2 and 3 that could  affect Lynx in an anonymous account,
 * especially LOCAL_EXECUTION_LINKS_ALWAYS_OFF_FOR_ANONYMOUS.
 *
 * This ends the section specific to anonymous accounts.
 */

#endif /* USERDEFS_H */
