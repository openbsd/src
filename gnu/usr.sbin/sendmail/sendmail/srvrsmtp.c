/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sendmail.h>

#ifndef lint
# if SMTP
static char id[] = "@(#)$Sendmail: srvrsmtp.c,v 8.457 2000/02/26 07:24:59 gshapiro Exp $ (with SMTP)";
# else /* SMTP */
static char id[] = "@(#)$Sendmail: srvrsmtp.c,v 8.457 2000/02/26 07:24:59 gshapiro Exp $ (without SMTP)";
# endif /* SMTP */
#endif /* ! lint */

#if SMTP
# if SASL
#  define ENC64LEN(l)	(((l) + 2) * 4 / 3 + 1)
static bool saslmechs __P((sasl_conn_t *, char **, bool));
# endif /* SASL */

static time_t	checksmtpattack __P((volatile int *, int, bool,
				     char *, ENVELOPE *));
static void	mail_esmtp_args __P((char *, char *, ENVELOPE *));
static void	printvrfyaddr __P((ADDRESS *, bool, bool));
static void	rcpt_esmtp_args __P((ADDRESS *, char *, char *, ENVELOPE *));
static int	runinchild __P((char *, ENVELOPE *));
static char	*skipword __P((char *volatile, char *));
extern ENVELOPE	BlankEnvelope;

/*
**  SMTP -- run the SMTP protocol.
**
**	Parameters:
**		nullserver -- if non-NULL, rejection message for
**			all SMTP commands.
**		e -- the envelope.
**
**	Returns:
**		never.
**
**	Side Effects:
**		Reads commands from the input channel and processes
**			them.
*/

struct cmd
{
	char	*cmd_name;	/* command name */
	int	cmd_code;	/* internal code, see below */
};

/* values for cmd_code */
# define CMDERROR	0	/* bad command */
# define CMDMAIL	1	/* mail -- designate sender */
# define CMDRCPT	2	/* rcpt -- designate recipient */
# define CMDDATA	3	/* data -- send message text */
# define CMDRSET	4	/* rset -- reset state */
# define CMDVRFY	5	/* vrfy -- verify address */
# define CMDEXPN	6	/* expn -- expand address */
# define CMDNOOP	7	/* noop -- do nothing */
# define CMDQUIT	8	/* quit -- close connection and die */
# define CMDHELO	9	/* helo -- be polite */
# define CMDHELP	10	/* help -- give usage info */
# define CMDEHLO	11	/* ehlo -- extended helo (RFC 1425) */
# define CMDETRN	12	/* etrn -- flush queue */
# if SASL
#  define CMDAUTH	13	/* auth -- SASL authenticate */
# endif /* SASL */
/* non-standard commands */
# define CMDONEX	16	/* onex -- sending one transaction only */
# define CMDVERB	17	/* verb -- go into verbose mode */
# define CMDXUSR	18	/* xusr -- initial (user) submission */
/* unimplemented commands from RFC 821 */
# define CMDUNIMPL	19	/* unimplemented rfc821 commands */
/* use this to catch and log "door handle" attempts on your system */
# define CMDLOGBOGUS	23	/* bogus command that should be logged */
/* debugging-only commands, only enabled if SMTPDEBUG is defined */
# define CMDDBGQSHOW	24	/* showq -- show send queue */
# define CMDDBGDEBUG	25	/* debug -- set debug mode */

static struct cmd	CmdTab[] =
{
	{ "mail",	CMDMAIL		},
	{ "rcpt",	CMDRCPT		},
	{ "data",	CMDDATA		},
	{ "rset",	CMDRSET		},
	{ "vrfy",	CMDVRFY		},
	{ "expn",	CMDEXPN		},
	{ "help",	CMDHELP		},
	{ "noop",	CMDNOOP		},
	{ "quit",	CMDQUIT		},
	{ "helo",	CMDHELO		},
	{ "ehlo",	CMDEHLO		},
	{ "etrn",	CMDETRN		},
	{ "verb",	CMDVERB		},
	{ "onex",	CMDONEX		},
	{ "xusr",	CMDXUSR		},
	{ "send",	CMDUNIMPL	},
	{ "saml",	CMDUNIMPL	},
	{ "soml",	CMDUNIMPL	},
	{ "turn",	CMDUNIMPL	},
# if SASL
	{ "auth",	CMDAUTH,	},
# endif /* SASL */
    /* remaining commands are here only to trap and log attempts to use them */
	{ "showq",	CMDDBGQSHOW	},
	{ "debug",	CMDDBGDEBUG	},
	{ "wiz",	CMDLOGBOGUS	},

	{ NULL,		CMDERROR	}
};

static bool	OneXact = FALSE;	/* one xaction only this run */
static char	*CurSmtpClient;		/* who's at the other end of channel */

# define MAXBADCOMMANDS	25	/* maximum number of bad commands */
# define MAXNOOPCOMMANDS	20	/* max "noise" commands before slowdown */
# define MAXHELOCOMMANDS	3	/* max HELO/EHLO commands before slowdown */
# define MAXVRFYCOMMANDS	6	/* max VRFY/EXPN commands before slowdown */
# define MAXETRNCOMMANDS	8	/* max ETRN commands before slowdown */
# define MAXTIMEOUT	(4 * 60)	/* max timeout for bad commands */

void
smtp(nullserver, d_flags, e)
	char *volatile nullserver;
	BITMAP256 d_flags;
	register ENVELOPE *volatile e;
{
	register char *volatile p;
	register struct cmd *volatile c = NULL;
	char *cmd;
	auto ADDRESS *vrfyqueue;
	ADDRESS *a;
	volatile bool gotmail;		/* mail command received */
	volatile bool gothello;		/* helo command received */
	bool vrfy;			/* set if this is a vrfy command */
	char *volatile protocol;	/* sending protocol */
	char *volatile sendinghost;	/* sending hostname */
	char *volatile peerhostname;	/* name of SMTP peer or "localhost" */
	auto char *delimptr;
	char *id;
	volatile int nrcpts = 0;	/* number of RCPT commands */
	bool doublequeue;
	volatile bool discard;
	volatile int badcommands = 0;	/* count of bad commands */
	volatile int nverifies = 0;	/* count of VRFY/EXPN commands */
	volatile int n_etrn = 0;	/* count of ETRN commands */
	volatile int n_noop = 0;	/* count of NOOP/VERB/ONEX etc cmds */
	volatile int n_helo = 0;	/* count of HELO/EHLO commands */
	volatile int delay = 1;		/* timeout for bad commands */
	bool ok;
	volatile bool tempfail = FALSE;
# if _FFR_MILTER
	volatile bool milterize = (nullserver == NULL);
# endif /* _FFR_MILTER */
	volatile time_t wt;		/* timeout after too many commands */
	volatile time_t previous;	/* time after checksmtpattack() */
	volatile int lognullconnection = TRUE;
	register char *q;
	char *addr;
	char *greetcode = "220";
	QUEUE_CHAR *new;
	int argno;
	char *args[MAXSMTPARGS];
	char inp[MAXLINE];
	char cmdbuf[MAXLINE];
# if SASL
	sasl_conn_t *conn;
	volatile bool sasl_ok;
	bool ismore;
	int result;
	volatile int authenticating;
	char *hostname;
	char *user;
	char *in, *out, *out2;
	const char *errstr;
	int inlen, out2len;
	unsigned int outlen;
	char *volatile auth_type;
	char *mechlist;
	int len;
	sasl_security_properties_t ssp;
	sasl_external_properties_t ext_ssf;
# endif /* SASL */

	if (fileno(OutChannel) != fileno(stdout))
	{
		/* arrange for debugging output to go to remote host */
		(void) dup2(fileno(OutChannel), fileno(stdout));
	}

	settime(e);
	(void)sm_getla(e);
	peerhostname = RealHostName;
	if (peerhostname == NULL)
		peerhostname = "localhost";
	CurHostName = peerhostname;
	CurSmtpClient = macvalue('_', e);
	if (CurSmtpClient == NULL)
		CurSmtpClient = CurHostName;

	/* check_relay may have set discard bit, save for later */
	discard = bitset(EF_DISCARD, e->e_flags);

	sm_setproctitle(TRUE, e, "server %s startup", CurSmtpClient);

# if SASL
	sasl_ok = FALSE;	/* SASL can't be used (yet) */

	/* SASL server new connection */
	hostname = macvalue('j', e);
#  if SASL > 10505
	/* use empty realm: doesn't work in SASL <= 1.5.5 */
	result = sasl_server_new("smtp", hostname, "", NULL, 0, &conn);
#  else /* SASL > 10505 */
	/* use no realm -> realm is set to hostname by SASL lib */
	result = sasl_server_new("smtp", hostname, NULL, NULL, 0, &conn);
#  endif /* SASL > 10505 */
	if (result == SASL_OK)
	{
		sasl_ok = TRUE;

		/*
		**  SASL set properties for sasl
		**  set local/remote IP
		**  XXX only IPv4: Cyrus SASL doesn't support anything else
		**
		**  XXX where exactly are these used/required?
		**  Kerberos_v4
		*/

# if NETINET
		in = macvalue(macid("{daemon_family}", NULL), e);
		if (in != NULL && strcmp(in, "inet") == 0)
		{
			SOCKADDR_LEN_T addrsize;
			struct sockaddr_in saddr_l;
			struct sockaddr_in saddr_r;

			addrsize = sizeof(struct sockaddr_in);
			if (getpeername(fileno(InChannel),
					(struct sockaddr *)&saddr_r,
					&addrsize) == 0)
			{
				sasl_setprop(conn, SASL_IP_REMOTE, &saddr_r);
				addrsize = sizeof(struct sockaddr_in);
				if (getsockname(fileno(InChannel),
						(struct sockaddr *)&saddr_l,
						&addrsize) == 0)
					sasl_setprop(conn, SASL_IP_LOCAL,
						     &saddr_l);
			}
		}
# endif /* NETINET */

		authenticating = SASL_NOT_AUTH;
		auth_type = NULL;
		mechlist = NULL;
		user = NULL;
#  if 0
		define(macid("{auth_author}", NULL), NULL, &BlankEnvelope);
#  endif /* 0 */

		/* set properties */
		(void) memset(&ssp, '\0', sizeof ssp);
		sasl_ok = sasl_setprop(conn, SASL_SEC_PROPS, &ssp) == SASL_OK;

		if (sasl_ok)
		{
			/*
			**  external security strength factor;
			**  we have none so zero
			*/
			ext_ssf.ssf = 0;
			ext_ssf.auth_id = NULL;
			sasl_ok = sasl_setprop(conn, SASL_SSF_EXTERNAL,
					       &ext_ssf) == SASL_OK;
		}
		if (sasl_ok)
			sasl_ok = saslmechs(conn, &mechlist, sasl_ok);
	}
	else
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "SASL error: sasl_server_new failed=%d",
				  result);
	}
# endif /* SASL */

# if _FFR_MILTER
	if (milterize)
	{
		char state;

		/* initialize mail filter connection */
		milter_init(e, &state);
		switch (state)
		{
		  case SMFIR_REJECT:
			greetcode = "554";
			nullserver = "Command rejected";
			milterize = FALSE;
			break;

		  case SMFIR_TEMPFAIL:
			tempfail = TRUE;
			milterize = FALSE;
			break;
		}
	}

	if (milterize && !bitset(EF_DISCARD, e->e_flags))
	{
		char state;
		char *response;

		response = milter_connect(peerhostname, RealHostAddr,
					  e, &state);
		switch (state)
		{
		  case SMFIR_REPLYCODE:	/* REPLYCODE shouldn't happen */
		  case SMFIR_REJECT:
			greetcode = "554";
			nullserver = "Command rejected";
			milterize = FALSE;
			break;

		  case SMFIR_DISCARD:
			e->e_flags |= EF_DISCARD;
			milterize = FALSE;
			break;

		  case SMFIR_TEMPFAIL:
			tempfail = TRUE;
			milterize = FALSE;
			break;
		}
	}
# endif /* _FFR_MILTER */

	/* output the first line, inserting "ESMTP" as second word */
	expand(SmtpGreeting, inp, sizeof inp, e);
	p = strchr(inp, '\n');
	if (p != NULL)
		*p++ = '\0';
	id = strchr(inp, ' ');
	if (id == NULL)
		id = &inp[strlen(inp)];
	if (p == NULL)
		snprintf(cmdbuf, sizeof cmdbuf,
			 "%s %%.*s ESMTP%%s", greetcode);
	else
		snprintf(cmdbuf, sizeof cmdbuf,
			 "%s-%%.*s ESMTP%%s", greetcode);
	message(cmdbuf, id - inp, inp, id);

	/* output remaining lines */
	while ((id = p) != NULL && (p = strchr(id, '\n')) != NULL)
	{
		*p++ = '\0';
		if (isascii(*id) && isspace(*id))
			id++;
		(void) snprintf(cmdbuf, sizeof cmdbuf, "%s-%%s", greetcode);
		message(cmdbuf, id);
	}
	if (id != NULL)
	{
		if (isascii(*id) && isspace(*id))
			id++;
		(void) snprintf(cmdbuf, sizeof cmdbuf, "%s %%s", greetcode);
		message(cmdbuf, id);
	}

	protocol = NULL;
	sendinghost = macvalue('s', e);
	gothello = FALSE;
	gotmail = FALSE;
	for (;;)
	{
		/* arrange for backout */
		(void) setjmp(TopFrame);
		QuickAbort = FALSE;
		HoldErrs = FALSE;
		SuprErrs = FALSE;
		LogUsrErrs = FALSE;
		OnlyOneError = TRUE;
		e->e_flags &= ~(EF_VRFYONLY|EF_GLOBALERRS);

		/* setup for the read */
		e->e_to = NULL;
		Errors = 0;
		FileName = NULL;
		(void) fflush(stdout);

		/* read the input line */
		SmtpPhase = "server cmd read";
		sm_setproctitle(TRUE, e, "server %s cmd read", CurSmtpClient);
# if SASL
		/*
		**  SMTP AUTH requires accepting any length,
		**  at least for challenge/response
		**  XXX
		*/
# endif /* SASL */

		/* handle errors */
		if (ferror(OutChannel) ||
		    (p = sfgets(inp, sizeof inp, InChannel,
				TimeOuts.to_nextcommand, SmtpPhase)) == NULL)
		{
			char *d;

			d = macvalue(macid("{daemon_name}", NULL), e);
			if (d == NULL)
				d = "stdin";
			/* end of file, just die */
			disconnect(1, e);

# if _FFR_MILTER
			/* close out milter filters */
			milter_quit(e);
# endif /* _FFR_MILTER */

			message("421 4.4.1 %s Lost input channel from %s",
				MyHostName, CurSmtpClient);
			if (LogLevel > (gotmail ? 1 : 19))
				sm_syslog(LOG_NOTICE, e->e_id,
					  "lost input channel from %.100s to %s after %s",
					  CurSmtpClient, d,
					  (c == NULL || c->cmd_name == NULL) ? "startup" : c->cmd_name);
			/*
			**  If have not accepted mail (DATA), do not bounce
			**  bad addresses back to sender.
			*/

			if (bitset(EF_CLRQUEUE, e->e_flags))
				e->e_sendqueue = NULL;
			goto doquit;
		}

		/* clean up end of line */
		fixcrlf(inp, TRUE);

# if SASL
		if (authenticating == SASL_PROC_AUTH)
		{
#  if 0
			if (*inp == '\0')
			{
				authenticating = SASL_NOT_AUTH;
				message("501 5.5.2 missing input");
				continue;
			}
#  endif /* 0 */
			if (*inp == '*' && *(inp + 1) == '\0')
			{
				authenticating = SASL_NOT_AUTH;

				/* rfc 2254 4. */
				message("501 5.0.0 AUTH aborted");
				continue;
			}

			/* could this be shorter? XXX */
			out = xalloc(strlen(inp));
			result = sasl_decode64(inp, strlen(inp), out, &outlen);
			if (result != SASL_OK)
			{
				authenticating = SASL_NOT_AUTH;

				/* rfc 2254 4. */
				message("501 5.5.4 cannot decode AUTH parameter %s",
					inp);
				continue;
			}

			result = sasl_server_step(conn,	out, outlen,
						  &out, &outlen, &errstr);

			/* get an OK if we're done */
			if (result == SASL_OK)
			{
  authenticated:
				message("235 2.0.0 OK Authenticated");
				authenticating = SASL_IS_AUTH;
				define(macid("{auth_type}", NULL),
				       newstr(auth_type), &BlankEnvelope);

				result = sasl_getprop(conn, SASL_USERNAME,
						      (void **)&user);
				if (result != SASL_OK)
				{
					user = "";
					define(macid("{auth_authen}", NULL),
					       NULL, &BlankEnvelope);
				}
				else
				{
					define(macid("{auth_authen}", NULL),
					       newstr(user), &BlankEnvelope);
				}

#  if 0
				/* get realm? */
				sasl_getprop(conn, SASL_REALM, (void **) &data);
#  endif /* 0 */


				if (LogLevel > 9)
					sm_syslog(LOG_INFO, NOQID,
						  "SASL: connection from %.64s: mech=%.16s, id=%.64s",
						  CurSmtpClient, auth_type,
						  user);
			}
			else if (result == SASL_CONTINUE)
			{
				len = ENC64LEN(outlen);
				out2 = xalloc(len);
				result = sasl_encode64(out, outlen, out2, len,
						       (u_int *)&out2len);
				if (result != SASL_OK)
				{
					/* correct code? XXX */
					/* 454 Temp. authentication failure */
					message("454 4.5.4 Internal error: unable to encode64");
					if (LogLevel > 5)
						sm_syslog(LOG_WARNING, e->e_id,
							  "SASL encode64 error [%d for \"%s\"]",
							  result, out);
					/* start over? */
					authenticating = SASL_NOT_AUTH;
				}
				else
				{
					message("334 %s", out2);
					if (tTd(95, 2))
						dprintf("SASL continue: msg='%s' len=%d\n",
							out2, out2len);
				}
			}
			else
			{
				/* not SASL_OK or SASL_CONT */
				message("500 5.7.0 authentication failed");
				if (LogLevel > 9)
					sm_syslog(LOG_WARNING, e->e_id,
						  "AUTH failure (%s): %s (%d)",
						  auth_type,
						  sasl_errstring(result, NULL,
								 NULL),
						  result);
				authenticating = SASL_NOT_AUTH;
			}
		}
		else
		{
			/* don't want to do any of this if authenticating */
# endif /* SASL */

		/* echo command to transcript */
		if (e->e_xfp != NULL)
			fprintf(e->e_xfp, "<<< %s\n", inp);

		if (LogLevel >= 15)
			sm_syslog(LOG_INFO, e->e_id,
				  "<-- %s",
				  inp);

		if (e->e_id == NULL)
			sm_setproctitle(TRUE, e, "%s: %.80s",
					CurSmtpClient, inp);
		else
			sm_setproctitle(TRUE, e, "%s %s: %.80s",
					qid_printname(e),
					CurSmtpClient, inp);

		/* break off command */
		for (p = inp; isascii(*p) && isspace(*p); p++)
			continue;
		cmd = cmdbuf;
		while (*p != '\0' &&
		       !(isascii(*p) && isspace(*p)) &&
		       cmd < &cmdbuf[sizeof cmdbuf - 2])
			*cmd++ = *p++;
		*cmd = '\0';

		/* throw away leading whitespace */
		while (isascii(*p) && isspace(*p))
			p++;

		/* decode command */
		for (c = CmdTab; c->cmd_name != NULL; c++)
		{
			if (!strcasecmp(c->cmd_name, cmdbuf))
				break;
		}

		/* reset errors */
		errno = 0;

		/*
		**  Process command.
		**
		**	If we are running as a null server, return 550
		**	to everything.
		*/

		if (nullserver != NULL || bitnset(D_ETRNONLY, d_flags))
		{
			switch (c->cmd_code)
			{
			  case CMDQUIT:
			  case CMDHELO:
			  case CMDEHLO:
			  case CMDNOOP:
			  case CMDRSET:
				/* process normally */
				break;

			  case CMDETRN:
				if (bitnset(D_ETRNONLY, d_flags) &&
				    nullserver == NULL)
					break;
				continue;

			  default:
				if (++badcommands > MAXBADCOMMANDS)
				{
					delay *= 2;
					if (delay >= MAXTIMEOUT)
						delay = MAXTIMEOUT;
					(void) sleep(delay);
				}
				if (nullserver != NULL)
				{
					if (ISSMTPREPLY(nullserver))
						usrerr(nullserver);
					else
						usrerr("550 5.0.0 %s", nullserver);
				}
				else
					usrerr("452 4.4.5 Insufficient disk space; try again later");
				continue;
			}
		}

		/* non-null server */
		switch (c->cmd_code)
		{
		  case CMDMAIL:
		  case CMDEXPN:
		  case CMDVRFY:
		  case CMDETRN:
			lognullconnection = FALSE;
		}

		switch (c->cmd_code)
		{
# if SASL
		  case CMDAUTH: /* sasl */
			if (!sasl_ok)
			{
				message("503 5.3.3 AUTH not available");
				break;
			}
			if (authenticating == SASL_IS_AUTH)
			{
				message("503 5.5.0 Already Authenticated");
				break;
			}
			if (gotmail)
			{
				message("503 5.5.0 AUTH not permitted during a mail transaction");
				break;
			}
			ismore = FALSE;

			/* make sure it's a valid string */
			for (q = p; *q != '\0' && isascii(*q); q++)
			{
				if (isspace(*q))
				{
					*q = '\0';
					while (*++q != '\0' &&
					       isascii(*q) && isspace(*q))
						continue;
					*(q - 1) = '\0';
					ismore = (*q != '\0');
					break;
				}
			}

			/* check whether mechanism is available */
			if (iteminlist(p, mechlist, " ") == NULL)
			{
				message("503 5.3.3 AUTH mechanism %s not available",
					p);
				break;
			}

			if (ismore)
			{
				/* could this be shorter? XXX */
				in = xalloc(strlen(q));
				result = sasl_decode64(q, strlen(q), in,
						       (u_int *)&inlen);
				if (result != SASL_OK)
				{
					message("501 5.5.4 cannot BASE64 decode '%s'",
						q);
					if (LogLevel > 5)
						sm_syslog(LOG_WARNING, e->e_id,
							  "SASL decode64 error [%d for \"%s\"]",
							  result, q);
					/* start over? */
					authenticating = SASL_NOT_AUTH;
					in = NULL;
					inlen = 0;
					break;
				}
#  if 0
				if (tTd(95, 99))
				{
					int i;

					dprintf("AUTH: more \"");
					for (i = 0; i < inlen; i++)
					{
						if (isascii(in[i]) &&
						    isprint(in[i]))
							dprintf("%c", in[i]);
						else
							dprintf("_");
					}
					dprintf("\"\n");
				}
#  endif /* 0 */
			}
			else
			{
				in = NULL;
				inlen = 0;
			}

			/* see if that auth type exists */
			result = sasl_server_start(conn, p, in, inlen,
						   &out, &outlen, &errstr);

			if (result != SASL_OK && result != SASL_CONTINUE)
			{
				message("500 5.7.0 authentication failed");
				if (LogLevel > 9)
					sm_syslog(LOG_ERR, e->e_id,
						  "AUTH failure (%s): %s (%d)",
						  p,
						  sasl_errstring(result, NULL,
								 NULL),
						  result);
				break;
			}
			auth_type = newstr(p);

			if (result == SASL_OK)
			{
				/* ugly, but same code */
				goto authenticated;
				/* authenticated by the initial response */
			}

			/* len is at least 2 */
			len = ENC64LEN(outlen);
			out2 = xalloc(len);
			result = sasl_encode64(out, outlen, out2, len,
					       (u_int *)&out2len);

			if (result != SASL_OK)
			{
				message("454 4.5.4 Temporary authentication failure");
				if (LogLevel > 5)
					sm_syslog(LOG_WARNING, e->e_id,
						  "SASL encode64 error [%d for \"%s\"]",
						  result, out);

				/* start over? */
				authenticating = SASL_NOT_AUTH;
			}
			else
			{
				message("334 %s", out2);
				authenticating = SASL_PROC_AUTH;
			}

			break;
# endif /* SASL */


		  case CMDHELO:		/* hello -- introduce yourself */
		  case CMDEHLO:		/* extended hello */
			if (c->cmd_code == CMDEHLO)
			{
				protocol = "ESMTP";
				SmtpPhase = "server EHLO";
			}
			else
			{
				protocol = "SMTP";
				SmtpPhase = "server HELO";
			}

			/* avoid denial-of-service */
			(void) checksmtpattack(&n_helo, MAXHELOCOMMANDS, TRUE,
					       "HELO/EHLO", e);

			/* check for duplicate HELO/EHLO per RFC 1651 4.2 */
			if (gothello)
			{
				usrerr("503 %s Duplicate HELO/EHLO",
					MyHostName);
				break;
			}

			/* check for valid domain name (re 1123 5.2.5) */
			if (*p == '\0' && !AllowBogusHELO)
			{
				usrerr("501 %s requires domain address",
					cmdbuf);
				break;
			}

			/* check for long domain name (hides Received: info) */
			if (strlen(p) > MAXNAME)
			{
				usrerr("501 Invalid domain name");
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, CurEnv->e_id,
						  "invalid domain name (too long) from %.100s",
						  CurSmtpClient);
				break;
			}

			for (q = p; *q != '\0'; q++)
			{
				if (!isascii(*q))
					break;
				if (isalnum(*q))
					continue;
				if (isspace(*q))
				{
					*q = '\0';
					break;
				}
				if (strchr("[].-_#", *q) == NULL)
					break;
			}

			if (*q == '\0')
			{
				q = "pleased to meet you";
				sendinghost = newstr(p);
			}
			else if (!AllowBogusHELO)
			{
				usrerr("501 Invalid domain name");
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, CurEnv->e_id,
						  "invalid domain name (%.100s) from %.100s",
						  p, CurSmtpClient);
				break;
			}
			else
			{
				q = "accepting invalid domain name";
			}

# if _FFR_MILTER
			if (milterize && !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				ok = TRUE;
				response = milter_helo(p, e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					milterize = FALSE;
					ok = FALSE;
					usrerr(response);
					break;

				  case SMFIR_REJECT:
					ok = FALSE;
					nullserver = "Command rejected";
					milterize = FALSE;
					usrerr("550 HELO/EHLO rejected");
					break;

				  case SMFIR_DISCARD:
					e->e_flags |= EF_DISCARD;
					milterize = FALSE;
					break;

				  case SMFIR_TEMPFAIL:
					ok = FALSE;
					tempfail = TRUE;
					milterize = FALSE;
					break;
				}
				if (response != NULL)
					free(response);
				if (!ok)
					break;
			}
# endif /* _FFR_MILTER */

			gothello = TRUE;

			/* print HELO response message */
			if (c->cmd_code != CMDEHLO)
			{
				message("250 %s Hello %s, %s",
					MyHostName, CurSmtpClient, q);
				break;
			}

			message("250-%s Hello %s, %s",
				MyHostName, CurSmtpClient, q);

			/* offer ENHSC even for nullserver */
			if (nullserver != NULL)
			{
				message("250 ENHANCEDSTATUSCODES");
				break;
			}

			/* print EHLO features list */
			message("250-ENHANCEDSTATUSCODES");
			if (!bitset(PRIV_NOEXPN, PrivacyFlags))
			{
				message("250-EXPN");
				if (!bitset(PRIV_NOVERB, PrivacyFlags))
					message("250-VERB");
			}
# if MIME8TO7
			message("250-8BITMIME");
# endif /* MIME8TO7 */
			if (MaxMessageSize > 0)
				message("250-SIZE %ld", MaxMessageSize);
			else
				message("250-SIZE");
# if DSN
			if (SendMIMEErrors &&
			    !bitset(PRIV_NORECEIPTS, PrivacyFlags))
				message("250-DSN");
# endif /* DSN */
			message("250-ONEX");
			if (!bitset(PRIV_NOETRN, PrivacyFlags) &&
			    !bitnset(D_NOETRN, d_flags))
				message("250-ETRN");
			message("250-XUSR");

# if SASL
			if (sasl_ok && mechlist != NULL && *mechlist != '\0')
				message("250-AUTH %s", mechlist);
# endif /* SASL */
			message("250 HELP");
			break;

		  case CMDMAIL:		/* mail -- designate sender */
			SmtpPhase = "server MAIL";

			/* check for validity of this command */
			if (!gothello && bitset(PRIV_NEEDMAILHELO, PrivacyFlags))
			{
				usrerr("503 5.0.0 Polite people say HELO first");
				break;
			}
			if (gotmail)
			{
				usrerr("503 5.5.0 Sender already specified");
				break;
			}
			if (InChild)
			{
				errno = 0;
				syserr("503 5.5.0 Nested MAIL command: MAIL %s", p);
				finis(TRUE, ExitStat);
			}
# if SASL
			if (bitnset(D_AUTHREQ, d_flags) &&
			    authenticating != SASL_IS_AUTH)
			{
				usrerr("530 5.7.0 Authentication required");
				break;
			}
# endif /* SASL */

			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "MAIL From:<%.100s> from %.100s tempfailed (from previous HELO/EHLO check)",
						  args[0], CurSmtpClient);
				usrerr("451 4.7.1 Please try again later");
				break;
			}
			/* make sure we know who the sending host is */
			if (sendinghost == NULL)
				sendinghost = peerhostname;

			p = skipword(p, "from");
			if (p == NULL)
				break;

			/* fork a subprocess to process this command */
			if (runinchild("SMTP-MAIL", e) > 0)
				break;
			if (Errors > 0)
				goto undo_subproc_no_pm;
			if (!gothello)
			{
				auth_warning(e,
					"%s didn't use HELO protocol",
					CurSmtpClient);
			}
# ifdef PICKY_HELO_CHECK
			if (strcasecmp(sendinghost, peerhostname) != 0 &&
			    (strcasecmp(peerhostname, "localhost") != 0 ||
			     strcasecmp(sendinghost, MyHostName) != 0))
			{
				auth_warning(e, "Host %s claimed to be %s",
					CurSmtpClient, sendinghost);
			}
# endif /* PICKY_HELO_CHECK */

			if (protocol == NULL)
				protocol = "SMTP";
			define('r', protocol, e);
			define('s', sendinghost, e);

			if (Errors > 0)
				goto undo_subproc_no_pm;
			nrcpts = 0;
			define(macid("{ntries}", NULL), "0", e);
			e->e_flags |= EF_CLRQUEUE;
			sm_setproctitle(TRUE, e, "%s %s: %.80s",
					qid_printname(e),
					CurSmtpClient, inp);

			/* child -- go do the processing */
			if (setjmp(TopFrame) > 0)
			{
				/* this failed -- undo work */
 undo_subproc_no_pm:
				e->e_flags &= ~EF_PM_NOTIFY;
 undo_subproc:
				if (InChild)
				{
					QuickAbort = FALSE;
					SuprErrs = TRUE;
					e->e_flags &= ~EF_FATALERRS;

					if (LogLevel > 4 &&
					    bitset(EF_LOGSENDER, e->e_flags))
						logsender(e, NULL);
					e->e_flags &= ~EF_LOGSENDER;

					finis(TRUE, ExitStat);
				}
				break;
			}
			QuickAbort = TRUE;

			/* must parse sender first */
			delimptr = NULL;
			setsender(p, e, &delimptr, ' ', FALSE);
			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';
			if (Errors > 0)
				goto undo_subproc_no_pm;

			/* Successfully set e_from, allow logging */
			e->e_flags |= EF_LOGSENDER;

			/* put resulting triple from parseaddr() into macros */
			if (e->e_from.q_mailer != NULL)
				 define(macid("{mail_mailer}", NULL),
					e->e_from.q_mailer->m_name, e);
			else
				 define(macid("{mail_mailer}", NULL),
					NULL, e);
			if (e->e_from.q_host != NULL)
				define(macid("{mail_host}", NULL),
				       e->e_from.q_host, e);
			else
				define(macid("{mail_host}", NULL),
				       "localhost", e);
			if (e->e_from.q_user != NULL)
				define(macid("{mail_addr}", NULL),
				       e->e_from.q_user, e);
			else
				define(macid("{mail_addr}", NULL),
				       NULL, e);
			if (Errors > 0)
			  goto undo_subproc_no_pm;

			/* check for possible spoofing */
			if (RealUid != 0 && OpMode == MD_SMTP &&
			    !wordinclass(RealUserName, 't') &&
			    (!bitnset(M_LOCALMAILER,
				      e->e_from.q_mailer->m_flags) ||
			     strcmp(e->e_from.q_user, RealUserName) != 0))
			{
				auth_warning(e, "%s owned process doing -bs",
					RealUserName);
			}

			/* now parse ESMTP arguments */
			e->e_msgsize = 0;
			addr = p;
			argno = 0;
			args[argno++] = p;
			p = delimptr;
			while (p != NULL && *p != '\0')
			{
				char *kp;
				char *vp = NULL;
				char *equal = NULL;

				/* locate the beginning of the keyword */
				while (isascii(*p) && isspace(*p))
					p++;
				if (*p == '\0')
					break;
				kp = p;

				/* skip to the value portion */
				while ((isascii(*p) && isalnum(*p)) || *p == '-')
					p++;
				if (*p == '=')
				{
					equal = p;
					*p++ = '\0';
					vp = p;

					/* skip to the end of the value */
					while (*p != '\0' && *p != ' ' &&
					       !(isascii(*p) && iscntrl(*p)) &&
					       *p != '=')
						p++;
				}

				if (*p != '\0')
					*p++ = '\0';

				if (tTd(19, 1))
					dprintf("MAIL: got arg %s=\"%s\"\n", kp,
						vp == NULL ? "<null>" : vp);

				mail_esmtp_args(kp, vp, e);
				if (equal != NULL)
					*equal = '=';
				args[argno++] = kp;
				if (argno >= MAXSMTPARGS - 1)
					usrerr("501 5.5.4 Too many parameters");
				if (Errors > 0)
					goto undo_subproc_no_pm;
			}
			args[argno] = NULL;
			if (Errors > 0)
				goto undo_subproc_no_pm;

			/* do config file checking of the sender */
			if (rscheck("check_mail", addr,
				    NULL, e, TRUE, TRUE) != EX_OK ||
			    Errors > 0)
				goto undo_subproc_no_pm;

			if (MaxMessageSize > 0 && e->e_msgsize > MaxMessageSize)
			{
				usrerr("552 5.2.3 Message size exceeds fixed maximum message size (%ld)",
					MaxMessageSize);
				goto undo_subproc_no_pm;
			}

			if (!enoughdiskspace(e->e_msgsize, TRUE))
			{
				usrerr("452 4.4.5 Insufficient disk space; try again later");
				goto undo_subproc_no_pm;
			}
			if (Errors > 0)
				goto undo_subproc_no_pm;

# if _FFR_MILTER
			if (milterize && !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_envfrom(args, e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					usrerr(response);
					break;

				  case SMFIR_REJECT:
					usrerr("550 5.7.1 Command rejected");
					break;

				  case SMFIR_DISCARD:
					e->e_flags |= EF_DISCARD;
					break;

				  case SMFIR_TEMPFAIL:
					usrerr("451 4.7.1 Try again later");
					break;
				}
				if (response != NULL)
					free(response);
			}
# endif /* _FFR_MILTER */
			if (Errors > 0)
				goto undo_subproc_no_pm;

			message("250 2.1.0 Sender ok");
			gotmail = TRUE;
			break;

		  case CMDRCPT:		/* rcpt -- designate recipient */
			if (!gotmail)
			{
				usrerr("503 5.0.0 Need MAIL before RCPT");
				break;
			}
			SmtpPhase = "server RCPT";
			if (setjmp(TopFrame) > 0)
			{
				e->e_flags &= ~(EF_FATALERRS|EF_PM_NOTIFY);
				break;
			}
			QuickAbort = TRUE;
			LogUsrErrs = TRUE;

			/* limit flooding of our machine */
			if (MaxRcptPerMsg > 0 && nrcpts >= MaxRcptPerMsg)
			{
				usrerr("452 4.5.3 Too many recipients");
				break;
			}

			if (e->e_sendmode != SM_DELIVER)
				e->e_flags |= EF_VRFYONLY;

# if _FFR_MILTER
			/*
			**  If the filter will be deleting recipients,
			**  don't expand them at RCPT time (in the call
			**  to recipient()).  If they are expanded, it
			**  is impossible for removefromlist() to figure
			**  out the expanded members of the original
			**  recipient and mark them as QS_DONTSEND.
			*/

			if (milter_can_delrcpts())
				e->e_flags |= EF_VRFYONLY;
# endif /* _FFR_MILTER */

			p = skipword(p, "to");
			if (p == NULL)
				break;
# if _FFR_ADDR_TYPE
			define(macid("{addr_type}", NULL), "e r", e);
# endif /* _FFR_ADDR_TYPE */
			a = parseaddr(p, NULLADDR, RF_COPYALL, ' ', &delimptr, e);
#if _FFR_ADDR_TYPE
			define(macid("{addr_type}", NULL), NULL, e);
#endif /* _FFR_ADDR_TYPE */
			if (Errors > 0)
				break;
			if (a == NULL)
			{
				usrerr("501 5.0.0 Missing recipient");
				break;
			}

			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';

			/* put resulting triple from parseaddr() into macros */
			if (a->q_mailer != NULL)
				define(macid("{rcpt_mailer}", NULL),
				       a->q_mailer->m_name, e);
			else
				define(macid("{rcpt_mailer}", NULL),
				       NULL, e);
			if (a->q_host != NULL)
				define(macid("{rcpt_host}", NULL),
				       a->q_host, e);
			else
				define(macid("{rcpt_host}", NULL),
				       "localhost", e);
			if (a->q_user != NULL)
				define(macid("{rcpt_addr}", NULL),
				       a->q_user, e);
			else
				define(macid("{rcpt_addr}", NULL),
				       NULL, e);
			if (Errors > 0)
				break;

			/* now parse ESMTP arguments */
			addr = p;
			argno = 0;
			args[argno++] = p;
			p = delimptr;
			while (p != NULL && *p != '\0')
			{
				char *kp;
				char *vp = NULL;
				char *equal = NULL;

				/* locate the beginning of the keyword */
				while (isascii(*p) && isspace(*p))
					p++;
				if (*p == '\0')
					break;
				kp = p;

				/* skip to the value portion */
				while ((isascii(*p) && isalnum(*p)) || *p == '-')
					p++;
				if (*p == '=')
				{
					equal = p;
					*p++ = '\0';
					vp = p;

					/* skip to the end of the value */
					while (*p != '\0' && *p != ' ' &&
					       !(isascii(*p) && iscntrl(*p)) &&
					       *p != '=')
						p++;
				}

				if (*p != '\0')
					*p++ = '\0';

				if (tTd(19, 1))
					dprintf("RCPT: got arg %s=\"%s\"\n", kp,
						vp == NULL ? "<null>" : vp);

				rcpt_esmtp_args(a, kp, vp, e);
				if (equal != NULL)
					*equal = '=';
				args[argno++] = kp;
				if (argno >= MAXSMTPARGS - 1)
					usrerr("501 5.5.4 Too many parameters");
				if (Errors > 0)
					break;
			}
			args[argno] = NULL;
			if (Errors > 0)
				break;

			/* do config file checking of the recipient */
			if (rscheck("check_rcpt", addr,
				    NULL, e, TRUE, TRUE) != EX_OK ||
			    Errors > 0)
				break;

# if _FFR_MILTER
			if (milterize && !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_envrcpt(args, e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					usrerr(response);
					break;

				  case SMFIR_REJECT:
					usrerr("550 5.7.1 Command rejected");
					break;

				  case SMFIR_DISCARD:
					e->e_flags |= EF_DISCARD;
					break;

				  case SMFIR_TEMPFAIL:
					usrerr("451 4.7.1 Try again later");
					break;
				}
				if (response != NULL)
					free(response);
			}
# endif /* _FFR_MILTER */

			define(macid("{rcpt_mailer}", NULL), NULL, e);
			define(macid("{rcpt_relay}", NULL), NULL, e);
			define(macid("{rcpt_addr}", NULL), NULL, e);
			define(macid("{dsn_notify}", NULL), NULL, e);
			if (Errors > 0)
				break;

			/* save in recipient list after ESMTP mods */
			a = recipient(a, &e->e_sendqueue, 0, e);
			if (Errors > 0)
				break;

			/* no errors during parsing, but might be a duplicate */
			e->e_to = a->q_paddr;
			if (!QS_IS_BADADDR(a->q_state))
			{
				if (e->e_queuedir == NOQDIR)
					initsys(e);
				message("250 2.1.5 Recipient ok%s",
					QS_IS_QUEUEUP(a->q_state) ?
						" (will queue)" : "");
				nrcpts++;
			}
			else
			{
				/* punt -- should keep message in ADDRESS.... */
				usrerr("550 5.1.1 Addressee unknown");
			}
			break;

		  case CMDDATA:		/* data -- text of mail */
			SmtpPhase = "server DATA";
			if (!gotmail)
			{
				usrerr("503 5.0.0 Need MAIL command");
				break;
			}
			else if (nrcpts <= 0)
			{
				usrerr("503 5.0.0 Need RCPT (recipient)");
				break;
			}

			/* put back discard bit */
			if (discard)
				e->e_flags |= EF_DISCARD;

			/* check to see if we need to re-expand aliases */
			/* also reset QS_BADADDR on already-diagnosted addrs */
			doublequeue = FALSE;
			for (a = e->e_sendqueue; a != NULL; a = a->q_next)
			{
				if (QS_IS_VERIFIED(a->q_state) &&
				    !bitset(EF_DISCARD, e->e_flags))
				{
					/* need to re-expand aliases */
					doublequeue = TRUE;
				}
				if (QS_IS_BADADDR(a->q_state))
				{
					/* make this "go away" */
					a->q_state = QS_DONTSEND;
				}
			}

			/* collect the text of the message */
			SmtpPhase = "collect";
			buffer_errors();
			collect(InChannel, TRUE, NULL, e);

# if _FFR_MILTER
			if (milterize &&
			    Errors <= 0 &&
			    !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_body(e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					usrerr(response);
					break;

				  case SMFIR_REJECT:
					usrerr("554 5.7.1 Command rejected");
					break;

				  case SMFIR_DISCARD:
					e->e_flags |= EF_DISCARD;
					break;

				  case SMFIR_TEMPFAIL:
					usrerr("451 4.7.1 Try again later");
					break;
				}
				if (response != NULL)
					free(response);
			}

			/* abort message filters that didn't get the body */
			if (milterize)
				milter_abort(e);
# endif /* _FFR_MILTER */

			/* redefine message size */
			if ((q = macvalue(macid("{msg_size}", NULL), e))
			    != NULL)
				free(q);
			snprintf(inp, sizeof inp, "%ld", e->e_msgsize);
			define(macid("{msg_size}", NULL), newstr(inp), e);
			if (Errors > 0)
			{
				/* Log who the mail would have gone to */
				if (LogLevel > 8 &&
				    e->e_message != NULL)
				{
					for (a = e->e_sendqueue;
					     a != NULL;
					     a = a->q_next)
					{
						if (!QS_IS_UNDELIVERED(a->q_state))
							continue;

						e->e_to = a->q_paddr;
						logdelivery(NULL, NULL,
							    a->q_status,
							    e->e_message,
							    NULL,
							    (time_t) 0, e);
					}
					e->e_to = NULL;
				}
				flush_errors(TRUE);
				buffer_errors();
				goto abortmessage;
			}

			/* make sure we actually do delivery */
			e->e_flags &= ~EF_CLRQUEUE;

			/* from now on, we have to operate silently */
			buffer_errors();
			e->e_errormode = EM_MAIL;

			/*
			**  Arrange to send to everyone.
			**	If sending to multiple people, mail back
			**		errors rather than reporting directly.
			**	In any case, don't mail back errors for
			**		anything that has happened up to
			**		now (the other end will do this).
			**	Truncate our transcript -- the mail has gotten
			**		to us successfully, and if we have
			**		to mail this back, it will be easier
			**		on the reader.
			**	Then send to everyone.
			**	Finally give a reply code.  If an error has
			**		already been given, don't mail a
			**		message back.
			**	We goose error returns by clearing error bit.
			*/

			SmtpPhase = "delivery";
			(void) bftruncate(e->e_xfp);
			id = e->e_id;

			if (doublequeue)
			{
				/* make sure it is in the queue */
				queueup(e, FALSE);
			}
			else
			{
				/* send to all recipients */
# if NAMED_BIND
				_res.retry = TimeOuts.res_retry[RES_TO_FIRST];
				_res.retrans = TimeOuts.res_retrans[RES_TO_FIRST];
# endif /* NAMED_BIND */
				sendall(e, SM_DEFAULT);
			}
			e->e_to = NULL;

			/* issue success message */
			message("250 2.0.0 %s Message accepted for delivery", id);

			/* if we just queued, poke it */
			if (doublequeue &&
			    e->e_sendmode != SM_QUEUE &&
			    e->e_sendmode != SM_DEFER)
			{
				CurrentLA = sm_getla(e);

				if (!shouldqueue(e->e_msgpriority, e->e_ctime))
				{
					/* close all the queue files */
					closexscript(e);
					if (e->e_dfp != NULL)
						(void) bfclose(e->e_dfp);
					e->e_dfp = NULL;
					unlockqueue(e);

					(void) dowork(e->e_queuedir, id,
						      TRUE, TRUE, e);
				}
			}

  abortmessage:
			if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
				logsender(e, NULL);
			e->e_flags &= ~EF_LOGSENDER;

			/* if in a child, pop back to our parent */
			if (InChild)
				finis(TRUE, ExitStat);

			/* clean up a bit */
			gotmail = FALSE;
			dropenvelope(e, TRUE);
			CurEnv = e = newenvelope(e, CurEnv);
			e->e_flags = BlankEnvelope.e_flags;
			break;

		  case CMDRSET:		/* rset -- reset state */
# if _FFR_MILTER
			/* abort milter filters */
			milter_abort(e);
# endif /* _FFR_MILTER */

			if (tTd(94, 100))
				message("451 4.0.0 Test failure");
			else
				message("250 2.0.0 Reset state");

			/* arrange to ignore any current send list */
			e->e_sendqueue = NULL;
			e->e_flags |= EF_CLRQUEUE;

			if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
				logsender(e, NULL);
			e->e_flags &= ~EF_LOGSENDER;

			if (InChild)
				finis(TRUE, ExitStat);

			/* clean up a bit */
			gotmail = FALSE;
			SuprErrs = TRUE;
			dropenvelope(e, TRUE);
			CurEnv = e = newenvelope(e, CurEnv);
			break;

		  case CMDVRFY:		/* vrfy -- verify address */
		  case CMDEXPN:		/* expn -- expand address */
			wt = checksmtpattack(&nverifies, MAXVRFYCOMMANDS, FALSE,
				c->cmd_code == CMDVRFY ? "VRFY" : "EXPN", e);
			previous = curtime();
			vrfy = c->cmd_code == CMDVRFY;
			if (bitset(vrfy ? PRIV_NOVRFY : PRIV_NOEXPN,
						PrivacyFlags))
			{
				if (vrfy)
					message("252 2.5.2 Cannot VRFY user; try RCPT to attempt delivery (or try finger)");
				else
					message("502 5.7.0 Sorry, we do not allow this operation");
				if (LogLevel > 5)
					sm_syslog(LOG_INFO, e->e_id,
						  "%.100s: %s [rejected]",
						  CurSmtpClient,
						  shortenstring(inp, MAXSHORTSTR));
				break;
			}
			else if (!gothello &&
				 bitset(vrfy ? PRIV_NEEDVRFYHELO : PRIV_NEEDEXPNHELO,
						PrivacyFlags))
			{
				usrerr("503 5.0.0 I demand that you introduce yourself first");
				break;
			}
			if (runinchild(vrfy ? "SMTP-VRFY" : "SMTP-EXPN", e) > 0)
				break;
			if (Errors > 0)
				goto undo_subproc;
			if (LogLevel > 5)
				sm_syslog(LOG_INFO, e->e_id,
					  "%.100s: %s",
					  CurSmtpClient,
					  shortenstring(inp, MAXSHORTSTR));
			if (setjmp(TopFrame) > 0)
				goto undo_subproc;
			QuickAbort = TRUE;
			vrfyqueue = NULL;
			if (vrfy)
				e->e_flags |= EF_VRFYONLY;
			while (*p != '\0' && isascii(*p) && isspace(*p))
				p++;
			if (*p == '\0')
			{
				usrerr("501 5.5.2 Argument required");
			}
			else
			{
				/* do config file checking of the address */
				if (rscheck(vrfy ? "check_vrfy" : "check_expn",
					    p, NULL, e, TRUE, FALSE) != EX_OK ||
				    Errors > 0)
					goto undo_subproc;
				(void) sendtolist(p, NULLADDR, &vrfyqueue, 0, e);
			}
			if (wt > 0)
				(void) sleep(wt - (curtime() - previous));
			if (Errors > 0)
				goto undo_subproc;
			if (vrfyqueue == NULL)
			{
				usrerr("554 5.5.2 Nothing to %s", vrfy ? "VRFY" : "EXPN");
			}
			while (vrfyqueue != NULL)
			{
				if (!QS_IS_UNDELIVERED(vrfyqueue->q_state))
				{
					vrfyqueue = vrfyqueue->q_next;
					continue;
				}

				/* see if there is more in the vrfy list */
				a = vrfyqueue;
				while ((a = a->q_next) != NULL &&
				       (!QS_IS_UNDELIVERED(vrfyqueue->q_state)))
					continue;
				printvrfyaddr(vrfyqueue, a == NULL, vrfy);
				vrfyqueue = a;
			}
			if (InChild)
				finis(TRUE, ExitStat);
			break;

		  case CMDETRN:		/* etrn -- force queue flush */
			if (bitset(PRIV_NOETRN, PrivacyFlags) ||
			    bitnset(D_NOETRN, d_flags))
			{
				/* different message for MSA ? */
				message("502 5.7.0 Sorry, we do not allow this operation");
				if (LogLevel > 5)
					sm_syslog(LOG_INFO, e->e_id,
						  "%.100s: %s [rejected]",
						  CurSmtpClient,
						  shortenstring(inp, MAXSHORTSTR));
				break;
			}

			if (strlen(p) <= 0)
			{
				usrerr("500 5.5.2 Parameter required");
				break;
			}

			/* crude way to avoid denial-of-service attacks */
			(void) checksmtpattack(&n_etrn, MAXETRNCOMMANDS, TRUE,
					     "ETRN", e);

			/* do config file checking of the parameter */
			if (rscheck("check_etrn", p, NULL, e, TRUE, FALSE)
			    != EX_OK || Errors > 0)
				break;

			if (LogLevel > 5)
				sm_syslog(LOG_INFO, e->e_id,
					  "%.100s: ETRN %s",
					  CurSmtpClient,
					  shortenstring(p, MAXSHORTSTR));

			id = p;
			if (*id == '@')
				id++;
			else
				*--id = '@';

			if ((new = (QUEUE_CHAR *)malloc(sizeof(QUEUE_CHAR))) == NULL)
			{
				syserr("500 5.5.0 ETRN out of memory");
				break;
			}
			new->queue_match = id;
			new->queue_next = NULL;
			QueueLimitRecipient = new;
			ok = runqueue(TRUE, FALSE);
			free(QueueLimitRecipient);
			QueueLimitRecipient = NULL;
			if (ok && Errors == 0)
				message("250 2.0.0 Queuing for node %s started", p);
			break;

		  case CMDHELP:		/* help -- give user info */
			help(p, e);
			break;

		  case CMDNOOP:		/* noop -- do nothing */
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "NOOP", e);
			message("250 2.0.0 OK");
			break;

		  case CMDQUIT:		/* quit -- leave mail */
			message("221 2.0.0 %s closing connection", MyHostName);

			/* arrange to ignore any current send list */
			e->e_sendqueue = NULL;

# if SASL
			if (authenticating == SASL_IS_AUTH)
			{
				sasl_dispose(&conn);
				authenticating = SASL_NOT_AUTH;
			}
# endif /* SASL */

doquit:
			/* avoid future 050 messages */
			disconnect(1, e);

# if _FFR_MILTER
			/* close out milter filters */
			milter_quit(e);
# endif /* _FFR_MILTER */

			if (InChild)
				ExitStat = EX_QUIT;

			if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
				logsender(e, NULL);
			e->e_flags &= ~EF_LOGSENDER;

			if (lognullconnection && LogLevel > 5)
			{
				char *d;

				d = macvalue(macid("{daemon_name}", NULL), e);
				if (d == NULL)
					d = "stdin";
				sm_syslog(LOG_INFO, NULL,
					 "%.100s did not issue MAIL/EXPN/VRFY/ETRN during connection to %s",
					  CurSmtpClient, d);
			}
			finis(TRUE, ExitStat);
			/* NOTREACHED */

		  case CMDVERB:		/* set verbose mode */
			if (bitset(PRIV_NOEXPN, PrivacyFlags) ||
			    bitset(PRIV_NOVERB, PrivacyFlags))
			{
				/* this would give out the same info */
				message("502 5.7.0 Verbose unavailable");
				break;
			}
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "VERB", e);
			Verbose = 1;
			set_delivery_mode(SM_DELIVER, e);
			message("250 2.0.0 Verbose mode");
			break;

		  case CMDONEX:		/* doing one transaction only */
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "ONEX", e);
			OneXact = TRUE;
			message("250 2.0.0 Only one transaction");
			break;

		  case CMDXUSR:		/* initial (user) submission */
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "XUSR", e);
			define(macid("{daemon_flags}", NULL), "c u", CurEnv);
			message("250 2.0.0 Initial submission");
			break;

# if SMTPDEBUG
		  case CMDDBGQSHOW:	/* show queues */
			printf("Send Queue=");
			printaddr(e->e_sendqueue, TRUE);
			break;

		  case CMDDBGDEBUG:	/* set debug mode */
			tTsetup(tTdvect, sizeof tTdvect, "0-99.1");
			tTflag(p);
			message("200 2.0.0 Debug set");
			break;

# else /* SMTPDEBUG */
		  case CMDDBGQSHOW:	/* show queues */
		  case CMDDBGDEBUG:	/* set debug mode */
# endif /* SMTPDEBUG */
		  case CMDLOGBOGUS:	/* bogus command */
			if (LogLevel > 0)
				sm_syslog(LOG_CRIT, e->e_id,
					  "\"%s\" command from %.100s (%.100s)",
					  c->cmd_name, CurSmtpClient,
					  anynet_ntoa(&RealHostAddr));
			/* FALLTHROUGH */

		  case CMDERROR:	/* unknown command */
			if (++badcommands > MAXBADCOMMANDS)
			{
				message("421 4.7.0 %s Too many bad commands; closing connection",
					MyHostName);

				/* arrange to ignore any current send list */
				e->e_sendqueue = NULL;
				goto doquit;
			}

			usrerr("500 5.5.1 Command unrecognized: \"%s\"",
			       shortenstring(inp, MAXSHORTSTR));
			break;

		  case CMDUNIMPL:
			usrerr("502 5.5.1 Command not implemented: \"%s\"",
			       shortenstring(inp, MAXSHORTSTR));
			break;

		  default:
			errno = 0;
			syserr("500 5.5.0 smtp: unknown code %d", c->cmd_code);
			break;
		}
# if SASL
		}
# endif /* SASL */
	}

}
/*
**  CHECKSMTPATTACK -- check for denial-of-service attack by repetition
**
**	Parameters:
**		pcounter -- pointer to a counter for this command.
**		maxcount -- maximum value for this counter before we
**			slow down.
**		waitnow -- sleep now (in this routine)?
**		cname -- command name for logging.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Slows down if we seem to be under attack.
*/

static time_t
checksmtpattack(pcounter, maxcount, waitnow, cname, e)
	volatile int *pcounter;
	int maxcount;
	bool waitnow;
	char *cname;
	ENVELOPE *e;
{
	if (++(*pcounter) >= maxcount)
	{
		time_t s;

		if (*pcounter == maxcount && LogLevel > 5)
		{
			sm_syslog(LOG_INFO, e->e_id,
				  "%.100s: %.40s attack?",
				  CurSmtpClient, cname);
		}
		s = 1 << (*pcounter - maxcount);
		if (s >= MAXTIMEOUT)
			s = MAXTIMEOUT;
		/* sleep at least 1 second before returning */
		(void) sleep(*pcounter / maxcount);
		s -= *pcounter / maxcount;
		if (waitnow)
		{
			(void) sleep(s);
			return(0);
		}
		return(s);
	}
	return((time_t) 0);
}
/*
**  SKIPWORD -- skip a fixed word.
**
**	Parameters:
**		p -- place to start looking.
**		w -- word to skip.
**
**	Returns:
**		p following w.
**		NULL on error.
**
**	Side Effects:
**		clobbers the p data area.
*/

static char *
skipword(p, w)
	register char *volatile p;
	char *w;
{
	register char *q;
	char *firstp = p;

	/* find beginning of word */
	while (isascii(*p) && isspace(*p))
		p++;
	q = p;

	/* find end of word */
	while (*p != '\0' && *p != ':' && !(isascii(*p) && isspace(*p)))
		p++;
	while (isascii(*p) && isspace(*p))
		*p++ = '\0';
	if (*p != ':')
	{
	  syntax:
		usrerr("501 5.5.2 Syntax error in parameters scanning \"%s\"",
			shortenstring(firstp, MAXSHORTSTR));
		return NULL;
	}
	*p++ = '\0';
	while (isascii(*p) && isspace(*p))
		p++;

	if (*p == '\0')
		goto syntax;

	/* see if the input word matches desired word */
	if (strcasecmp(q, w))
		goto syntax;

	return p;
}
/*
**  MAIL_ESMTP_ARGS -- process ESMTP arguments from MAIL line
**
**	Parameters:
**		kp -- the parameter key.
**		vp -- the value of that parameter.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

static void
mail_esmtp_args(kp, vp, e)
	char *kp;
	char *vp;
	ENVELOPE *e;
{
	if (strcasecmp(kp, "size") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 5.5.2 SIZE requires a value");
			/* NOTREACHED */
		}
		define(macid("{msg_size}", NULL), newstr(vp), e);
# if defined(__STDC__) && !defined(BROKEN_ANSI_LIBRARY)
		e->e_msgsize = strtoul(vp, (char **) NULL, 10);
# else /* defined(__STDC__) && !defined(BROKEN_ANSI_LIBRARY) */
		e->e_msgsize = strtol(vp, (char **) NULL, 10);
# endif /* defined(__STDC__) && !defined(BROKEN_ANSI_LIBRARY) */
	}
	else if (strcasecmp(kp, "body") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 5.5.2 BODY requires a value");
			/* NOTREACHED */
		}
		else if (strcasecmp(vp, "8bitmime") == 0)
		{
			SevenBitInput = FALSE;
		}
		else if (strcasecmp(vp, "7bit") == 0)
		{
			SevenBitInput = TRUE;
		}
		else
		{
			usrerr("501 5.5.4 Unknown BODY type %s",
				vp);
			/* NOTREACHED */
		}
		e->e_bodytype = newstr(vp);
	}
	else if (strcasecmp(kp, "envid") == 0)
	{
		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, ENVID not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 ENVID requires a value");
			/* NOTREACHED */
		}
		if (!xtextok(vp))
		{
			usrerr("501 5.5.4 Syntax error in ENVID parameter value");
			/* NOTREACHED */
		}
		if (e->e_envid != NULL)
		{
			usrerr("501 5.5.0 Duplicate ENVID parameter");
			/* NOTREACHED */
		}
		e->e_envid = newstr(vp);
		define(macid("{dsn_envid}", NULL), newstr(vp), e);
	}
	else if (strcasecmp(kp, "ret") == 0)
	{
		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, RET not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 RET requires a value");
			/* NOTREACHED */
		}
		if (bitset(EF_RET_PARAM, e->e_flags))
		{
			usrerr("501 5.5.0 Duplicate RET parameter");
			/* NOTREACHED */
		}
		e->e_flags |= EF_RET_PARAM;
		if (strcasecmp(vp, "hdrs") == 0)
			e->e_flags |= EF_NO_BODY_RETN;
		else if (strcasecmp(vp, "full") != 0)
		{
			usrerr("501 5.5.2 Bad argument \"%s\" to RET", vp);
			/* NOTREACHED */
		}
		define(macid("{dsn_ret}", NULL), newstr(vp), e);
	}
# if SASL
	else if (strcasecmp(kp, "auth") == 0)
	{
		int len;
		char *q;
		char *auth_param;	/* the value of the AUTH=x */
		bool saveQuickAbort = QuickAbort;
		bool saveSuprErrs = SuprErrs;
		char pbuf[256];

		if (vp == NULL)
		{
			usrerr("501 5.5.2 AUTH= requires a value");
			/* NOTREACHED */
		}
		if (e->e_auth_param != NULL)
		{
			usrerr("501 5.5.0 Duplicate AUTH parameter");
			/* NOTREACHED */
		}
		if ((q = strchr(vp, ' ')) != NULL)
			len = q - vp + 1;
		else
			len = strlen(vp) + 1;
		auth_param = xalloc(len);
		(void) strlcpy(auth_param, vp, len);
		if (!xtextok(auth_param))
		{
			usrerr("501 5.5.4 Syntax error in AUTH parameter value");
			/* just a warning? */
			/* NOTREACHED */
		}

		/* XXX this might be cut off */
		snprintf(pbuf, sizeof pbuf, "%s", xuntextify(auth_param));
		/* xalloc() the buffer instead? */

		/* XXX define this always or only if trusted? */
		define(macid("{auth_author}", NULL), newstr(pbuf), e);

		/*
		**  call Strust_auth to find out whether
		**  auth_param is acceptable (trusted)
		**  we shouldn't trust it if not authenticated
		**  (required by RFC, leave it to ruleset?)
		*/

		SuprErrs = TRUE;
		QuickAbort = FALSE;
		if (strcmp(auth_param, "<>") != 0 &&
		     (rscheck("trust_auth", pbuf, NULL, e, TRUE, FALSE)
		      != EX_OK || Errors > 0))
		{
			if (tTd(95, 8))
			{
				q = e->e_auth_param;
				dprintf("auth=\"%.100s\" not trusted user=\"%.100s\"\n",
					pbuf, (q == NULL) ? "" : q);
			}
			/* not trusted */
			e->e_auth_param = newstr("<>");
		}
		else
		{
			if (tTd(95, 8))
				dprintf("auth=\"%.100s\" trusted\n", pbuf);
			e->e_auth_param = newstr(auth_param);
		}
		free(auth_param);
		/* reset values */
		Errors = 0;
		QuickAbort = saveQuickAbort;
		SuprErrs = saveSuprErrs;
	}
# endif /* SASL */
	else
	{
		usrerr("501 5.5.4 %s parameter unrecognized", kp);
		/* NOTREACHED */
	}
}
/*
**  RCPT_ESMTP_ARGS -- process ESMTP arguments from RCPT line
**
**	Parameters:
**		a -- the address corresponding to the To: parameter.
**		kp -- the parameter key.
**		vp -- the value of that parameter.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

static void
rcpt_esmtp_args(a, kp, vp, e)
	ADDRESS *a;
	char *kp;
	char *vp;
	ENVELOPE *e;
{
	if (strcasecmp(kp, "notify") == 0)
	{
		char *p;

		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, NOTIFY not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 NOTIFY requires a value");
			/* NOTREACHED */
		}
		a->q_flags &= ~(QPINGONSUCCESS|QPINGONFAILURE|QPINGONDELAY);
		a->q_flags |= QHASNOTIFY;
		define(macid("{dsn_notify}", NULL), newstr(vp), e);

		if (strcasecmp(vp, "never") == 0)
			return;
		for (p = vp; p != NULL; vp = p)
		{
			p = strchr(p, ',');
			if (p != NULL)
				*p++ = '\0';
			if (strcasecmp(vp, "success") == 0)
				a->q_flags |= QPINGONSUCCESS;
			else if (strcasecmp(vp, "failure") == 0)
				a->q_flags |= QPINGONFAILURE;
			else if (strcasecmp(vp, "delay") == 0)
				a->q_flags |= QPINGONDELAY;
			else
			{
				usrerr("501 5.5.4 Bad argument \"%s\"  to NOTIFY",
					vp);
				/* NOTREACHED */
			}
		}
	}
	else if (strcasecmp(kp, "orcpt") == 0)
	{
		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, ORCPT not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 ORCPT requires a value");
			/* NOTREACHED */
		}
		if (strchr(vp, ';') == NULL || !xtextok(vp))
		{
			usrerr("501 5.5.4 Syntax error in ORCPT parameter value");
			/* NOTREACHED */
		}
		if (a->q_orcpt != NULL)
		{
			usrerr("501 5.5.0 Duplicate ORCPT parameter");
			/* NOTREACHED */
		}
		a->q_orcpt = newstr(vp);
	}
	else
	{
		usrerr("501 5.5.4 %s parameter unrecognized", kp);
		/* NOTREACHED */
	}
}
/*
**  PRINTVRFYADDR -- print an entry in the verify queue
**
**	Parameters:
**		a -- the address to print
**		last -- set if this is the last one.
**		vrfy -- set if this is a VRFY command.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Prints the appropriate 250 codes.
*/
#define OFFF	(3 + 1 + 5 + 1)	/* offset in fmt: SMTP reply + enh. code */

static void
printvrfyaddr(a, last, vrfy)
	register ADDRESS *a;
	bool last;
	bool vrfy;
{
	char fmtbuf[30];

	if (vrfy && a->q_mailer != NULL &&
	    !bitnset(M_VRFY250, a->q_mailer->m_flags))
		(void) strlcpy(fmtbuf, "252", sizeof fmtbuf);
	else
		(void) strlcpy(fmtbuf, "250", sizeof fmtbuf);
	fmtbuf[3] = last ? ' ' : '-';
	(void) strlcpy(&fmtbuf[4], "2.1.5 ", sizeof fmtbuf - 4);
	if (a->q_fullname == NULL)
	{
		if ((a->q_mailer == NULL ||
		     a->q_mailer->m_addrtype == NULL ||
		     strcasecmp(a->q_mailer->m_addrtype, "rfc822") == 0) &&
		    strchr(a->q_user, '@') == NULL)
			(void) strlcpy(&fmtbuf[OFFF], "<%s@%s>",
				       sizeof fmtbuf - OFFF);
		else
			(void) strlcpy(&fmtbuf[OFFF], "<%s>",
				       sizeof fmtbuf - OFFF);
		message(fmtbuf, a->q_user, MyHostName);
	}
	else
	{
		if ((a->q_mailer == NULL ||
		     a->q_mailer->m_addrtype == NULL ||
		     strcasecmp(a->q_mailer->m_addrtype, "rfc822") == 0) &&
		    strchr(a->q_user, '@') == NULL)
			(void) strlcpy(&fmtbuf[OFFF], "%s <%s@%s>",
				       sizeof fmtbuf - OFFF);
		else
			(void) strlcpy(&fmtbuf[OFFF], "%s <%s>",
				       sizeof fmtbuf - OFFF);
		message(fmtbuf, a->q_fullname, a->q_user, MyHostName);
	}
}
/*
**  RUNINCHILD -- return twice -- once in the child, then in the parent again
**
**	Parameters:
**		label -- a string used in error messages
**
**	Returns:
**		zero in the child
**		one in the parent
**
**	Side Effects:
**		none.
*/

static int
runinchild(label, e)
	char *label;
	register ENVELOPE *e;
{
	pid_t childpid;

	if (!OneXact)
	{
		extern int NumQueues;

		/*
		**  advance state of PRNG
		**  this is necessary because otherwise all child processes
		**  will produce the same PRN sequence and hence the selection
		**  of a queue directory is not "really" random.
		*/
		if (NumQueues > 1)
			(void) get_random();

		/*
		**  Disable child process reaping, in case ETRN has preceded
		**  MAIL command, and then fork.
		*/

		(void) blocksignal(SIGCHLD);

		childpid = dofork();
		if (childpid < 0)
		{
			syserr("451 4.3.0 %s: cannot fork", label);
			(void) releasesignal(SIGCHLD);
			return 1;
		}
		if (childpid > 0)
		{
			auto int st;

			/* parent -- wait for child to complete */
			sm_setproctitle(TRUE, e, "server %s child wait",
					CurSmtpClient);
			st = waitfor(childpid);
			if (st == -1)
				syserr("451 4.3.0 %s: lost child", label);
			else if (!WIFEXITED(st))
				syserr("451 4.3.0 %s: died on signal %d",
					label, st & 0177);

			/* if we exited on a QUIT command, complete the process */
			if (WEXITSTATUS(st) == EX_QUIT)
			{
				disconnect(1, e);
				finis(TRUE, ExitStat);
			}

			/* restore the child signal */
			(void) releasesignal(SIGCHLD);

			return 1;
		}
		else
		{
			/* child */
			InChild = TRUE;
			QuickAbort = FALSE;
			clearstats();
			clearenvelope(e, FALSE);
			assign_queueid(e);
			(void) setsignal(SIGCHLD, SIG_DFL);
			(void) releasesignal(SIGCHLD);
		}
	}
	return 0;
}

# if SASL

static bool
saslmechs(conn, mechlist, sasl_ok)
	sasl_conn_t *conn;
	char **mechlist;
	bool sasl_ok;
{
	int len, num, result;

	if (sasl_ok)
	{
		/* "user" is currently unused */
		result = sasl_listmech(conn, "user", /* XXX */
				       "", " ", "", mechlist,
				       (u_int *)&len, (u_int *)&num);
		if (result == SASL_OK && num > 0)
		{
			if (LogLevel > 11)
				sm_syslog(LOG_INFO, NOQID,
					  "SASL: available mech=%s, allowed mech=%s",
					  *mechlist, AuthMechanisms);
			*mechlist = intersect(AuthMechanisms, *mechlist);
		}
		else
		{
			sasl_ok = FALSE;
			if (LogLevel > 9)
				sm_syslog(LOG_WARNING, NOQID,
					  "SASL error: listmech=%d, num=%d",
					  result, num);
		}
	}
	return sasl_ok;
}

int
proxy_policy(context, auth_identity, requested_user, user, errstr)
	void *context;
	const char *auth_identity;
	const char *requested_user;
	const char **user;
	const char **errstr;
{
	if (user != NULL)
	{
		*user = newstr(auth_identity);
		return SASL_OK;
	}
	return SASL_FAIL;
}

# endif /* SASL */

#endif /* SMTP */
/*
**  HELP -- implement the HELP command.
**
**	Parameters:
**		topic -- the topic we want help for.
**		e -- envelope
**
**	Returns:
**		none.
**
**	Side Effects:
**		outputs the help file to message output.
*/
#define HELPVSTR	"#vers	"
#define HELPVERSION	2

void
help(topic, e)
	char *topic;
	ENVELOPE *e;
{
	register FILE *hf;
	register char *p;
	int len;
	bool noinfo;
	bool first = TRUE;
	long sff = SFF_OPENASROOT|SFF_REGONLY;
	char buf[MAXLINE];
	char inp[MAXLINE];
	static int foundvers = -1;
	extern char Version[];

	if (DontLockReadFiles)
		sff |= SFF_NOLOCK;
	if (!bitnset(DBS_HELPFILEINUNSAFEDIRPATH, DontBlameSendmail))
		sff |= SFF_SAFEDIRPATH;

	if (HelpFile == NULL ||
	    (hf = safefopen(HelpFile, O_RDONLY, 0444, sff)) == NULL)
	{
		/* no help */
		errno = 0;
		message("502 5.3.0 Sendmail %s -- HELP not implemented",
			Version);
		return;
	}

	if (topic == NULL || *topic == '\0')
	{
		topic = "smtp";
		noinfo = FALSE;
	}
	else
	{
		makelower(topic);
		noinfo = TRUE;
	}

	len = strlen(topic);

	while (fgets(buf, sizeof buf, hf) != NULL)
	{
		if (buf[0] == '#')
		{
			if (foundvers < 0 &&
			    strncmp(buf, HELPVSTR, strlen(HELPVSTR)) == 0)
			{
				int h;

				if (sscanf(buf + strlen(HELPVSTR), "%d",
					   &h) == 1)
					foundvers = h;
			}
			continue;
		}
		if (strncmp(buf, topic, len) == 0)
		{
			if (first)
			{
				first = FALSE;

				/* print version if no/old vers# in file */
				if (foundvers < 2 && !noinfo)
					message("214-2.0.0 This is Sendmail version %s", Version);
			}
			p = strpbrk(buf, " \t");
			if (p == NULL)
				p = buf + strlen(buf) - 1;
			else
				p++;
			fixcrlf(p, TRUE);
			if (foundvers >= 2)
			{
				translate_dollars(p);
				expand(p, inp, sizeof inp, e);
				p = inp;
			}
			message("214-2.0.0 %s", p);
			noinfo = FALSE;
		}
	}

	if (noinfo)
		message("504 5.3.0 HELP topic \"%.10s\" unknown", topic);
	else
		message("214 2.0.0 End of HELP info");

	if (foundvers != 0 && foundvers < HELPVERSION)
	{
		if (LogLevel > 1)
			sm_syslog(LOG_WARNING, e->e_id,
				  "%s too old (require version %d)",
				  HelpFile, HELPVERSION);

		/* avoid log next time */
		foundvers = 0;
	}

	(void) fclose(hf);
}
