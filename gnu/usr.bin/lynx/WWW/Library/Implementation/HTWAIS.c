/*	WorldWideWeb - Wide Area Informaion Server Access	HTWAIS.c
 *	==================================================
 *
 *	This module allows a WWW server or client to read data from a
 *	remote	WAIS
 *  server, and provide that data to a WWW client in hypertext form.
 *  Source files, once retrieved, are stored and used to provide
 *  information about the index when that is acessed.
 *
 *  Authors
 *	BK	Brewster Kahle, Thinking Machines, <Brewster@think.com>
 *	TBL	Tim Berners-Lee, CERN <timbl@info.cern.ch>
 *	FM	Foteos Macrides, WFEB <macrides@sci.wfeb.edu>
 *
 *  History
 *	   Sep 91	TBL adapted shell-ui.c (BK) with HTRetrieve.c from WWW.
 *	   Feb 91	TBL Generated HTML cleaned up a bit (quotes, escaping)
 *			    Refers to lists of sources.
 *	   Mar 93	TBL Lib 2.0 compatible module made.
 *	   May 94	FM  Added DIRECT_WAIS support for VMS.
 *
 *  Bugs
 *	Uses C stream i/o to read and write sockets, which won't work
 *	on VMS TCP systems.
 *
 *	Should cache connections.
 *
 *	ANSI C only as written
 *
 *  Bugs fixed
 *	NT Nathan Torkington (Nathan.Torkington@vuw.ac.nz)
 *
 *  WAIS comments:
 *
 *	1.	Separate directories for different system's .o would help
 *	2.	Document ids are rather long!
 *
 * W WW Address mapping convention:
 *
 *	/servername/database/type/length/document-id
 *
 *	/servername/database?word+word+word
 */
/* WIDE AREA INFORMATION SERVER SOFTWARE:
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   Brewster@think.com
*/

#include <HTUtils.h>
#include <HTParse.h>
#include <HTAccess.h>		/* We implement a protocol */
#include <HTML.h>		/* The object we will generate */
#include <HTWSRC.h>
#include <HTTCP.h>
#include <HTCJK.h>
#include <HTAlert.h>

/*			From WAIS
 *			---------
 */
#ifdef VMS
#include <HTVMS_WaisUI.h>
#include <HTVMS_WaisProt.h>
#else
#include <ui.h>
#endif /* VMS */

#define MAX_MESSAGE_LEN 100000
#define CHARS_PER_PAGE 10000	/* number of chars retrieved in each request */

#define WAISSEARCH_DATE "Fri Jul 19 1991"

/*			FROM WWW
 *			--------
 */
#include <LYUtils.h>
#include <LYLeaks.h>

#define DIRECTORY "/cnidr.org:210/directory-of-servers"
/* #define DIRECTORY "/quake.think.com:210/directory-of-servers" */

#define BIG 1024		/* identifier size limit  @@@@@ */

#define BUFFER_SIZE 4096	/* Arbitrary size for efficiency */

#define HEX_ESCAPE '%'

static BOOL as_gate;		/* Client is using us as gateway */

static char line[2048];		/* For building strings to display */

				/* Must be able to take id */

#define PUTC(c) (*target->isa->put_character)(target, c)
#define PUTS(s) (*target->isa->put_string)(target, s)
#define START(e) (*target->isa->start_element)(target, e, 0, 0, -1, 0)
#define END(e) (*target->isa->end_element)(target, e, 0)
#define MAYBE_END(e) if (HTML_dtd.tags[e].contents != SGML_EMPTY) \
			(*target->isa->end_element)(target, e, 0)
#define FREE_TARGET (*target->isa->_free)(target)

struct _HTStructured {
    const HTStructuredClass *isa;
    /* ... */
};

struct _HTStream {
    const HTStreamClass *isa;
    /* ... */
};

/* ------------------------------------------------------------------------ */
/* ---------------- Local copy of connect_to_server calls ----------------- */
/* ------------------------------------------------------------------------ */
/* Returns 1 on success, 0 on fail, -1 on interrupt. */
static int fd_mosaic_connect_to_server(char *host_name,
				       long port,
				       long *fd)
{
    char *dummy = NULL;
    int status;
    int result;

    HTSprintf0(&dummy, "%s//%s:%d/", STR_WAIS_URL, host_name, port);

    status = HTDoConnect(dummy, "WAIS", 210, (int *) fd);
    if (status == HT_INTERRUPTED) {
	result = -1;
    } else if (status < 0) {
	result = 0;
    } else {
	result = 1;
    }
    FREE(dummy);
    return result;
}

/* Returns 1 on success, 0 on fail, -1 on interrupt. */
#ifdef VMS
static int mosaic_connect_to_server(char *host_name,
				    long port,
				    long *fdp)
#else
static int mosaic_connect_to_server(char *host_name,
				    long port,
				    FILE **fp)
#endif				/* VMS */
{
#ifndef VMS
    FILE *file;
#endif /* VMS */
    long fd;
    int rv;

    rv = fd_mosaic_connect_to_server(host_name, port, &fd);
    if (rv == 0) {
	HTAlert(gettext("Could not connect to WAIS server."));
	return 0;
    } else if (rv == -1) {
	HTAlert(CONNECTION_INTERRUPTED);
	return -1;
    }
#ifndef VMS
    if ((file = fdopen(fd, "r+")) == NULL) {
	HTAlert(gettext("Could not open WAIS connection for reading."));
	return 0;
    }

    *fp = file;
#else
    *fdp = fd;
#endif /* VMS */
    return 1;
}
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

/*								showDiags
*/
/* modified from Jonny G's version in ui/question.c */
static void showDiags(HTStream *target, diagnosticRecord ** d)
{
    long i;

    for (i = 0; d[i] != NULL; i++) {
	if (d[i]->ADDINFO != NULL) {
	    PUTS(gettext("Diagnostic code is "));
	    PUTS(d[i]->DIAG);
	    PUTC(' ');
	    PUTS(d[i]->ADDINFO);
	    PUTC('\n');
	}
    }
}

/*	Matrix of allowed characters in filenames
 *	-----------------------------------------
 */

static BOOL acceptable[256];
static BOOL acceptable_inited = NO;

static void init_acceptable(void)
{
    unsigned int i;
    char *good =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./-_$";

    for (i = 0; i < 256; i++)
	acceptable[i] = NO;
    for (; *good; good++)
	acceptable[(unsigned int) *good] = YES;
    acceptable_inited = YES;
}

/*	Transform file identifier into WWW address
 *	------------------------------------------
 *
 *
 * On exit,
 *	returns		nil if error
 *			pointer to malloced string (must be freed) if ok
 */
static char *WWW_from_archie(char *file)
{
    char *end;
    char *result;
    char *colon;

    for (end = file; *end > ' '; end++) ;	/* assumes ASCII encoding */
    result = (char *) malloc(10 + (end - file));
    if (!result)
	return result;		/* Malloc error */
    strcpy(result, "file://");
    strncat(result, file, end - file);
    colon = strchr(result + 7, ':');	/* Expect colon after host */
    if (colon) {
	for (; colon[0]; colon[0] = colon[1], colon++) ;	/* move down */
    }
    return result;
}				/* WWW_from_archie */

/*	Transform document identifier into URL
 *	--------------------------------------
 *
 *  Bugs: A static buffer of finite size is used!
 *	The format of the docid MUST be good!
 *
 *  On exit,
 *	returns		nil if error
 *			pointer to malloced string (must be freed) if ok
 */
static char hex[17] = "0123456789ABCDEF";

static char *WWW_from_WAIS(any *docid)
{
    static char buf[BIG];
    char *q = buf;
    char *p = (docid->bytes);
    char *result = NULL;
    int i, l;

    if (TRACE) {
	char *p;

	fprintf(tfp, "WAIS id (%d bytes) is ", (int) docid->size);
	for (p = docid->bytes; p < docid->bytes + docid->size; p++) {
	    if ((*p >= ' ') && (*p <= '~'))	/* Assume ASCII! */
		fprintf(tfp, "%c", *p);
	    else
		fprintf(tfp, "<%x>", (unsigned) *p);
	}
	fprintf(tfp, "\n");
    }
    for (p = docid->bytes;
	 (p < docid->bytes + docid->size) && (q < &buf[BIG]);) {
	CTRACE((tfp, "    Record type %d, length %d\n", p[0], p[1]));
	if (*p > 10) {
	    CTRACE((tfp, "Eh?  DOCID record type of %d!\n", *p));
	    return 0;
	} {			/* Bug fix -- allow any byte value 15 Apr 93 */
	    unsigned int i = (unsigned) *p++;

	    if (i > 99) {
		*q++ = (i / 100) + '0';
		i = i % 100;
	    }
	    if (i > 9) {
		*q++ = (i / 10) + '0';
		i = i % 10;
	    }
	    *q++ = i + '0';	/* Record type */
	}
	*q++ = '=';		/* Separate */
	l = *p++;		/* Length */
	for (i = 0; i < l; i++, p++) {
	    if (!acceptable[*p]) {
		*q++ = HEX_ESCAPE;	/* Means hex coming */
		*q++ = hex[(*p) >> 4];
		*q++ = hex[(*p) & 15];
	    } else
		*q++ = *p;
	}
	*q++ = ';';		/* Terminate field */
    }
    *q++ = 0;			/* Terminate string */
    CTRACE((tfp, "WWW form of id: %s\n", buf));
    StrAllocCopy(result, buf);
    return result;
}				/* WWW_from_WAIS */

/*	Transform URL into WAIS document identifier
 *	-------------------------------------------
 *
 *  On entry,
 *	docname		points to valid name produced originally by
 *			WWW_from_WAIS
 *  On exit,
 *	docid->size	is valid
 *	docid->bytes	is malloced and must later be freed.
 */
static any *WAIS_from_WWW(any *docid, char *docname)
{
    char *z;			/* Output pointer */
    char *sor;			/* Start of record - points to size field. */
    char *p;			/* Input pointer */
    char *q;			/* Poisition of "=" */
    char *s;			/* Position of semicolon */
    int n;			/* size */

    CTRACE((tfp, "WWW id (to become WAIS id): %s\n", docname));
    for (n = 0, p = docname; *p; p++) {		/* Count sizes of strings */
	n++;
	if (*p == ';')
	    n--;		/* Not converted */
	else if (*p == HEX_ESCAPE)
	    n = n - 2;		/* Save two bytes */
	docid->size = n;
    }

    if (!(docid->bytes = (char *) malloc(docid->size)))		/* result record */
	outofmem(__FILE__, "WAIS_from_WWW");
    z = docid->bytes;

    for (p = docname; *p;) {	/* Convert of strings */
	/* Record type */

	*z = 0;			/* Initialize record type */
	while (*p >= '0' && *p <= '9') {
	    *z = *z * 10 + (*p++ - '0');	/* Decode decimal record type */
	}
	z++;
	if (*p != '=')
	    return 0;
	q = p;

/*	  *z++ = *p++ - '0';
	q = strchr(p , '=');
	if (!q) return 0;
*/
	s = strchr(q, ';');	/* (Check only) */
	if (!s)
	    return 0;		/* Bad! No ';'; */
	sor = z;		/* Remember where the size field was */
	z++;			/* Skip record size for now     */
	for (p = q + 1; *p != ';';) {
	    if (*p == HEX_ESCAPE) {
		char c;
		unsigned int b;

		p++;
		c = *p++;
		b = from_hex(c);
		c = *p++;
		if (!c)
		    break;	/* Odd number of chars! */
		*z++ = (b << 4) + from_hex(c);
	    } else {
		*z++ = *p++;	/* Record */
	    }
	}
	*sor = (z - sor - 1);	/* Fill in size -- not counting size itself */
	p++;			/* After semicolon: start of next record */
    }

    if (TRACE) {
	char *p;

	fprintf(tfp, "WAIS id (%d bytes) is ", (int) docid->size);
	for (p = docid->bytes; p < docid->bytes + docid->size; p++) {
	    if ((*p >= ' ') && (*p <= '~'))	/* Assume ASCII! */
		fprintf(tfp, "%c", *p);
	    else
		fprintf(tfp, "<%x>", (unsigned) *p);
	}
	fprintf(tfp, "\n");
    }
    return docid;		/* Ok */

}				/* WAIS_from_WWW */

/*	Send a plain text record to the client		output_text_record()
 *	--------------------------------------
 */
static void output_text_record(HTStream *target,
			       WAISDocumentText *record,
			       boolean quote_string_quotes,
			       boolean binary)
{
    long count;

    /* printf(" Text\n");
       print_any("     DocumentID:  ", record->DocumentID);
       printf("     VersionNumber:  %d\n", record->VersionNumber);
     */

    if (binary) {
	(*target->isa->put_block) (target,
				   record->DocumentText->bytes,
				   record->DocumentText->size);
	return;
    }

    for (count = 0; count < record->DocumentText->size; count++) {
	long ch = (unsigned char) record->DocumentText->bytes[count];

	if (ch == 27) {		/* What is this in for?  Tim */
	    /* then we have an escape code */
	    /* if the next letter is '(' or ')', then ignore two letters */
	    if ('(' == record->DocumentText->bytes[count + 1] ||
		')' == record->DocumentText->bytes[count + 1])
		count += 1;	/* it is a term marker */
	    else
		count += 4;	/* it is a paragraph marker */
	} else if (ch == '\n' || ch == '\r') {
	    PUTC('\n');
	} else if (HTCJK != NOCJK || ch == '\t' || isprint(ch)) {
	    PUTC(ch);
	}
    }
}				/* output text record */

/*	Format A Search response for the client		display_search_response
 *	---------------------------------------
 */
/* modified from tracy shen's version in wutil.c
 * displays either a text record or a set of headlines.
 */
static void display_search_response(HTStructured * target, SearchResponseAPDU *response,
				    char *database,
				    char *keywords)
{
    WAISSearchResponse *info;
    long i, k;

    BOOL archie = strstr(database, "archie") != 0;	/* Special handling */

    CTRACE((tfp, "HTWAIS: Displaying search response\n"));
    PUTS(gettext("Index "));
    START(HTML_EM);
    PUTS(database);
    END(HTML_EM);
    sprintf(line, gettext(" contains the following %d item%s relevant to \""),
	    (int) (response->NumberOfRecordsReturned),
	    response->NumberOfRecordsReturned == 1 ? "" : "s");
    PUTS(line);
    START(HTML_EM);
    PUTS(keywords);
    END(HTML_EM);
    PUTS("\".\n");
    PUTS(gettext("The first figure after each entry is its relative score, "));
    PUTS(gettext("the second is the number of lines in the item."));
    START(HTML_BR);
    START(HTML_BR);
    PUTC('\n');
    START(HTML_OL);

    if (response->DatabaseDiagnosticRecords != 0) {
	info = (WAISSearchResponse *) response->DatabaseDiagnosticRecords;
	i = 0;

	if (info->Diagnostics != NULL)
	    showDiags((HTStream *) target, info->Diagnostics);

	if (info->DocHeaders != 0) {
	    for (k = 0; info->DocHeaders[k] != 0; k++) {
		WAISDocumentHeader *head = info->DocHeaders[k];
		char *headline = trim_junk(head->Headline);
		any *docid = head->DocumentID;
		char *docname;	/* printable version of docid */

		i++;
		/*
		 * Make a printable string out of the document id.
		 */
		CTRACE((tfp, "HTWAIS:  %2ld: Score: %4ld, lines:%4ld '%s'\n",
			i,
			(long int) (info->DocHeaders[k]->Score),
			(long int) (info->DocHeaders[k]->Lines),
			headline));

		START(HTML_LI);

		if (archie) {
		    char *www_name = WWW_from_archie(headline);

		    if (www_name) {
			HTStartAnchor(target, NULL, www_name);
			PUTS(headline);
			END(HTML_A);
			FREE(www_name);
		    } else {
			PUTS(headline);
			PUTS(gettext(" (bad file name)"));
		    }
		} else {	/* Not archie */
		    docname = WWW_from_WAIS(docid);
		    if (docname) {
			if ((head->Types) &&
			    (!strcmp(head->Types[0], "URL"))) {
			    HTStartAnchor(target, NULL, headline);
			} else {
			    char *dbname = HTEscape(database, URL_XPALPHAS);
			    char *w3_address = NULL;

			    HTSprintf0(&w3_address,
				       "/%s/%s/%d/%s",
				       dbname,
				       head->Types ? head->Types[0] : "TEXT",
				       (int) (head->DocumentLength),
				       docname);
			    HTStartAnchor(target, NULL, w3_address);
			    FREE(w3_address);
			    FREE(dbname);
			}
			PUTS(headline);
			END(HTML_A);
			FREE(docname);
		    } else {
			PUTS(gettext("(bad doc id)"));
		    }
		}

		sprintf(line, "%5ld  %5ld  ",
			head->Score,
			head->Lines);
		PUTS(line);
		MAYBE_END(HTML_LI);
	    }			/* next document header */
	}
	/* if there were any document headers */
	if (info->ShortHeaders != 0) {
	    k = 0;
	    while (info->ShortHeaders[k] != 0) {
		i++;
		PUTS(gettext("(Short Header record, can't display)"));
	    }
	}
	if (info->LongHeaders != 0) {
	    k = 0;
	    while (info->LongHeaders[k] != 0) {
		i++;
		PUTS(gettext("\nLong Header record, can't display\n"));
	    }
	}
	if (info->Text != 0) {
	    k = 0;
	    while (info->Text[k] != 0) {
		i++;
		PUTS(gettext("\nText record\n"));
		output_text_record((HTStream *) target,
				   info->Text[k++], false, false);
	    }
	}
	if (info->Headlines != 0) {
	    k = 0;
	    while (info->Headlines[k] != 0) {
		i++;
		PUTS(gettext("\nHeadline record, can't display\n"));
		/* dsply_headline_record( info->Headlines[k++]); */
	    }
	}
	if (info->Codes != 0) {
	    k = 0;
	    while (info->Codes[k] != 0) {
		i++;
		PUTS(gettext("\nCode record, can't display\n"));
		/* dsply_code_record( info->Codes[k++]); */
	    }
	}
    }				/* Loop: display user info */
    END(HTML_OL);
    PUTC('\n');
}

/*		Load by name					HTLoadWAIS
 *		============
 *
 *  This renders any object or search as required.
 */
int HTLoadWAIS(const char *arg,
	       HTParentAnchor *anAnchor,
	       HTFormat format_out,
	       HTStream *sink)
#define MAX_KEYWORDS_LENGTH 1000
#define MAX_SERVER_LENGTH 1000
#define MAX_DATABASE_LENGTH 1000
#define MAX_SERVICE_LENGTH 1000
#define MAXDOCS 200

{
    char *key;			/* pointer to keywords in URL */
    char *request_message = NULL;	/* arbitrary message limit */
    char *response_message = NULL;	/* arbitrary message limit */
    long request_buffer_length;	/* how of the request is left */
    SearchResponseAPDU *retrieval_response = 0;
    char keywords[MAX_KEYWORDS_LENGTH + 1];
    char *server_name;
    char *wais_database = NULL;	/* name of current database */
    char *www_database;		/* Same name escaped */
    char *service;
    char *doctype;
    char *doclength;
    long document_length;
    char *docname;

#ifdef VMS
    long connection = 0;

#else
    FILE *connection = NULL;
#endif /* VMS */
    char *names;		/* Copy of arg to be hacked up */
    BOOL ok = NO;
    int return_status = HT_LOADED;
    int rv;

    if (!acceptable_inited)
	init_acceptable();

    /* Decipher and check syntax of WWW address:
     * ----------------------------------------
     *
     * First we remove the "wais:" if it was specified.  920110
     */
    names = HTParse(arg, "", PARSE_HOST | PARSE_PATH | PARSE_PUNCTUATION);
    key = strchr(names, '?');

    if (key) {
	char *p;

	*key++ = 0;		/* Split off keywords */
	for (p = key; *p; p++)
	    if (*p == '+')
		*p = ' ';
	HTUnEscape(key);
    }
    if (names[0] == '/') {
	server_name = names + 1;
	if ((as_gate = (*server_name == '/')) != 0)
	    server_name++;	/* Accept one or two */
	www_database = strchr(server_name, '/');
	if (www_database) {
	    *www_database++ = 0;	/* Separate database name */
	    doctype = strchr(www_database, '/');
	    if (key)
		ok = YES;	/* Don't need doc details */
	    else if (doctype) {	/* If not search parse doc details */
		*doctype++ = 0;	/* Separate rest of doc address */
		doclength = strchr(doctype, '/');
		if (doclength) {
		    *doclength++ = 0;
		    document_length = atol(doclength);
		    if (document_length) {
			docname = strchr(doclength, '/');
			if (docname) {
			    *docname++ = 0;
			    ok = YES;	/* To avoid a goto! */
			}	/* if docname */
		    }		/* if document_length valid */
		}		/* if doclength */
	    } else {		/* no doctype?  Assume index required */
		if (!key)
		    key = "";
		ok = YES;
	    }			/* if doctype */
	}			/* if database */
    }

    if (!ok)
	return HTLoadError(sink, 500, gettext("Syntax error in WAIS URL"));

    CTRACE((tfp, "HTWAIS: Parsed OK\n"));

    service = strchr(names, ':');
    if (service)
	*service++ = 0;
    else
	service = "210";

    if (server_name[0] == 0) {
#ifdef VMS
	connection = 0;
#else
	connection = NULL;
#endif /* VMS */

    } else if (!(key && !*key)) {
	int status;

	CTRACE((tfp, "===WAIS=== calling mosaic_connect_to_server\n"));
	status = mosaic_connect_to_server(server_name,
					  atoi(service),
					  &connection);
	if (status == 0) {
	    CTRACE((tfp, "===WAIS=== connection failed\n"));
	    FREE(names);
	    return HT_NOT_LOADED;
	} else if (status == -1) {
	    CTRACE((tfp, "===WAIS=== connection interrupted\n"));
	    FREE(names);
	    return HT_NOT_LOADED;
	}
    }

    StrAllocCopy(wais_database, www_database);
    HTUnEscape(wais_database);

    /*
     * This below fixed size stuff is terrible.
     */
#ifdef VMS
    if ((request_message = typecallocn(char, MAX_MESSAGE_LEN)) == 0)
	  outofmem(__FILE__, "HTLoadWAIS");
    if ((response_message = typecallocn(char, MAX_MESSAGE_LEN)) == 0)
	  outofmem(__FILE__, "HTLoadWAIS");

#else
    request_message = (char *) s_malloc((size_t) MAX_MESSAGE_LEN * sizeof(char));
    response_message = (char *) s_malloc((size_t) MAX_MESSAGE_LEN * sizeof(char));
#endif /* VMS */

    /*
     * If keyword search is performed but there are no keywords, the user has
     * followed a link to the index itself.  It would be appropriate at this
     * point to send him the .SRC file - how?
     */
    if (key && !*key) {		/* I N D E X */
#ifdef CACHE_FILE_PREFIX
	char *filename = NULL;
	FILE *fp;
#endif
	HTStructured *target = HTML_new(anAnchor, format_out, sink);

	START(HTML_HEAD);
	PUTC('\n');
	HTStartIsIndex(target, HTWAIS_SOLICIT_QUERY, NULL);
	PUTC('\n');

	{
	    START(HTML_TITLE);
	    PUTS(wais_database);
	    PUTS(gettext(" (WAIS Index)"));
	    END(HTML_TITLE);
	    PUTC('\n');
	    END(HTML_HEAD);
	    PUTC('\n');

	    START(HTML_H1);
	    PUTS(gettext("WAIS Index: "));
	    START(HTML_EM);
	    PUTS(wais_database);
	    END(HTML_EM);
	    END(HTML_H1);
	    PUTC('\n');
	    PUTS(gettext("This is a link for searching the "));
	    START(HTML_EM);
	    PUTS(wais_database);
	    END(HTML_EM);
	    PUTS(gettext(" WAIS Index.\n"));

	}
	/*
	 * If we have seen a source file for this database, use that.
	 */
#ifdef CACHE_FILE_PREFIX
	HTSprintf0(&filename, "%sWSRC-%s:%s:%.100s.txt",
		   CACHE_FILE_PREFIX,
		   server_name, service, www_database);

	fp = fopen(filename, "r");	/* Have we found this already? */
	CTRACE((tfp, "HTWAIS: Description of server %s %s.\n",
		filename,
		fp ? "exists already" : "does NOT exist!"));

	if (fp) {
	    char c;

	    START(HTML_PRE);	/* Preformatted description */
	    PUTC('\n');
	    while ((c = getc(fp)) != EOF)
		PUTC(c);	/* Transfer file */
	    END(HTML_PRE);
	    fclose(fp);
	}
	FREE(filename);
#endif
	START(HTML_P);
	PUTS(gettext("\nEnter the 's'earch command and then specify search words.\n"));

	FREE_TARGET;
    } else if (key) {		/* S E A R C H */
	char *p;
	HTStructured *target;

	strncpy(keywords, key, MAX_KEYWORDS_LENGTH);
	while ((p = strchr(keywords, '+')) != 0)
	    *p = ' ';

	/*
	 * Send advance title to get something fast to the other end.
	 */
	target = HTML_new(anAnchor, format_out, sink);

	START(HTML_HEAD);
	PUTC('\n');
	HTStartIsIndex(target, HTWAIS_SOLICIT_QUERY, NULL);
	PUTC('\n');
	START(HTML_TITLE);
	PUTS(keywords);
	PUTS(gettext(" (in "));
	PUTS(wais_database);
	PUTC(')');
	END(HTML_TITLE);
	PUTC('\n');
	END(HTML_HEAD);
	PUTC('\n');

	START(HTML_H1);
	PUTS(gettext("WAIS Search of \""));
	START(HTML_EM);
	PUTS(keywords);
	END(HTML_EM);
	PUTS(gettext("\" in: "));
	START(HTML_EM);
	PUTS(wais_database);
	END(HTML_EM);
	END(HTML_H1);
	PUTC('\n');

	request_buffer_length = MAX_MESSAGE_LEN;	/* Amount left */
	CTRACE((tfp, "HTWAIS: Search for `%s' in `%s'\n",
		keywords, wais_database));
	if (NULL ==
	    generate_search_apdu(request_message + HEADER_LENGTH,
				 &request_buffer_length,
				 keywords, wais_database, NULL, MAXDOCS)) {
#ifdef VMS
	    HTAlert(gettext("HTWAIS: Request too large."));
	    return_status = HT_NOT_LOADED;
	    FREE_TARGET;
	    goto CleanUp;
#else
	    panic("request too large");
#endif /* VMS */
	}

	HTProgress(gettext("Searching WAIS database..."));
	rv = interpret_message(request_message,
			       MAX_MESSAGE_LEN - request_buffer_length,
			       response_message,
			       MAX_MESSAGE_LEN,
			       connection,
			       false	/* true verbose */
	    );

	if (rv == HT_INTERRUPTED) {
	    HTAlert(gettext("Search interrupted."));
	    return_status = HT_INTERRUPTED;
	    FREE_TARGET;
	    goto CleanUp;
	} else if (!rv) {
#ifdef VMS
	    HTAlert(HTWAIS_MESSAGE_TOO_BIG);
	    return_status = HT_NOT_LOADED;
	    FREE_TARGET;
	    goto CleanUp;
#else
	    panic("returned message too large");
#endif /* VMS */
	} else {		/* returned message ok */
	    SearchResponseAPDU *query_response = 0;

	    readSearchResponseAPDU(&query_response,
				   response_message + HEADER_LENGTH);
	    display_search_response(target,
				    query_response, wais_database, keywords);
	    if (query_response->DatabaseDiagnosticRecords)
		freeWAISSearchResponse(query_response->DatabaseDiagnosticRecords);
	    freeSearchResponseAPDU(query_response);
	}			/* returned message not too large */
	FREE_TARGET;
    } else {			/* D O C U M E N T    F E T C H */
	HTFormat format_in;
	boolean binary;		/* how to transfer stuff coming over */
	HTStream *target;
	long count;
	any doc_chunk;
	any *docid = &doc_chunk;

	CTRACE((tfp,
		"HTWAIS: Retrieve document id `%s' type `%s' length %ld\n",
		docname, doctype, document_length));

	format_in =
	    !strcmp(doctype, "WSRC") ? HTAtom_for("application/x-wais-source") :
	    !strcmp(doctype, "TEXT") ? HTAtom_for("text/plain") :
	    !strcmp(doctype, "HTML") ? HTAtom_for("text/html") :
	    !strcmp(doctype, "GIF") ? HTAtom_for("image/gif") :
	    HTAtom_for("application/octet-stream");
	binary =
	    0 != strcmp(doctype, "WSRC") &&
	    0 != strcmp(doctype, "TEXT") &&
	    0 != strcmp(doctype, "HTML");

	target = HTStreamStack(format_in, format_out, sink, anAnchor);
	if (!target)
	    return HTLoadError(sink, 500,
			       gettext("Can't convert format of WAIS document"));
	/*
	 * Decode hex or litteral format for document ID.
	 */
	WAIS_from_WWW(docid, docname);

	/*
	 * Loop over slices of the document.
	 */
	for (count = 0;
	     count * CHARS_PER_PAGE < document_length;
	     count++) {
#ifdef VMS
	    char *type = NULL;

	    StrAllocCopy(type, doctype);
#else
	    char *type = s_strdup(doctype);	/* Gets freed I guess */
#endif /* VMS */
	    request_buffer_length = MAX_MESSAGE_LEN;	/* Amount left */
	    CTRACE((tfp, "HTWAIS: Slice number %ld\n", count));

	    if (HTCheckForInterrupt()) {
		HTAlert(TRANSFER_INTERRUPTED);
		(*target->isa->_abort) (target, NULL);
#ifdef VMS
		FREE(type);
#endif /* VMS */
		return_status = HT_NOT_LOADED;
		goto CleanUp;
	    }

	    if (0 ==
		generate_retrieval_apdu(request_message + HEADER_LENGTH,
					&request_buffer_length,
					docid,
					CT_byte,
					count * CHARS_PER_PAGE,
					(((count + 1) * CHARS_PER_PAGE <= document_length)
					 ? (count + 1) * CHARS_PER_PAGE
					 : document_length),
					type,
					wais_database)) {
#ifdef VMS
		HTAlert(gettext("HTWAIS: Request too long."));
		return_status = HT_NOT_LOADED;
		FREE_TARGET;
		FREE(type);
		FREE(docid->bytes);
		goto CleanUp;
#else
		panic("request too long");
#endif /* VMS */
	    }

	    /*
	     * Actually do the transaction given by request_message.
	     */
	    HTProgress(gettext("Fetching WAIS document..."));
	    rv = interpret_message(request_message,
				   MAX_MESSAGE_LEN - request_buffer_length,
				   response_message,
				   MAX_MESSAGE_LEN,
				   connection,
				   false	/* true verbose */
		);
	    if (rv == HT_INTERRUPTED) {
		HTAlert(TRANSFER_INTERRUPTED);
		return_status = HT_INTERRUPTED;
		FREE_TARGET;
		FREE(type);
		FREE(docid->bytes);
		goto CleanUp;
	    } else if (!rv) {
#ifdef VMS
		HTAlert(HTWAIS_MESSAGE_TOO_BIG);
		return_status = HT_NOT_LOADED;
		FREE_TARGET;
		FREE(type);
		FREE(docid->bytes);
		goto CleanUp;
#else
		panic("Returned message too large");
#endif /* VMS */
	    }

	    /*
	     * Parse the result which came back into memory.
	     */
	    readSearchResponseAPDU(&retrieval_response,
				   response_message + HEADER_LENGTH);

	    if (NULL ==
		((WAISSearchResponse *)
		 retrieval_response->DatabaseDiagnosticRecords)->Text) {
		/* display_search_response(target, retrieval_response,
		   wais_database, keywords); */
		PUTS(gettext("No text was returned!\n"));
		/* panic("No text was returned"); */
	    } else {
		output_text_record(target,
				   ((WAISSearchResponse *)
				    retrieval_response->DatabaseDiagnosticRecords)->Text[0],
				   false, binary);
	    }			/* If text existed */

#ifdef VMS
	    FREE(type);
#endif /* VMS */
	}			/* Loop over slices */

	FREE_TARGET;
	FREE(docid->bytes);

	freeWAISSearchResponse(retrieval_response->DatabaseDiagnosticRecords);
	freeSearchResponseAPDU(retrieval_response);

    }				/* If document rather than search */

  CleanUp:
    /*
     * (This postponed until later, after a timeout:)
     */
#ifdef VMS
    if (connection)
	NETCLOSE((int) connection);
#else
    if (connection)
	fclose(connection);
#endif /* VMS */
    FREE(wais_database);
#ifdef VMS
    FREE(request_message);
    FREE(response_message);
#else
    s_free(request_message);
    s_free(response_message);
#endif /* VMS */
    FREE(names);
    return (return_status);
}

#ifdef GLOBALDEF_IS_MACRO
#define _HTWAIS_C_1_INIT { "wais", HTLoadWAIS, NULL }
GLOBALDEF(HTProtocol, HTWAIS, _HTWAIS_C_1_INIT);
#else
GLOBALDEF HTProtocol HTWAIS =
{"wais", HTLoadWAIS, NULL};
#endif /* GLOBALDEF_IS_MACRO */
