/*	$OpenBSD: pcnfsd.x,v 1.3 2003/02/15 12:19:27 deraadt Exp $	*/
/*	$NetBSD: pcnfsd.x,v 1.2 1995/07/25 22:20:33 gwr Exp $	*/

/*
 *=====================================================================
 * Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
 *	@(#)pcnfsd_cache.c	1.1	9/3/91
 *
 * pcnfsd is copyrighted software, but is freely licensed. This
 * means that you are free to redistribute it, modify it, ship it
 * in binary with your system, whatever, provided:
 *
 * - you leave the Sun copyright notice in the source code
 * - you make clear what changes you have introduced and do
 *   not represent them as being supported by Sun.
 *
 * If you make changes to this software, we ask that you do so in
 * a way which allows you to build either the "standard" version or
 * your custom version from a single source file. Test it, lint
 * it (it won't lint 100%, very little does, and there are bugs in
 * some versions of lint :-), and send it back to Sun via email
 * so that we can roll it into the source base and redistribute
 * it. We'll try to make sure your contributions are acknowledged
 * in the source, but after all these years it's getting hard to
 * remember who did what.
 *=====================================================================
 */

/* The maximum number of bytes in a user name argument */
const IDENTLEN = 32;
/*  The maximum number of bytes in a password argument  */
const PASSWORDLEN = 64;
/*  The maximum number of bytes in a print client name argument  */
const CLIENTLEN = 64;
/*  The maximum number of bytes in a printer name argument  */
const PRINTERNAMELEN = 64;
/*  The maximum number of bytes in a print user name argument  */
const USERNAMELEN = 64;
/*  The maximum number of bytes in a print spool file name argument  */
const SPOOLNAMELEN = 64;
/*  The maximum number of bytes in a print options argument  */
const OPTIONSLEN = 64;
/*  The maximum number of bytes in a print spool directory path  */
const SPOOLDIRLEN = 255;
/*   The maximum number of secondary GIDs returned by a V2 AUTH  */
const EXTRAGIDLEN = 16;
/*   The  maximum number of bytes in a home directory spec  */
const HOMEDIRLEN = 255;
/*   The maximum number of bytes in a misc. comments string */
const COMMENTLEN = 255;
/*   The maximum number of bytes in a print job id */
const PRINTJOBIDLEN = 255;
/*   The maximum number of printers returned by a LIST operation */
const PRLISTMAX = 32;
/*   The maximum number of print jobs returned by a QUEUE operation */
const PRQUEUEMAX = 128;
/*   The maximum number of entries in the facilities list */
const FACILITIESMAX = 32;
/*   The maximum length of an operator message */
const MESSAGELEN = 512;



typedef string ident<IDENTLEN>;
/*
** The type ident is used for passing an encoded user name for
** authentication. The server should decode the string by replacing each
** octet with the value formed by performing an exclusive-or of the octet
** value with the value 0x5b and and'ing the result with 0x7f.
*/

typedef string message<MESSAGELEN>;
/*
** The type message is used for passing an alert message to the
** system operator on the server. The text may include newlines.
*/

typedef string password<PASSWORDLEN>;
/*
** The type password is used for passing an encode password for
** authentication.  The server should decode the password as described
** above.
*/

typedef string client<CLIENTLEN>;
/*
** The type client is used for passing the hostname of a client for
** printing. The server may use this name in constructing the spool
** directory name.
*/

typedef string printername<PRINTERNAMELEN>;
/*
** The type printername is used for passing the name of a printer on which
** the client wishes to print.
*/

typedef string username<USERNAMELEN>;
/*
** The type username is used for passing the user name for a print job.
** The server may use this in any way it chooses: it may attempt to change
** the effective identity with which it is running to username or may
** simply arrange for the text to be printed on the banner page.
*/

typedef string comment<COMMENTLEN>;
/*
** The type comment is used to pass an uninterpreted text string which
** may be used by displayed to a human user or used for custom
** extensions to the PCNFSD service. If you elect to extend PCNFSD
** service in this way, please do so in a way which will avoid
** problems if your client attempts to interoperate with a server
** which does not support your extension. One way to do this is to
** use the 
*/

typedef string spoolname<SPOOLNAMELEN>;
/*
** The type spoolname is used for passing the name of a print spool file
** (a simple filename not a pathname) within the spool directory.
*/

typedef string printjobid<PRINTJOBIDLEN>;
/*
** The type printjobid is used for passing the id of a print job.
*/

typedef string homedir<OPTIONSLEN>;
/*
** The type homedir is used to return the home directory for the user.
** If present, it should be in the form "hostname:path", where hostname
** and path are in a suitable form for communicating with the mount server.
*/

typedef string options<OPTIONSLEN>;
/*
** The type options is used for passing implementation-specific print
** control information.  The option string is a set of printable ASCII
** characters.  The first character should be ignored by the server; it is
** reserved for client use. The second character specifies the type of
** data in the print file.  The following types are defined (an
** implementation may define additional values):
** 
**  p - PostScript data. The client will ensure that a valid
**      PostScript header is included.
**  d - Diablo 630 data.
**  x - Generic printable ASCII text. The client will have filtered
**      out all non-printable characters other than CR, LF, TAB,
**      BS and VT.
**  r - Raw print data. The client performs no filtering.
**  u - User-defined. Reserved for custom extensions. A vanilla
**      pcnfsd server will treat this as equivalent to "r"
** 
** If diablo data (type 'd') is specified, a formatting specification
** string will be appended. This has the form:
** 	ppnnnbbb
**         pp
** Pitch - 10, 12 or 15.
**           nnn
** The ``normal'' font to be used - encoded as follows:
**             Courier                    crn
**             Courier-Bold               crb
**             Courier-Oblique            con
**             Courier-BoldObliqu         cob
**             Helvetica                  hrn
**             Helvetica-Bold             hrb
**             Helvetica-Oblique          hon
**             Helvetica-BoldOblique      hob
**             Times-Roman                trn
**             Times-Bold                 trb
**             Times-Italic               ton
**             Times-BoldItalic           tob
**              bbb
** The ``bold'' font to be used - encoded in the same way.  For example,
** the string ``nd10hrbcob'' specifies that the print data is in Diablo
** 630 format, it should be printed at 10 pitch, ``normal'' text should be
** printed in Helvetica-Bold, and ``bold'' text should be printed in
** Courier-BoldOblique.
*/

enum arstat {
        AUTH_RES_OK = 0,
        AUTH_RES_FAKE = 1,
        AUTH_RES_FAIL = 2
};
/*
** The type arstat is returned by PCNFSD_AUTH. A value of AUTH_RES_OK
** indicates that the server was able to verify the ident and password
** successfully.AUTH_RES_FAIL is returned if a verification failure
** occurred. The value AUTH_RES_FAKE may be used if the server wishes to
** indicate that the verification failed, but that the server has
** synthesised acceptable values for uid and gid which the client may use
** if it wishes.
*/

enum alrstat {
        ALERT_RES_OK = 0,
        ALERT_RES_FAIL = 1
};
/*
** The type alrstat is returned by PCNFSD_ALERT. A value of ALERT_RES_OK
** indicates that the server was able to notify the system operator
** successfully. ALERT_RES_FAIL is returned if a failure occurred
*/
enum pirstat {
        PI_RES_OK = 0,
        PI_RES_NO_SUCH_PRINTER = 1,
        PI_RES_FAIL = 2
};
/*
** The type pirstat is returned by a number of print operations. PI_RES_OK
** indicates that the operation was performed successfully. PI_RES_FAIL
** indicates that the printer name was valid, but the operation could
** not be performed. PI_RES_NO_SUCH_PRINTER indicates that the printer
** name was not recognised.
*/

enum pcrstat {
        PC_RES_OK = 0,
        PC_RES_NO_SUCH_PRINTER = 1,
        PC_RES_NO_SUCH_JOB = 2,
        PC_RES_NOT_OWNER = 3,
        PC_RES_FAIL = 4
};
/*
** The type pcrstat is returned by a CANCEL, REQUEUE, HOLD, or RELEASE
** print operation.
** PC_RES_OK indicates that the operation was performed successfully.
** PC_RES_NO_SUCH_PRINTER indicates that the printer name was not recognised.
** PC_RES_NO_SUCH_JOB means that the job does not exist, or is not
** associated with the specified printer.
** PC_RES_NOT_OWNER means that the user does not have permission to
** manipulate the job.
** PC_RES_FAIL means that the job could not be manipulated for an unknown
** reason.
*/


enum psrstat {
        PS_RES_OK = 0,
        PS_RES_ALREADY = 1,
        PS_RES_NULL = 2,
        PS_RES_NO_FILE = 3,
        PS_RES_FAIL = 4
};
/*
** The type psrstat is returned by PCNFSD_PR_START. A value of PS_RES_OK
** indicates that the server has started printing the job. It is possible
** that the reply from a PCNFSD_PR_START call may be lost, in which case
** the client will repeat the call. If the spool file is still in
** existence, the server will return PS_RES_ALREADY indicating that it has
** already started printing. If the file cannot be found, PS_RES_NO_FILE
** is returned.  PS_RES_NULL indicates that the spool file was empty,
** while PS_RES_FAIL denotes a general failure.  PI_RES_FAIL is returned
** if spool directory could not be created. The value
** PI_RES_NO_SUCH_PRINTER indicates that the printer name was not
** recognised.
*/

enum mapreq {
        MAP_REQ_UID = 0,
        MAP_REQ_GID = 1,
        MAP_REQ_UNAME = 2,
        MAP_REQ_GNAME = 3
};
/*
** The type mapreq identifies the type of a mapping request.
** MAP_REQ_UID requests that the server treat the value in the
** id field as a uid and return the corresponding username in name.
** MAP_REQ_GID requests that the server treat the value in the
** id field as a gid and return the corresponding groupname in name.
** MAP_REQ_UNAME requests that the server treat the value in the
** name field as a username and return the corresponding uid in id.
** MAP_REQ_GNAME requests that the server treat the value in the
** name field as a groupname and return the corresponding gid in id.
*/

enum maprstat {
        MAP_RES_OK = 0,
        MAP_RES_UNKNOWN = 1,
        MAP_RES_DENIED = 2
};
/*
** The type maprstat indicates the success or failure of
** an individual mapping request.
*/

/* 
**********************************************************
** Version 1 of the PCNFSD protocol.
**********************************************************
*/
struct auth_args {
        ident           id;
        password        pw;
};
struct auth_results {
        arstat          stat;
        unsigned int    uid;
        unsigned int    gid;
};

struct pr_init_args {
        client          system;
        printername     pn;
};
struct pr_init_results {
        pirstat         stat;
        spoolname       dir;
};

struct pr_start_args {
        client          system;
        printername     pn;
        username        user;
        spoolname       file;
        options         opts;
};
struct pr_start_results {
        psrstat         stat;
};


/* 
**********************************************************
** Version 2 of the PCNFSD protocol.
**********************************************************
*/

struct v2_info_args {
        comment         vers;
        comment         cm;
};

struct v2_info_results {
        comment         vers;
        comment         cm;
	int             facilities<FACILITIESMAX>;
};

struct v2_pr_init_args {
        client          system;
        printername     pn;
        comment         cm;
};
struct v2_pr_init_results {
        pirstat         stat;
        spoolname       dir;
        comment         cm;
};
 
struct v2_pr_start_args {
        client          system;
        printername     pn;
        username        user;
        spoolname       file;
        options         opts;
	int             copies;
        comment         cm;
};
struct v2_pr_start_results {
        psrstat         stat;
        printjobid      id;
        comment         cm;
};



typedef struct pr_list_item *pr_list;

struct pr_list_item {
        printername    pn;
        printername    device;
        client         remhost; /* empty if local */
        comment        cm;
        pr_list        pr_next;
};

struct v2_pr_list_results {
        comment        cm;
        pr_list        printers;
};
 
struct v2_pr_queue_args {
        printername     pn;
        client          system; 
        username        user;
        bool            just_mine;
        comment         cm;
};

typedef struct pr_queue_item *pr_queue;

struct pr_queue_item {
        int            position;
        printjobid     id;
        comment        size;
        comment        status;
        client         system;
        username       user;
        spoolname      file;
        comment        cm;
        pr_queue       pr_next;
};
 
struct v2_pr_queue_results {
        pirstat        stat;
        comment        cm;
        bool           just_yours;
        int            qlen;
        int            qshown;
        pr_queue       jobs;
};
 

struct v2_pr_cancel_args {
        printername     pn;
        client          system;
        username        user;
        printjobid      id; 
        comment         cm;
};
struct v2_pr_cancel_results {
        pcrstat        stat;
        comment        cm;
};
 

struct v2_pr_status_args { 
        printername     pn; 
        comment         cm; 
}; 
struct v2_pr_status_results { 
        pirstat        stat;
        bool           avail;
        bool           printing;
	int            qlen;
        bool           needs_operator;
	comment        status;
        comment        cm; 
}; 
  
struct v2_pr_admin_args {
        client          system;
        username        user;
        printername     pn;
        comment         cm;
};
struct v2_pr_admin_results {
        pirstat         stat;
        comment         cm;
};

struct v2_pr_requeue_args {
        printername     pn;
        client          system;
        username        user;
        printjobid      id;
	int             qpos;
        comment         cm;
};

struct v2_pr_requeue_results {
        pcrstat        stat;
        comment        cm;
};

struct v2_pr_hold_args {
        printername     pn;
        client          system;
        username        user;
        printjobid      id; 
        comment         cm;
};
struct v2_pr_hold_results {
        pcrstat        stat;
        comment        cm;
};
 
struct v2_pr_release_args {
        printername     pn;
        client          system;
        username        user;
        printjobid      id; 
        comment         cm;
};
struct v2_pr_release_results {
        pcrstat        stat;
        comment        cm;
};
 

typedef struct mapreq_arg_item *mapreq_arg;

struct mapreq_arg_item {
	mapreq           req;
        int              id;
        username         name;
        mapreq_arg       mapreq_next;
};

typedef struct mapreq_res_item *mapreq_res;

struct mapreq_res_item {
	mapreq           req;
	maprstat         stat;
        int              id;
        username         name;
        mapreq_res       mapreq_next;
};

struct v2_mapid_args {
        comment         cm;
        mapreq_arg      req_list;
};


struct v2_mapid_results {
        comment         cm;
        mapreq_res      res_list;
};
 
struct v2_auth_args {
        client          system;
        ident           id;
        password        pw;
        comment         cm;
};
struct v2_auth_results {
        arstat          stat;
        unsigned int    uid;
        unsigned int    gid;
        unsigned int    gids<EXTRAGIDLEN>;
        homedir         home;
        int             def_umask;
        comment         cm;
};

struct v2_alert_args {
        client          system;
        printername     pn;
        username        user;
        message         msg;
};
struct v2_alert_results {
        alrstat          stat;
        comment         cm;
};
 

/* 
**********************************************************
** Protocol description for the PCNFSD program
**********************************************************
*/
/* 
** Version 1 of the PCNFSD protocol.
**
** -- PCNFSD_NULL() = 0
**	Null procedure - standard for all RPC programs.
**
** -- PCNFSD_AUTH() = 1
**	Perform user authentication - map username, password into uid, gid.
**
** -- PCNFSD_PR_INIT() = 2
**	Prepare for remote printing: identify exporting spool directory.
**
** -- PCNFSD_PR_START() = 3
**	Submit a spooled print job for printing: the print data is
**	in a file created in the spool directory.
**
** Version 2 of the -- PCNFSD protocol.
**
** -- PCNFSD2_NULL() = 0
**	Null procedure - standard for all RPC programs.
**
** -- PCNFSD2_INFO() = 1
**	Determine which services are supported by this implementation
**	of PCNFSD.
**
** -- PCNFSD2_PR_INIT() = 2
**	 Prepare for remote printing: identify exporting spool directory.
**
** -- PCNFSD2_PR_START() = 3
**	Submit a spooled print job for printing: the print data is
**      in a file created in the spool directory.
**
** -- PCNFSD2_PR_LIST() = 4
**	List all printers known on the server.
**
** -- PCNFSD2_PR_QUEUE() = 5
**	List all or part of the queued jobs for a printer.
**
** -- PCNFSD2_PR_STATUS() = 6
**	Determine the status of a printer.
**	
** -- PCNFSD2_PR_CANCEL() = 7
**	Cancel a print job.
**	
** -- PCNFSD2_PR_ADMIN() = 8
**	Perform an implementation-dependent printer administration
**	operation.
**	
** -- PCNFSD2_PR_REQUEUE() = 9
**	Change the queue position of a previously-submitted print job.
**	
** -- PCNFSD2_PR_HOLD() = 10
**	Place a "hold" on a previously-submitted print job. The job
**	will remain in the queue, but will not be printed.
**	
** -- PCNFSD2_PR_RELEASE() = 11
**	Release the "hold" on a previously-held print job.
**	
** -- PCNFSD2_MAPID() = 12
**	Perform one or more translations between user and group
**	names and IDs.
**	
** -- PCNFSD2_AUTH() = 13
**	Perform user authentication - map username, password into uid, gid;
**	may also return secondary gids, home directory, umask.
**	
** -- PCNFSD2_ALERT() = 14
**	Send a message to the system operator.
*/
program PCNFSDPROG {
        version PCNFSDVERS {
                void             PCNFSD_NULL(void) = 0;
                auth_results     PCNFSD_AUTH(auth_args) = 1;
                pr_init_results  PCNFSD_PR_INIT(pr_init_args) = 2;
                pr_start_results PCNFSD_PR_START(pr_start_args) = 3;
        } = 1;
/*
** Version 2 of the PCNFSD protocol.
*/
        version PCNFSDV2 {
                void                   PCNFSD2_NULL(void) = 0;
                v2_info_results        PCNFSD2_INFO(v2_info_args) = 1;
                v2_pr_init_results     PCNFSD2_PR_INIT(v2_pr_init_args) = 2;
                v2_pr_start_results    PCNFSD2_PR_START(v2_pr_start_args) = 3;
                v2_pr_list_results     PCNFSD2_PR_LIST(void) = 4;
                v2_pr_queue_results    PCNFSD2_PR_QUEUE(v2_pr_queue_args) = 5;
                v2_pr_status_results   PCNFSD2_PR_STATUS(v2_pr_status_args) = 6;
                v2_pr_cancel_results   PCNFSD2_PR_CANCEL(v2_pr_cancel_args) = 7;
                v2_pr_admin_results    PCNFSD2_PR_ADMIN(v2_pr_admin_args) = 8;
                v2_pr_requeue_results  PCNFSD2_PR_REQUEUE(v2_pr_requeue_args) = 9;
                v2_pr_hold_results     PCNFSD2_PR_HOLD(v2_pr_hold_args) = 10;
                v2_pr_release_results  PCNFSD2_PR_RELEASE(v2_pr_release_args) = 11;
                v2_mapid_results       PCNFSD2_MAPID(v2_mapid_args) = 12;
                v2_auth_results        PCNFSD2_AUTH(v2_auth_args) = 13;
                v2_alert_results       PCNFSD2_ALERT(v2_alert_args) = 14;
        } = 2;

} = 150001;

/*
** The following forces a publically-visible msg_out()
*/
%#if RPC_SVC
% static void _msgout();
% void msg_out(msg) char *msg; {_msgout(msg);}
%#endif
%#if RPC_HDR
% extern void msg_out();
%#endif
