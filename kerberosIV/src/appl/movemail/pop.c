/* pop.c: client routines for talking to a POP3-protocol post-office server
   Copyright (c) 1991,1993 Free Software Foundation, Inc.
   Written by Jonathan Kamens, jik@security.ov.com.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#include <protos.h>
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <pop.h>

#ifdef HESIOD
#include <hesiod.h>
/*
 * It really shouldn't be necessary to put this declaration here, but
 * the version of hesiod.h that Athena has installed in release 7.2
 * doesn't declare this function; I don't know if the 7.3 version of
 * hesiod.h does.
 */
extern struct servent *hes_getservbyname (/* char *, char * */);
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <errno.h>

#ifdef SOCKS
#include <socks.h>
/* This doesn't belong here. */
struct tm *localtime(const time_t *);
struct hostent  *gethostbyname(const char *);
#endif

#include <des.h>
#include <krb.h>

#include <roken.h>

static int socket_connection ( char *, int );
static char *pop_getline ( popserver );
static int sendline ( popserver, char * );
static int fullwrite ( int, char *, int );
static int getok ( popserver );
#if 0
static int gettermination ( popserver );
#endif
static void pop_trash ( popserver );
static char *find_crlf ( char * );
#if defined(__ultrix) || IRIX == 4
char *getpass(const char*);
#endif

#define ERROR_MAX 80		/* a pretty arbitrary size */
#define POP_PORT 110
#define KPOP_PORT 1109
#define POP_SERVICE "pop"
#define KPOP_SERVICE "kpop"

char pop_error[ERROR_MAX];
int pop_debug = 0;

/*
 * Function: pop_open (char *host, char *username, char *password,
 * 		       int flags)
 *
 * Purpose: Establishes a connection with a post-office server, and
 * 	completes the authorization portion of the session.
 *
 * Arguments:
 * 	host	The server host with which the connection should be
 * 		established.  Optional.  If omitted, internal
 * 		heuristics will be used to determine the server host,
 * 		if possible.
 * 	username
 * 		The username of the mail-drop to access.  Optional.
 * 		If omitted, internal heuristics will be used to
 * 		determine the username, if possible.
 * 	password
 * 		The password to use for authorization.  If omitted,
 * 		internal heuristics will be used to determine the
 * 		password, if possible.
 * 	flags	A bit mask containing flags controlling certain
 * 		functions of the routine.  Valid flags are defined in
 * 		the file pop.h
 *
 * Return value: Upon successful establishment of a connection, a
 * 	non-null popserver will be returned.  Otherwise, null will be
 * 	returned, and the string variable pop_error will contain an
 * 	explanation of the error.
 */
popserver
pop_open (host, username, password, flags)
     char *host;
     char *username;
     char *password;
     int flags;
{
  int sock;
  popserver server;

  /* Determine the user name */
  if (! username)
    {
      username = getenv ("USER");
      if (! (username && *username))
	{
	  username = getlogin ();
	  if (! (username && *username))
	    {
	      struct passwd *passwd;
	      passwd = getpwuid (getuid ());
	      if (passwd && passwd->pw_name && *passwd->pw_name)
		{
		  username = passwd->pw_name;
		}
	      else
		{
		  strcpy (pop_error, "Could not determine username");
		  return (0);
		}
	    }
	}
    }

  /*
   *  Determine the mail host.
   */

  if (! host)
    {
      host = getenv ("MAILHOST");
    }

#ifdef HESIOD
  if ((! host) && (! (flags & POP_NO_HESIOD)))
    {
      struct hes_postoffice *office;
      office = hes_getmailhost (username);
      if (office && office->po_type && (! strcmp (office->po_type, "POP"))
	  && office->po_name && *office->po_name && office->po_host
	  && *office->po_host)
	{
	  host = office->po_host;
	  username = office->po_name;
	}
    }
#endif

#ifdef MAILHOST
  if (! host)
    {
      host = MAILHOST;
    }
#endif

  if (! host)
    {
      strcpy (pop_error, "Could not determine POP server");
      return (0);
    }

  /* Determine the password */
#define DONT_NEED_PASSWORD (! (flags & POP_NO_KERBEROS))
 
  if ((! password) && (! DONT_NEED_PASSWORD))
    {
      if (! (flags & POP_NO_GETPASS))
	{
	  password = getpass ("Enter POP password:");
	}
      if (! password)
	{
	  strcpy (pop_error, "Could not determine POP password");
	  return (0);
	}
    }
  if (password)
    flags |= POP_NO_KERBEROS;
  else
    password = username;

  sock = socket_connection (host, flags);
  if (sock == -1)
    return (0);

  server = (popserver) malloc (sizeof (struct _popserver));
  if (! server)
    {
      strcpy (pop_error, "Out of memory in pop_open");
      return (0);
    }
  server->buffer = (char *) malloc (GETLINE_MIN);
  if (! server->buffer)
    {
      strcpy (pop_error, "Out of memory in pop_open");
      free (server);
      return (0);
    }
	  
  server->file = sock;
  server->data = 0;
  server->buffer_index = 0;
  server->buffer_size = GETLINE_MIN;
  server->in_multi = 0;

  if (getok (server))
    return (0);

  /*
   * I really shouldn't use the pop_error variable like this, but....
   */
  if (strlen (username) > ERROR_MAX - 6)
    {
      pop_close (server);
      strcpy (pop_error,
	      "Username too long; recompile pop.c with larger ERROR_MAX");
      return (0);
    }
  sprintf (pop_error, "USER %s", username);

  if (sendline (server, pop_error) || getok (server))
    {
      return (0);
    }

  if (strlen (password) > ERROR_MAX - 6)
    {
      pop_close (server);
      strcpy (pop_error,
	      "Password too long; recompile pop.c with larger ERROR_MAX");
      return (0);
    }
  sprintf (pop_error, "PASS %s", password);

  if (sendline (server, pop_error) || getok (server))
    {
      return (0);
    }

  return (server);
}

/*
 * Function: pop_stat
 *
 * Purpose: Issue the STAT command to the server and return (in the
 * 	value parameters) the number of messages in the maildrop and
 * 	the total size of the maildrop.
 *
 * Return value: 0 on success, or non-zero with an error in pop_error
 * 	in failure.
 *
 * Side effects: On failure, may make further operations on the
 * 	connection impossible.
 */
int
pop_stat (server, count, size)
     popserver server;
     int *count;
     int *size;
{
  char *fromserver;

  if (server->in_multi)
    {
      strcpy (pop_error, "In multi-line query in pop_stat");
      return (-1);
    }
     
  if (sendline (server, "STAT") || (! (fromserver = pop_getline (server))))
    return (-1);

  if (strncmp (fromserver, "+OK ", 4))
    {
      if (0 == strncmp (fromserver, "-ERR", 4))
	{
	  strncpy (pop_error, fromserver, ERROR_MAX);
	}
      else
	{
	  strcpy (pop_error,
		  "Unexpected response from POP server in pop_stat");
	  pop_trash (server);
	}
      return (-1);
    }

  *count = atoi (&fromserver[4]);
     
  fromserver = strchr (&fromserver[4], ' ');
  if (! fromserver)
    {
      strcpy (pop_error,
	      "Badly formatted response from server in pop_stat");
      pop_trash (server);
      return (-1);
    }

  *size = atoi (fromserver + 1);

  return (0);
}

/*
 * Function: pop_list
 *
 * Purpose: Performs the POP "list" command and returns (in value
 * 	parameters) two malloc'd zero-terminated arrays -- one of
 * 	message IDs, and a parallel one of sizes.
 *
 * Arguments:
 * 	server	The pop connection to talk to.
 * 	message	The number of the one message about which to get
 * 		information, or 0 to get information about all
 * 		messages.
 *
 * Return value: 0 on success, non-zero with error in pop_error on
 * 	failure.
 *
 * Side effects: On failure, may make further operations on the
 * 	connection impossible.
 */
int
pop_list (server, message, IDs, sizes)
     popserver server;
     int message;
     int **IDs;
     int **sizes;
{
  int how_many, i;
  char *fromserver;

  if (server->in_multi)
    {
      strcpy (pop_error, "In multi-line query in pop_list");
      return (-1);
    }

  if (message)
    how_many = 1;
  else
    {
      int count, size;
      if (pop_stat (server, &count, &size))
	return (-1);
      how_many = count;
    }

  *IDs = (int *) malloc ((how_many + 1) * sizeof (int));
  *sizes = (int *) malloc ((how_many + 1) * sizeof (int));
  if (! (*IDs && *sizes))
    {
      strcpy (pop_error, "Out of memory in pop_list");
      return (-1);
    }

  if (message)
    {
      sprintf (pop_error, "LIST %d", message);
      if (sendline (server, pop_error))
	{
	  free (*IDs);
	  free (*sizes);
	  return (-1);
	}
      if (! (fromserver = pop_getline (server)))
	{
	  free (*IDs);
	  free (*sizes);
	  return (-1);
	}
      if (strncmp (fromserver, "+OK ", 4))
	{
	  if (! strncmp (fromserver, "-ERR", 4))
	    strncpy (pop_error, fromserver, ERROR_MAX);
	  else
	    {
	      strcpy (pop_error,
		      "Unexpected response from server in pop_list");
	      pop_trash (server);
	    }
	  free (*IDs);
	  free (*sizes);
	  return (-1);
	}
      (*IDs)[0] = atoi (&fromserver[4]);
      fromserver = strchr (&fromserver[4], ' ');
      if (! fromserver)
	{
	  strcpy (pop_error,
		  "Badly formatted response from server in pop_list");
	  pop_trash (server);
	  free (*IDs);
	  free (*sizes);
	  return (-1);
	}
      (*sizes)[0] = atoi (fromserver);
      (*IDs)[1] = (*sizes)[1] = 0;
      return (0);
    }
  else
    {
      if (pop_multi_first (server, "LIST", &fromserver))
	{
	  free (*IDs);
	  free (*sizes);
	  return (-1);
	}
      for (i = 0; i < how_many; i++)
	{
	  if (pop_multi_next (server, &fromserver))
	    {
	      free (*IDs);
	      free (*sizes);
	      return (-1);
	    }
	  (*IDs)[i] = atoi (fromserver);
	  fromserver = strchr (fromserver, ' ');
	  if (! fromserver)
	    {
	      strcpy (pop_error,
		      "Badly formatted response from server in pop_list");
	      free (*IDs);
	      free (*sizes);
	      pop_trash (server);
	      return (-1);
	    }
	  (*sizes)[i] = atoi (fromserver);
	}
      if (pop_multi_next (server, &fromserver))
	{
	  free (*IDs);
	  free (*sizes);
	  return (-1);
	}
      else if (fromserver)
	{
	  strcpy (pop_error,
		  "Too many response lines from server in pop_list");
	  free (*IDs);
	  free (*sizes);
	  return (-1);
	}
      (*IDs)[i] = (*sizes)[i] = 0;
      return (0);
    }
}

/*
 * Function: pop_retrieve
 *
 * Purpose: Retrieve a specified message from the maildrop.
 *
 * Arguments:
 * 	server	The server to retrieve from.
 * 	message	The message number to retrieve.
 *	markfrom
 * 		If true, then mark the string "From " at the beginning
 * 		of lines with '>'.
 * 
 * Return value: A string pointing to the message, if successful, or
 * 	null with pop_error set if not.
 *
 * Side effects: May kill connection on error.
 */
char *
pop_retrieve (server, message, markfrom)
     popserver server;
     int message;
     int markfrom;
{
  int *IDs, *sizes, bufsize, fromcount = 0, cp = 0;
  char *ptr, *fromserver;
  int ret;

  if (server->in_multi)
    {
      strcpy (pop_error, "In multi-line query in pop_retrieve");
      return (0);
    }

  if (pop_list (server, message, &IDs, &sizes))
    return (0);

  if (pop_retrieve_first (server, message, &fromserver))
    {
      return (0);
    }

  /*
   * The "5" below is an arbitrary constant -- I assume that if
   * there are "From" lines in the text to be marked, there
   * probably won't be more than 5 of them.  If there are, I
   * allocate more space for them below.
   */
  bufsize = sizes[0] + (markfrom ? 5 : 0);
  ptr = malloc (bufsize);
  free (IDs);
  free (sizes);

  if (! ptr)
    {
      strcpy (pop_error, "Out of memory in pop_retrieve");
      pop_retrieve_flush (server);
      return (0);
    }

  while (! (ret = pop_retrieve_next (server, &fromserver)))
    {
      int linesize;

      if (! fromserver)
	{
	  ptr[cp] = '\0';
	  return (ptr);
	}
      if (markfrom && fromserver[0] == 'F' && fromserver[1] == 'r' &&
	  fromserver[2] == 'o' && fromserver[3] == 'm' &&
	  fromserver[4] == ' ')
	{
	  if (++fromcount == 5)
	    {
	      bufsize += 5;
	      ptr = realloc (ptr, bufsize);
	      if (! ptr)
		{
		  strcpy (pop_error, "Out of memory in pop_retrieve");
		  pop_retrieve_flush (server);
		  return (0);
		}
	      fromcount = 0;
	    }
	  ptr[cp++] = '>';
	}
      linesize = strlen (fromserver);
      memcpy (&ptr[cp], fromserver, linesize);
      cp += linesize;
      ptr[cp++] = '\n';
    }

  if (ret)
    {
      free (ptr);
      return (0);
    }
  return (0);
}

int
pop_retrieve_first (server, message, response)
     popserver server;
     int message;
     char **response;
{
  sprintf (pop_error, "RETR %d", message);
  return (pop_multi_first (server, pop_error, response));
}

int
pop_retrieve_next (server, line)
     popserver server;
     char **line;
{
  return (pop_multi_next (server, line));
}

int
pop_retrieve_flush (server)
     popserver server;
{
  return (pop_multi_flush (server));
}

int
pop_top_first (server, message, lines, response)
     popserver server;
     int message, lines;
     char **response;
{
  sprintf (pop_error, "TOP %d %d", message, lines);
  return (pop_multi_first (server, pop_error, response));
}

int
pop_top_next (server, line)
     popserver server;
     char **line;
{
  return (pop_multi_next (server, line));
}

int
pop_top_flush (server)
     popserver server;
{
  return (pop_multi_flush (server));
}

int
pop_multi_first (server, command, response)
     popserver server;
     char *command;
     char **response;
{
  if (server->in_multi)
    {
      strcpy (pop_error,
	      "Already in multi-line query in pop_multi_first");
      return (-1);
    }

  if (sendline (server, command) || (! (*response = pop_getline (server))))
    {
      return (-1);
    }

  if (0 == strncmp (*response, "-ERR", 4))
    {
      strncpy (pop_error, *response, ERROR_MAX);
      return (-1);
    }
  else if (0 == strncmp (*response, "+OK", 3))
    {
      for (*response += 3; **response == ' '; (*response)++) /* empty */;
      server->in_multi = 1;
      return (0);
    }
  else
    {
      strcpy (pop_error,
	      "Unexpected response from server in pop_multi_first");
      return (-1);
    }
}

int
pop_multi_next (server, line)
     popserver server;
     char **line;
{
  char *fromserver;

  if (! server->in_multi)
    {
      strcpy (pop_error, "Not in multi-line query in pop_multi_next");
      return (-1);
    }

  fromserver = pop_getline (server);
  if (! fromserver)
    {
      return (-1);
    }

  if (fromserver[0] == '.')
    {
      if (! fromserver[1])
	{
	  *line = 0;
	  server->in_multi = 0;
	  return (0);
	}
      else
	{
	  *line = fromserver + 1;
	  return (0);
	}
    }
  else
    {
      *line = fromserver;
      return (0);
    }
}

int
pop_multi_flush (server)
     popserver server;
{
  char *line;

  if (! server->in_multi)
    {
      return (0);
    }

  while (! pop_multi_next (server, &line))
    {
      if (! line)
	{
	  return (0);
	}
    }

  return (-1);
}

/* Function: pop_delete
 *
 * Purpose: Delete a specified message.
 *
 * Arguments:
 * 	server	Server from which to delete the message.
 * 	message	Message to delete.
 *
 * Return value: 0 on success, non-zero with error in pop_error
 * 	otherwise.
 */
int
pop_delete (server, message)
     popserver server;
     int message;
{
  if (server->in_multi)
    {
      strcpy (pop_error, "In multi-line query in pop_delete");
      return (-1);
    }

  sprintf (pop_error, "DELE %d", message);

  if (sendline (server, pop_error) || getok (server))
    return (-1);

  return (0);
}

/*
 * Function: pop_noop
 *
 * Purpose: Send a noop command to the server.
 *
 * Argument:
 * 	server	The server to send to.
 *
 * Return value: 0 on success, non-zero with error in pop_error
 * 	otherwise.
 *
 * Side effects: Closes connection on error.
 */
int
pop_noop (server)
     popserver server;
{
  if (server->in_multi)
    {
      strcpy (pop_error, "In multi-line query in pop_noop");
      return (-1);
    }

  if (sendline (server, "NOOP") || getok (server))
    return (-1);

  return (0);
}

/*
 * Function: pop_last
 *
 * Purpose: Find out the highest seen message from the server.
 *
 * Arguments:
 * 	server	The server.
 *
 * Return value: If successful, the highest seen message, which is
 * 	greater than or equal to 0.  Otherwise, a negative number with
 * 	the error explained in pop_error.
 *
 * Side effects: Closes the connection on error.
 */
int
pop_last (server)
     popserver server;
{
  char *fromserver;
     
  if (server->in_multi)
    {
      strcpy (pop_error, "In multi-line query in pop_last");
      return (-1);
    }

  if (sendline (server, "LAST"))
    return (-1);

  if (! (fromserver = pop_getline (server)))
    return (-1);

  if (! strncmp (fromserver, "-ERR", 4))
    {
      strncpy (pop_error, fromserver, ERROR_MAX);
      return (-1);
    }
  else if (strncmp (fromserver, "+OK ", 4))
    {
      strcpy (pop_error, "Unexpected response from server in pop_last");
      pop_trash (server);
      return (-1);
    }
  else
    {
      return (atoi (&fromserver[4]));
    }
}

/*
 * Function: pop_reset
 *
 * Purpose: Reset the server to its initial connect state
 *
 * Arguments:
 * 	server	The server.
 *
 * Return value: 0 for success, non-0 with error in pop_error
 * 	otherwise.
 *
 * Side effects: Closes the connection on error.
 */
int
pop_reset (server)
     popserver server;
{
  if (pop_retrieve_flush (server))
    {
      return (-1);
    }

  if (sendline (server, "RSET") || getok (server))
    return (-1);

  return (0);
}

/*
 * Function: pop_quit
 *
 * Purpose: Quit the connection to the server,
 *
 * Arguments:
 * 	server	The server to quit.
 *
 * Return value: 0 for success, non-zero otherwise with error in
 * 	pop_error.
 *
 * Side Effects: The popserver passed in is unuseable after this
 * 	function is called, even if an error occurs.
 */
int
pop_quit (server)
     popserver server;
{
  int ret = 0;

  if (server->file >= 0)
    {
      if (pop_retrieve_flush (server))
	{
	  ret = -1;
	}

      if (sendline (server, "QUIT") || getok (server))
	{
	  ret = -1;
	}

      close (server->file);
    }

  if (server->buffer)
    free (server->buffer);
  free (server);

  return (ret);
}

/*
 * Function: socket_connection
 *
 * Purpose: Opens the network connection with the mail host, without
 * 	doing any sort of I/O with it or anything.
 *
 * Arguments:
 * 	host	The host to which to connect.
 *	flags	Option flags.
 * 	
 * Return value: A file descriptor indicating the connection, or -1
 * 	indicating failure, in which case an error has been copied
 * 	into pop_error.
 */
static int
socket_connection (char *host, int flags)
{
  struct hostent *hostent;
  struct sockaddr_in addr;
  char found_port = 0;
  char *service;
  int sock;
  KTEXT ticket;
  MSG_DAT msg_data;
  CREDENTIALS cred;
  Key_schedule schedule;
  int rem;

  int try_count = 0;

  do
    {
      hostent = gethostbyname (host);
      try_count++;
      if ((! hostent)
	  && ((h_errno != TRY_AGAIN) || (try_count == 5)))
	{
	  strcpy (pop_error, "Could not determine POP server's address");
	  return (-1);
	}
    } while (! hostent);

  memset (&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;

  service = (flags & POP_NO_KERBEROS) ? POP_SERVICE : KPOP_SERVICE;

#ifdef HESIOD
  if (! (flags & POP_NO_HESIOD))
    {
      struct servent *servent = hes_getservbyname (service, "tcp");
      if (servent)
	{
	  addr.sin_port = servent->s_port;
	  found_port = 1;
	}
    }
#endif
  if (! found_port)
    {
      addr.sin_port = k_getportbyname (service, "tcp",
				       htons((flags & POP_NO_KERBEROS) ?
					     POP_PORT : KPOP_PORT));
    }

#define SOCKET_ERROR "Could not create socket for POP connection: "

  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      strcpy (pop_error, SOCKET_ERROR);
      strncat (pop_error, strerror (errno),
	       ERROR_MAX - sizeof (SOCKET_ERROR));
      return (-1);
	  
    }


  while (*hostent->h_addr_list)
    {
      memcpy (&addr.sin_addr, *hostent->h_addr_list, sizeof(addr.sin_addr));
      if (! connect (sock, (struct sockaddr *) &addr, sizeof (addr)))
	break;
      hostent->h_addr_list++;
    }

#define CONNECT_ERROR "Could not connect to POP server: "
     
  if (! *hostent->h_addr_list)
    {
      close (sock);
      strcpy (pop_error, CONNECT_ERROR);
      strncat (pop_error, strerror (errno),
	       ERROR_MAX - sizeof (CONNECT_ERROR));
      return (-1);
	  
    }

#define KRB_ERROR "Kerberos error connecting to POP server: "
  if (! (flags & POP_NO_KERBEROS))
    {
      char *hostname = strdup((char *)hostent->h_name);
      ticket = (KTEXT) malloc (sizeof (KTEXT_ST));
      rem = krb_sendauth (0L, sock, ticket, "pop", hostname,
			  krb_realmofhost (hostname),
			  0UL, &msg_data, &cred, schedule,
			  (struct sockaddr_in *) 0,
			  (struct sockaddr_in *) 0,
			  "KPOPV0.1");
      free (ticket);
      free (hostname);
      if (rem != KSUCCESS)
	{
	  strcpy (pop_error, KRB_ERROR);
	  strncat (pop_error, krb_get_err_text(rem),
		   ERROR_MAX - sizeof (KRB_ERROR));
	  close (sock);
	  return (-1);
	}
    }

  return (sock);
} /* socket_connection */

/*
 * Function: pop_getline
 *
 * Purpose: Get a line of text from the connection and return a
 * 	pointer to it.  The carriage return and linefeed at the end of
 * 	the line are stripped, but periods at the beginnings of lines
 * 	are NOT dealt with in any special way.
 *
 * Arguments:
 * 	server	The server from which to get the line of text.
 *
 * Returns: A non-null pointer if successful, or a null pointer on any
 * 	error, with an error message copied into pop_error.
 *
 * Notes: The line returned is overwritten with each call to pop_getline.
 *
 * Side effects: Closes the connection on error.
 */
static char *
pop_getline (popserver server)
{
#define GETLINE_ERROR "Error reading from server: "

  int ret;
  int search_offset = 0;

  if (server->data)
    {
      char *cp = find_crlf (server->buffer + server->buffer_index);
      if (cp)
	{
	  int found;
	  int data_used;

	  found = server->buffer_index;
	  data_used = (cp + 2) - server->buffer - found;
	       
	  *cp = '\0';		/* terminate the string to be returned */
	  server->data -= data_used;
	  server->buffer_index += data_used;

	  if (pop_debug)
	    fprintf (stderr, "<<< %s\n", server->buffer + found);
	  return (server->buffer + found);
	}
      else
	{
	  memmove (server->buffer, server->buffer + server->buffer_index,
		   server->data);
	  /* Record the fact that we've searched the data already in
             the buffer for a CRLF, so that when we search below, we
             don't have to search the same data twice.  There's a "-
             1" here to account for the fact that the last character
             of the data we have may be the CR of a CRLF pair, of
             which we haven't read the second half yet, so we may have
             to search it again when we read more data. */
	  search_offset = server->data - 1;
	  server->buffer_index = 0;
	}
    }
  else
    {
      server->buffer_index = 0;
    }

  while (1)
    {
      /* There's a "- 1" here to leave room for the null that we put
         at the end of the read data below.  We put the null there so
         that find_crlf knows where to stop when we call it. */
      if (server->data == server->buffer_size - 1)
	{
	  server->buffer_size += GETLINE_INCR;
	  server->buffer = realloc (server->buffer, server->buffer_size);
	  if (! server->buffer)
	    {
	      strcpy (pop_error, "Out of memory in pop_getline");
	      pop_trash (server);
	      return (0);
	    }
	}
      ret = read (server->file, server->buffer + server->data,
		  server->buffer_size - server->data - 1);
      if (ret < 0)
	{
	  strcpy (pop_error, GETLINE_ERROR);
	  strncat (pop_error, strerror (errno),
		   ERROR_MAX - sizeof (GETLINE_ERROR));
	  pop_trash (server);
	  return (0);
	}
      else if (ret == 0)
	{
	  strcpy (pop_error, "Unexpected EOF from server in pop_getline");
	  pop_trash (server);
	  return (0);
	}
      else
	{
	  char *cp;
	  server->data += ret;
	  server->buffer[server->data] = '\0';
	       
	  cp = find_crlf (server->buffer + search_offset);
	  if (cp)
	    {
	      int data_used = (cp + 2) - server->buffer;
	      *cp = '\0';
	      server->data -= data_used;
	      server->buffer_index = data_used;

	      if (pop_debug)
		fprintf (stderr, "<<< %s\n", server->buffer);
	      return (server->buffer);
	    }
	  search_offset += ret;
	}
    }

  /* NOTREACHED */
}

/*
 * Function: sendline
 *
 * Purpose: Sends a line of text to the POP server.  The line of text
 * 	passed into this function should NOT have the carriage return
 * 	and linefeed on the end of it.  Periods at beginnings of lines
 * 	will NOT be treated specially by this function.
 *
 * Arguments:
 * 	server	The server to which to send the text.
 * 	line	The line of text to send.
 *
 * Return value: Upon successful completion, a value of 0 will be
 * 	returned.  Otherwise, a non-zero value will be returned, and
 * 	an error will be copied into pop_error.
 *
 * Side effects: Closes the connection on error.
 */
static int
sendline (popserver server, char *line)
{
#define SENDLINE_ERROR "Error writing to POP server: "
  int ret;

  ret = fullwrite (server->file, line, strlen (line));
  if (ret >= 0)
    {				/* 0 indicates that a blank line was written */
      ret = fullwrite (server->file, "\r\n", 2);
    }

  if (ret < 0)
    {
      pop_trash (server);
      strcpy (pop_error, SENDLINE_ERROR);
      strncat (pop_error, strerror (errno),
	       ERROR_MAX - sizeof (SENDLINE_ERROR));
      return (ret);
    }

  if (pop_debug)
    fprintf (stderr, ">>> %s\n", line);

  return (0);
}

/*
 * Procedure: fullwrite
 *
 * Purpose: Just like write, but keeps trying until the entire string
 * 	has been written.
 *
 * Return value: Same as write.  Pop_error is not set.
 */
static int
fullwrite (int fd, char *buf, int nbytes)
{
  char *cp;
  int ret;

  cp = buf;
  while ((ret = write (fd, cp, nbytes)) > 0)
    {
      cp += ret;
      nbytes -= ret;
    }

  return (ret);
}

/*
 * Procedure getok
 *
 * Purpose: Reads a line from the server.  If the return indicator is
 * 	positive, return with a zero exit status.  If not, return with
 * 	a negative exit status.
 *
 * Arguments:
 * 	server	The server to read from.
 * 
 * Returns: 0 for success, else for failure and puts error in pop_error.
 *
 * Side effects: On failure, may make the connection unuseable.
 */
static int
getok (popserver server)
{
  char *fromline;

  if (! (fromline = pop_getline (server)))
    {
      return (-1);
    }

  if (! strncmp (fromline, "+OK", 3))
    return (0);
  else if (! strncmp (fromline, "-ERR", 4))
    {
      strncpy (pop_error, fromline, ERROR_MAX);
      pop_error[ERROR_MAX-1] = '\0';
      return (-1);
    }
  else
    {
      strcpy (pop_error,
	      "Unexpected response from server; expecting +OK or -ERR");
      pop_trash (server);
      return (-1);
    }
}	  

#if 0
/*
 * Function: gettermination
 *
 * Purpose: Gets the next line and verifies that it is a termination
 * 	line (nothing but a dot).
 *
 * Return value: 0 on success, non-zero with pop_error set on error.
 *
 * Side effects: Closes the connection on error.
 */
static int
gettermination (server)
     popserver server;
{
  char *fromserver;

  fromserver = pop_getline (server);
  if (! fromserver)
    return (-1);

  if (strcmp (fromserver, "."))
    {
      strcpy (pop_error,
	      "Unexpected response from server in gettermination");
      pop_trash (server);
      return (-1);
    }

  return (0);
}
#endif

/*
 * Function pop_close
 *
 * Purpose: Close a pop connection, sending a "RSET" command to try to
 * 	preserve any changes that were made and a "QUIT" command to
 * 	try to get the server to quit, but ignoring any responses that
 * 	are received.
 *
 * Side effects: The server is unuseable after this function returns.
 * 	Changes made to the maildrop since the session was started (or
 * 	since the last pop_reset) may be lost.
 */
void 
pop_close (server)
     popserver server;
{
  pop_trash (server);
  free (server);

  return;
}

/*
 * Function: pop_trash
 *
 * Purpose: Like pop_close or pop_quit, but doesn't deallocate the
 * 	memory associated with the server.  It is legal to call
 * 	pop_close or pop_quit after this function has been called.
 */
static void
pop_trash (popserver server)
{
  if (server->file >= 0)
    {
      sendline (server, "RSET");
      sendline (server, "QUIT");

      close (server->file);
      server->file = -1;
      if (server->buffer)
	{
	  free (server->buffer);
	  server->buffer = 0;
	}
    }
}

/* Return a pointer to the first CRLF in IN_STRING,
   or 0 if it does not contain one.  */

static char *
find_crlf (char *in_string)
{
  while (1)
    {
      if (! *in_string)
	return (0);
      else if (*in_string == '\r')
	{
	  if (*++in_string == '\n')
	    return (in_string - 1);
	}
      else
	in_string++;
    }
  /* NOTREACHED */
}

