/*			FINGER ACCESS				HTFinger.c
**			=============
** Authors:
**  ARB  Andrew Brooks
**
** History:
**	21 Apr 94   First version (ARB, from HTNews.c by TBL)
**	12 Mar 96   Made the URL and command buffering secure from
**		     stack modifications, beautified the HTLoadFinger()
**		     and response() functions, and added support for the
**		     following URL formats for sending a "", "/w",
**		     "username[@host]", or "/w username[@host]" command
**		     to the server:
**			finger://host
**			finger://host/
**			finger://host/%2fw
**			finger://host/%2fw%20username[@host]
**			finger://host/w/username[@host]
**			finger://host/username[@host]
**			finger://host/username[@host]/w
**			finger://username@host
**			finger://username@host/
**			finger://username@host/w
**	15 Mar 96   Added support for port 79 gtype 0 gopher URLs
**		     relayed from HTLoadGopher. - FM
*/

#include "HTUtils.h"
#include "tcp.h"
#include "HTAlert.h"
#include "HTML.h"
#include "HTParse.h"
#include "HTFormat.h"
#include "HTTCP.h"
#include "HTString.h"
#include "HTFinger.h"

#include "LYLeaks.h"

/* #define TRACE 1 */

#define FINGER_PORT 79		/* See rfc742 */
#define BIG 1024		/* Bug */

#define FREE(x) if (x) {free(x); x = NULL;}

#define PUTC(c) (*targetClass.put_character)(target, c)
#define PUTS(s) (*targetClass.put_string)(target, s)
#define START(e) (*targetClass.start_element)(target, e, 0, 0, -1, 0)
#define END(e) (*targetClass.end_element)(target, e, 0)
#define FREE_TARGET (*targetClass._free)(target)
#define NEXT_CHAR HTGetCharacter() 


/*	Module-wide variables
*/
PRIVATE int s;					/* Socket for FingerHost */

struct _HTStructured {
	CONST HTStructuredClass * isa;		/* For gopher streams */
	/* ... */
};

PRIVATE HTStructured * target;			/* The output sink */
PRIVATE HTStructuredClass targetClass;		/* Copy of fn addresses */

/*	Initialisation for this module
**	------------------------------
*/
PRIVATE BOOL initialized = NO;
PRIVATE BOOL initialize NOARGS
{
  s = -1;		/* Disconnected */
  return YES;
}



/*	Start anchor element
**	--------------------
*/
PRIVATE void start_anchor ARGS1(CONST char *,  href)
{
    BOOL		present[HTML_A_ATTRIBUTES];
    CONST char*		value[HTML_A_ATTRIBUTES];
    
    {
    	int i;
    	for(i=0; i<HTML_A_ATTRIBUTES; i++)
	    present[i] = (i==HTML_A_HREF);
    }
    ((CONST char **)value)[HTML_A_HREF] = href;
    (*targetClass.start_element)(target, HTML_A, present,
    				 (CONST char **)value, -1, 0);

}

/*	Send Finger Command line to remote host & Check Response
**	--------------------------------------------------------
**
** On entry,
**	command	points to the command to be sent, including CRLF, or is null
**		pointer if no command to be sent.
** On exit,
**	Negative status indicates transmission error, socket closed.
**	Positive status is a Finger status.
*/


PRIVATE int response ARGS5(
	CONST char *,		command,
	char *,			sitename,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    int status;
    int length = strlen(command);
    int ch, i;
    char line[BIG], *l, *cmd=NULL;
    char *p = line, *href=NULL;

    if (length == 0)
        return(-1);

    /* Set up buffering.
    */
    HTInitInput(s);

    /* Send the command.
    */
    if (TRACE) 
        fprintf(stderr, "HTFinger command to be sent: %s", command);
    status = NETWRITE(s, (char *)command, length);
    if (status < 0) {
        if (TRACE)
	    fprintf(stderr,
                    "HTFinger: Unable to send command. Disconnecting.\n");
        NETCLOSE(s);
        s = -1;
        return status;
    } /* if bad status */
  
    /* Make a hypertext object with an anchor list.
    */
    target = HTML_new(anAnchor, format_out, sink);
    targetClass = *target->isa;	/* Copy routine entry points */

    /* Create the results report.
    */
    if (TRACE)
	fprintf(stderr,"HTFinger: Reading finger information\n");
    START(HTML_HTML);
    PUTS("\n");
    START(HTML_HEAD);
    PUTS("\n");
    START(HTML_TITLE);
    PUTS("Finger server on ");
    PUTS(sitename);
    END(HTML_TITLE);
    PUTS("\n");
    END(HTML_HEAD);
    PUTS("\n");
    START(HTML_BODY);
    PUTS("\n");
    START(HTML_H1);
    PUTS("Finger server on ");
    START(HTML_EM);
    PUTS(sitename);
    END(HTML_EM);
    PUTS(": ");
    if (command) {
        StrAllocCopy(cmd, command);
    } else {
        StrAllocCopy(cmd, "");
    }
    for (i = (strlen(cmd) - 1); i >= 0; i--) {
        if (cmd[i] == LF || cmd[i] == CR) {
	    cmd[i] = '\0';
	} else {
	    break;
	}
    }
    PUTS(cmd);
    FREE(cmd);
    END(HTML_H1);
    PUTS("\n");
    START(HTML_PRE);

    while ((ch=NEXT_CHAR) != (char)EOF) {

	if (interrupted_in_htgetcharacter) {
	    if (TRACE) {
	        fprintf(stderr,
		  "HTFinger: Interrupted in HTGetCharacter, apparently.\n");
	    }
	    _HTProgress ("Connection interrupted.");
	    goto end_html;
        }

	if (ch != LF) {
	    *p = ch;		/* Put character in line */
	    if (p < &line[BIG-1]) {
	        p++;
	    }
	} else {
	    *p = '\0';		/* Terminate line */
	    /*
	     * OK we now have a line.
	     * Load it as 'l' and parse it.
	     */
	    p = l = line;
	    while (*l) {
		if (strncmp(l, "news:", 5) &&
		    strncmp(l, "snews://", 8) &&
		    strncmp(l, "nntp://", 7) &&
		    strncmp(l, "snewspost:", 10) &&
		    strncmp(l, "snewsreply:", 11) &&
		    strncmp(l, "newspost:", 9) &&
		    strncmp(l, "newsreply:", 10) &&
		    strncmp(l, "ftp://", 6) &&
		    strncmp(l, "file:/", 6) &&
		    strncmp(l, "finger://", 9) &&
		    strncmp(l, "http://", 7) &&
		    strncmp(l, "https://", 8) &&
		    strncmp(l, "wais://", 7) &&
		    strncmp(l, "mailto:", 7) &&
		    strncmp(l, "cso://", 6) &&
		    strncmp(l, "gopher://", 9)) 
		    PUTC(*l++);
		else {
		    StrAllocCopy(href, l);
		    start_anchor(strtok(href, " \r\n\t,>)\""));
		    while (*l && !strchr(" \r\n\t,>)\"", *l))
		        PUTC(*l++);
		    END(HTML_A);
		    FREE(href);
		}
	    }
	    PUTC('\n');
	}
    }
    NETCLOSE(s);
    s = -1;

end_html:
    END(HTML_PRE);
    PUTS("\n");
    END(HTML_BODY);
    PUTS("\n");
    END(HTML_HTML);
    PUTS("\n");
    FREE_TARGET;
    return(0);
}


/*		Load by name					HTLoadFinger
**		============
*/
PUBLIC int HTLoadFinger ARGS4(
	CONST char *,		arg,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		stream)
{
    char *username, *sitename, *colon;	/* Fields extracted from URL */
    char *slash, *at_sign;		/* Fields extracted from URL */
    char *command, *str;		/* Buffers */
    int port;				/* Port number from URL */
    int status;				/* tcp return */
  
    if (TRACE) {
        fprintf(stderr, "HTFinger: Looking for %s\n", (arg ? arg : "NULL"));
    }
  
    if (!(arg && *arg)) {
        HTAlert("Could not load data.");
	return HT_NOT_LOADED;			/* Ignore if no name */
    }
  
    if (!initialized) 
        initialized = initialize();
    if (!initialized) {
        HTAlert ("Could not set up finger connection.");
	return HT_NOT_LOADED;	/* FAIL */
    }
    
  {
    CONST char * p1=arg;
    BOOL IsGopherURL = FALSE;
    
    /*  Set up the host and command fields.
    */        
    if (!strncasecomp(arg, "finger://", 9)) {
        p1 = arg + 9;  /* Skip "finger://" prefix */
    } else if (!strncasecomp(arg, "gopher://", 9)) {
        p1 = arg + 9;  /* Skip "gopher://" prefix */
	IsGopherURL = TRUE;
    }
    sitename = (char *)p1;

    if ((slash = strchr(sitename, '/')) != NULL) {
        *slash++ = '\0';
	HTUnEscape(slash);
	if (IsGopherURL) {
	    if (*slash != '0') {
	        HTAlert("Could not load data.");
		return HT_NOT_LOADED;	/* FAIL */
	    }
	    *slash++ = '\0';
	}
    }
    if ((at_sign = strchr(sitename, '@')) != NULL) {
        if (IsGopherURL) {
            HTAlert("Could not load data.");
	    return HT_NOT_LOADED;	/* FAIL */
	}
        *at_sign++ = '\0';
        username = sitename;
	sitename = at_sign;
	HTUnEscape(username);
    } else if (slash) {
        username = slash;
    } else {
        username = "";
    }
    
    if (*sitename == '\0') {
        HTAlert("Could not load data (no sitename in finger URL)");
	return HT_NOT_LOADED;		/* Ignore if no name */
    }

    if ((colon = strchr(sitename, ':')) != NULL) {
        *colon++ = '\0';
	port = atoi(colon);
	if (port != 79) {
	    HTAlert("Invalid port number - will only use port 79!");
	    return HT_NOT_LOADED;	/* Ignore if wrong port */
	}
    }

    /* Load the string for making a connection/
    */
    str = (char *)calloc(1, (strlen(sitename) + 10));
    if (str == NULL)
        outofmem(__FILE__, "HTLoadFinger");
    sprintf(str, "lose://%s/", sitename);
    
    /* Load the command for the finger server.
    */
    command = (char *)calloc(1, (strlen(username) + 10));
    if (command == NULL)
        outofmem(__FILE__, "HTLoadFinger");
    if (at_sign && slash) {
        if (*slash == 'w' || *slash == 'W') {
	    sprintf(command, "/w %s%c%c", username, CR, LF);
	} else {
	    sprintf(command, "%s%c%c", username, CR, LF);
	}
    } else if (at_sign) {
	sprintf(command, "%s%c%c", username, CR, LF);
    } else if (*username == '/') {
        if ((slash = strchr((username+1), '/')) != NULL) {
	    *slash = ' ';
	}
	sprintf(command, "%s%c%c", username, CR, LF);
    } else if ((*username == 'w' || *username == 'W') &&
    	       *(username+1) == '/') {
	if (*username+2 != '\0') {
	    *(username+1) = ' ';
	} else {
	    *(username+1) = '\0';
	}
	sprintf(command, "/%s%c%c", username, CR, LF);
    } else if ((*username == 'w' || *username == 'W') &&
    	       *(username+1) == '\0') {
	sprintf(command, "/%s%c%c", username, CR, LF);
    } else if ((slash = strchr(username, '/')) != NULL) {
	*slash++ = '\0';
	if (*slash == 'w' || *slash == 'W') {
	    sprintf(command, "/w %s%c%c", username, CR, LF);
	} else {
	    sprintf(command, "%s%c%c", username, CR, LF);
	}
    } else {
	sprintf(command, "%s%c%c", username, CR, LF);
    }
  } /* scope of p1 */
  
    /* Now, let's get a stream setup up from the FingerHost:
    ** CONNECTING to finger host
    */
    if (TRACE)
        fprintf(stderr, "HTFinger: doing HTDoConnect on '%s'\n", str);
    status = HTDoConnect(str, "finger", FINGER_PORT, &s);
    if (TRACE)
        fprintf(stderr, "HTFinger: Done DoConnect; status %d\n", status);

    if (status == HT_INTERRUPTED) {
        /* Interrupt cleanly */
	if (TRACE)
	    fprintf(stderr,
	    	  "HTFinger: Interrupted on connect; recovering cleanly.\n");
	HTProgress ("Connection interrupted.");
	FREE(str);
	FREE(command);
	return HT_NOT_LOADED;
    }
    if (status < 0) {
        NETCLOSE(s);
	s = -1;
	if (TRACE) 
	    fprintf(stderr, "HTFinger: Unable to connect to finger host.\n");
        HTAlert("Could not access finger host.");
	FREE(str);
	FREE(command);
	return HT_NOT_LOADED;	/* FAIL */
    }
    if (TRACE)
        fprintf(stderr, "HTFinger: Connected to finger host '%s'.\n", str);
    FREE(str);

    /* Send the command, and process response if successful.
    */
    if (response(command, sitename, anAnchor, format_out, stream) != 0) {
        HTAlert("No response from finger server.");
	FREE(command);
	return HT_NOT_LOADED;
    }

    FREE(command);
    return HT_LOADED;
}

#ifdef GLOBALDEF_IS_MACRO
#define _HTFINGER_C_1_INIT { "finger", HTLoadFinger, NULL }
GLOBALDEF (HTProtocol, HTFinger, _HTFINGER_C_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol HTFinger = { "finger", HTLoadFinger, NULL };
#endif /* GLOBALDEF_IS_MACRO */
