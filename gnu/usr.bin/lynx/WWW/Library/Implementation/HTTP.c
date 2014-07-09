/*
 * $LynxId: HTTP.c,v 1.135 2014/01/11 16:52:29 tom Exp $
 *
 * HyperText Tranfer Protocol	- Client implementation		HTTP.c
 * ==========================
 * Modified:
 * 27 Jan 1994	PDM  Added Ari Luotonen's Fix for Reload when using proxy
 *		     servers.
 * 28 Apr 1997	AJL,FM Do Proxy Authorisation.
 */

#include <HTUtils.h>
#include <HTTP.h>
#include <LYUtils.h>

#ifdef USE_SSL
#include <HTNews.h>
#endif

#define HTTP_VERSION	"HTTP/1.0"

#define HTTP_PORT   80
#define HTTPS_PORT  443
#define SNEWS_PORT  563

#define INIT_LINE_SIZE		1536	/* Start with line buffer this big */
#define LINE_EXTEND_THRESH	256	/* Minimum read size */
#define VERSION_LENGTH		20	/* for returned protocol version */

#include <HTParse.h>
#include <HTTCP.h>
#include <HTFormat.h>
#include <HTFile.h>
#include <HTAlert.h>
#include <HTMIME.h>
#include <HTML.h>
#include <HTInit.h>
#include <HTAABrow.h>
#include <HTAccess.h>		/* Are we using an HTTP gateway? */

#include <LYCookie.h>
#include <LYGlobalDefs.h>
#include <GridText.h>
#include <LYStrings.h>
#include <LYUtils.h>
#include <LYrcFile.h>
#include <LYLeaks.h>

#ifdef USE_SSL
#ifdef USE_OPENSSL_INCL
#include <openssl/x509v3.h>
#endif
#ifdef USE_GNUTLS_INCL
#include <gnutls/x509.h>
#endif
#endif

BOOLEAN reloading = FALSE;	/* Reloading => send no-cache pragma to proxy */
char *redirecting_url = NULL;	/* Location: value. */
BOOL permanent_redirection = FALSE;	/* Got 301 status? */
BOOL redirect_post_content = FALSE;	/* Don't convert to GET? */

#ifdef USE_SSL
SSL_CTX *ssl_ctx = NULL;	/* SSL ctx */
SSL *SSL_handle = NULL;
static int ssl_okay;

static void free_ssl_ctx(void)
{
    if (ssl_ctx != NULL)
	SSL_CTX_free(ssl_ctx);
}

static int HTSSLCallback(int preverify_ok, X509_STORE_CTX * x509_ctx GCC_UNUSED)
{
    char *msg = NULL;
    int result = 1;

#ifdef USE_X509_SUPPORT
    HTSprintf0(&msg,
	       gettext("SSL callback:%s, preverify_ok=%d, ssl_okay=%d"),
	       X509_verify_cert_error_string((long) X509_STORE_CTX_get_error(x509_ctx)),
	       preverify_ok, ssl_okay);
    _HTProgress(msg);
    FREE(msg);
#endif

#ifndef USE_NSS_COMPAT_INCL
    if (!(preverify_ok || ssl_okay || ssl_noprompt)) {
#ifdef USE_X509_SUPPORT
	HTSprintf0(&msg, SSL_FORCED_PROMPT,
		   X509_verify_cert_error_string((long)
						 X509_STORE_CTX_get_error(x509_ctx)));
	if (HTForcedPrompt(ssl_noprompt, msg, YES))
	    ssl_okay = 1;
	else
	    result = 0;
#endif

	FREE(msg);
    }
#endif
    return result;
}

SSL *HTGetSSLHandle(void)
{
#ifdef USE_GNUTLS_INCL
    static char *certfile = NULL;
#endif

    if (ssl_ctx == NULL) {
	/*
	 * First time only.
	 */
#if SSLEAY_VERSION_NUMBER < 0x0800
	ssl_ctx = SSL_CTX_new();
	X509_set_default_verify_paths(ssl_ctx->cert);
#else
	SSLeay_add_ssl_algorithms();
	ssl_ctx = SSL_CTX_new(SSLv23_client_method());
	SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2);
#ifdef SSL_OP_NO_COMPRESSION
	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_COMPRESSION);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
	SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
	SSL_CTX_set_default_verify_paths(ssl_ctx);
	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, HTSSLCallback);
#endif /* SSLEAY_VERSION_NUMBER < 0x0800 */
#if defined(USE_PROGRAM_DIR) & !defined(USE_GNUTLS_INCL)
	{
	    X509_LOOKUP *lookup;

	    lookup = X509_STORE_add_lookup(ssl_ctx->cert_store,
					   X509_LOOKUP_file());
	    if (lookup != NULL) {
		char *certfile = NULL;

		HTSprintf0(&certfile, "%s\\cert.pem", program_dir);
		X509_LOOKUP_load_file(lookup, certfile, X509_FILETYPE_PEM);
		FREE(certfile);
	    }
	}
#endif
#ifdef USE_GNUTLS_INCL
	if ((certfile = LYGetEnv("SSL_CERT_FILE")) != NULL) {
	    CTRACE((tfp,
		    "HTGetSSLHandle: certfile is set to %s by SSL_CERT_FILE\n",
		    certfile));
	} else {
	    if (non_empty(SSL_cert_file)) {
		certfile = SSL_cert_file;
		CTRACE((tfp,
			"HTGetSSLHandle: certfile is set to %s by config SSL_CERT_FILE\n",
			certfile));
	    }
#if defined(USE_PROGRAM_DIR)
	    else {
		HTSprintf0(&(certfile), "%s\\cert.pem", program_dir);
		CTRACE((tfp,
			"HTGetSSLHandle: certfile is set to %s by installed directory\n", certfile));
	    }
#endif
	}
#endif
	atexit(free_ssl_ctx);
    }
#ifdef USE_GNUTLS_INCL
    ssl_ctx->certfile = certfile;
    ssl_ctx->certfile_type = GNUTLS_X509_FMT_PEM;
#endif
    ssl_okay = 0;
    return (SSL_new(ssl_ctx));
}

void HTSSLInitPRNG(void)
{
#if SSLEAY_VERSION_NUMBER >= 0x00905100
    if (RAND_status() == 0) {
	char rand_file[256];
	time_t t;
	long l, seed;

#ifndef _WINDOWS
	pid_t pid;

#else
	DWORD pid;
#endif

	t = time(NULL);

#ifndef _WINDOWS
	pid = getpid();
#else
	pid = GetCurrentThreadId();
#endif

	RAND_file_name(rand_file, 256L);
	CTRACE((tfp, "HTTP: Seeding PRNG\n"));
	/* Seed as much as 1024 bytes from RAND_file_name */
	RAND_load_file(rand_file, 1024L);
	/* Seed in time (mod_ssl does this) */
	RAND_seed((unsigned char *) &t, (int) sizeof(time_t));

	/* Seed in pid (mod_ssl does this) */
	RAND_seed((unsigned char *) &pid, (int) sizeof(pid));
	/* Initialize system's random number generator */
	RAND_bytes((unsigned char *) &seed, (int) sizeof(long));

	lynx_srand((unsigned) seed);
	while (RAND_status() == 0) {
	    /* Repeatedly seed the PRNG using the system's random number generator until it has been seeded with enough data */
	    l = lynx_rand();
	    RAND_seed((unsigned char *) &l, (int) sizeof(long));
	}
	/* Write a rand_file */
	RAND_write_file(rand_file);
    }
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
    return;
}

#define HTTP_NETREAD(sock, buff, size, handle) \
	(handle \
	 ? SSL_read(handle, buff, size) \
	 : NETREAD(sock, buff, size))

#define HTTP_NETWRITE(sock, buff, size, handle) \
	(handle \
	 ? SSL_write(handle, buff, size) \
	 : NETWRITE(sock, buff, size))

#define HTTP_NETCLOSE(sock, handle)  \
	{ (void)NETCLOSE(sock); \
	  if (handle) \
	      SSL_free(handle); \
	  SSL_handle = handle = NULL; \
	}

#else
#define HTTP_NETREAD(a, b, c, d)   NETREAD(a, b, c)
#define HTTP_NETWRITE(a, b, c, d)  NETWRITE(a, b, c)
#define HTTP_NETCLOSE(a, b)  (void)NETCLOSE(a)
#endif /* USE_SSL */

#ifdef _WINDOWS			/* 1997/11/06 (Thu) 13:00:08 */

#define	BOX_TITLE	"Lynx " __FILE__
#define	BOX_FLAG	(MB_ICONINFORMATION | MB_SETFOREGROUND)

typedef struct {
    int fd;
    char *buf;
    int len;
} recv_data_t;

int ws_read_per_sec = 0;
static int ws_errno = 0;

static DWORD g_total_times = 0;
static DWORD g_total_bytes = 0;

/* The same like read, but takes care of EINTR and uses select to
   timeout the stale connections.  */

static int ws_read(int fd, char *buf, int len)
{
    int res;
    int retry = 3;

    do {
	res = recv(fd, buf, len, 0);
	if (WSAEWOULDBLOCK == WSAGetLastError()) {
	    Sleep(100);
	    if (retry-- > 0)
		continue;
	}
    } while (res == SOCKET_ERROR && SOCKET_ERRNO == EINTR);

    return res;
}

#define DWORD_ERR ((DWORD)-1)

static DWORD __stdcall _thread_func(void *p)
{
    DWORD result;
    int i, val;
    recv_data_t *q = (recv_data_t *) p;

    i = 0;
    i++;
    val = ws_read(q->fd, q->buf, q->len);

    if (val == SOCKET_ERROR) {
	ws_errno = WSAGetLastError();
#if 0
	char buff[256];

	sprintf(buff, "Thread read: %d, error (%ld), fd = %d, len = %d",
		i, ws_errno, q->fd, q->len);
	MessageBox(NULL, buff, BOX_TITLE, BOX_FLAG);
#endif
	result = DWORD_ERR;
    } else {
	result = val;
    }

    return result;
}

/* The same like read, but takes care of EINTR and uses select to
   timeout the stale connections.  */

int ws_netread(int fd, char *buf, int len)
{
    int i;
    char buff[256];

    /* 1998/03/30 (Mon) 09:01:21 */
    HANDLE hThread;
    DWORD dwThreadID;
    DWORD exitcode = 0;
    DWORD ret_val = DWORD_ERR;
    DWORD val, process_time, now_TickCount, save_TickCount;

    static recv_data_t para;

#define TICK	5
#define STACK_SIZE	0x2000uL

    InitializeCriticalSection(&critSec_READ);

    para.fd = fd;
    para.buf = buf;
    para.len = len;

    ws_read_per_sec = 0;
    save_TickCount = GetTickCount();

    hThread = CreateThread(NULL, STACK_SIZE,
			   _thread_func,
			   (void *) &para, 0UL, &dwThreadID);

    if (hThread == 0) {
	HTInfoMsg("CreateThread Failed (read)");
	goto read_exit;
    }

    i = 0;
    while (1) {
	val = WaitForSingleObject(hThread, 1000 / TICK);
	i++;
	if (val == WAIT_FAILED) {
	    HTInfoMsg("Wait Failed");
	    ret_val = DWORD_ERR;
	    break;
	} else if (val == WAIT_TIMEOUT) {
	    i++;
	    if (i / TICK > (AlertSecs + 2)) {
		sprintf(buff, "Read Waiting (%2d.%01d) for %d Bytes",
			i / TICK, (i % TICK) * 10 / TICK, len);
		SetConsoleTitle(buff);
	    }
	    if (win32_check_interrupt() || ((i / TICK) > lynx_timeout)) {
		if (CloseHandle(hThread) == FALSE) {
		    HTInfoMsg("Thread terminate Failed");
		}
		WSASetLastError(ETIMEDOUT);
		ret_val = HT_INTERRUPTED;
		break;
	    }
	} else if (val == WAIT_OBJECT_0) {
	    if (GetExitCodeThread(hThread, &exitcode) == FALSE) {
		exitcode = DWORD_ERR;
	    }
	    if (CloseHandle(hThread) == FALSE) {
		HTInfoMsg("Thread terminate Failed");
	    }
	    now_TickCount = GetTickCount();
	    if (now_TickCount >= save_TickCount)
		process_time = now_TickCount - save_TickCount;
	    else
		process_time = now_TickCount + (0xffffffff - save_TickCount);

	    if (process_time == 0)
		process_time = 1;
	    g_total_times += process_time;

	    /*
	     * DWORD is unsigned, and could be an error code which is signed.
	     */
	    if ((long) exitcode > 0)
		g_total_bytes += exitcode;

	    ws_read_per_sec = g_total_bytes;
	    if (ws_read_per_sec > 2000000) {
		if (g_total_times > 1000)
		    ws_read_per_sec /= (g_total_times / 1000);
	    } else {
		ws_read_per_sec *= 1000;
		ws_read_per_sec /= g_total_times;
	    }

	    ret_val = exitcode;
	    break;
	}
    }				/* end while(1) */

  read_exit:
    LeaveCriticalSection(&critSec_READ);
    return ret_val;
}
#endif /* _WINDOWS */

/*
 * Strip any username from the given string so we retain only the host.
 */
static void strip_userid(char *host)
{
    char *p1 = host;
    char *p2 = StrChr(host, '@');
    char *fake;

    if (p2 != 0) {
	*p2++ = '\0';
	if ((fake = HTParse(host, "", PARSE_HOST)) != NULL) {
	    char *msg = NULL;

	    CTRACE((tfp, "parsed:%s\n", fake));
	    HTSprintf0(&msg, gettext("Address contains a username: %s"), host);
	    HTAlert(msg);
	    FREE(msg);
	}
	while ((*p1++ = *p2++) != '\0') {
	    ;
	}
    }
}

/*
 * Check if the user's options specified to use the given encoding.  Normally
 * all encodings with compiled-in support are specified (encodingALL).
 */
static BOOL acceptEncoding(int code)
{
    BOOL result = FALSE;

    if ((code & LYAcceptEncoding) != 0) {
	const char *program = 0;

	switch (code) {
	case encodingGZIP:
	    program = HTGetProgramPath(ppGZIP);
	    break;
	case encodingDEFLATE:
	    program = HTGetProgramPath(ppINFLATE);
	    break;
	case encodingCOMPRESS:
	    program = HTGetProgramPath(ppCOMPRESS);
	    break;
	case encodingBZIP2:
	    program = HTGetProgramPath(ppBZIP2);
	    break;
	default:
	    break;
	}
	/*
	 * FIXME:  if lynx did not rely upon external programs to decompress
	 * files for external viewers, this check could be relaxed.
	 */
	result = (BOOL) (program != 0);
    }
    return result;
}

#ifdef USE_SSL
static void show_cert_issuer(X509 * peer_cert GCC_UNUSED)
{
#if defined(USE_OPENSSL_INCL) || defined(USE_GNUTLS_FUNCS)
    char ssl_dn[1024];
    char *msg = NULL;

    X509_NAME_oneline(X509_get_issuer_name(peer_cert), ssl_dn, (int) sizeof(ssl_dn));
    HTSprintf0(&msg, gettext("Certificate issued by: %s"), ssl_dn);
    _HTProgress(msg);
    FREE(msg);
#elif defined(USE_GNUTLS_INCL)
    /* the OpenSSL "compat" code compiles but dumps core with GNU TLS */
#endif
}
#endif

/*
 * Remove IPv6 brackets (and any port-number) from the given host-string.
 */
#ifdef USE_SSL
static char *StripIpv6Brackets(char *host)
{
    int port_number;
    char *p;

    if ((p = HTParsePort(host, &port_number)) != 0)
	*p = '\0';

    if (*host == '[') {
	p = host + strlen(host) - 1;
	if (*p == ']') {
	    *p = '\0';
	    ++host;
	}
    }
    return host;
}
#endif

/*		Load Document from HTTP Server			HTLoadHTTP()
 *		==============================
 *
 *	Given a hypertext address, this routine loads a document.
 *
 *
 *  On entry,
 *	arg	is the hypertext reference of the article to be loaded.
 *
 *  On exit,
 *	returns >=0	If no error, a good socket number
 *		<0	Error.
 *
 *	The socket must be closed by the caller after the document has been
 *	read.
 *
 */
static int HTLoadHTTP(const char *arg,
		      HTParentAnchor *anAnchor,
		      HTFormat format_out,
		      HTStream *sink)
{
    static char empty[1];
    int s;			/* Socket number for returned data */
    const char *url = arg;	/* The URL which get_physical() returned */
    bstring *command = NULL;	/* The whole command */
    char *eol;			/* End of line if found */
    char *start_of_data;	/* Start of body of reply */
    int status;			/* tcp return */
    off_t bytes_already_read;
    char crlf[3];		/* A CR LF equivalent string */
    HTStream *target;		/* Unconverted data */
    HTFormat format_in;		/* Format arriving in the message */
    BOOL do_head = FALSE;	/* Whether or not we should do a head */
    BOOL do_post = FALSE;	/* ARE WE posting ? */
    const char *METHOD;

    char *line_buffer = NULL;
    char *line_kept_clean = NULL;

#ifdef SH_EX			/* FIX BUG by kaz@maczuka.hitachi.ibaraki.jp */
    int real_length_of_line = 0;
#endif
    BOOL extensions;		/* Assume good HTTP server */
    char *linebuf = NULL;
    char temp[80];
    BOOL first_Accept = TRUE;
    BOOL show_401 = FALSE;
    BOOL show_407 = FALSE;
    BOOL auth_proxy = NO;	/* Generate a proxy authorization. - AJL */

    int length, rawlength, rv;
    int server_status = 0;
    BOOL doing_redirect, already_retrying = FALSE;
    int len = 0;

#ifdef USE_SSL
    unsigned long SSLerror;
    BOOL do_connect = FALSE;	/* ARE WE going to use a proxy tunnel ? */
    BOOL did_connect = FALSE;	/* ARE WE actually using a proxy tunnel ? */
    const char *connect_url = NULL;	/* The URL being proxied */
    char *connect_host = NULL;	/* The host being proxied */
    SSL *handle = NULL;		/* The SSL handle */
    X509 *peer_cert;		/* The peer certificate */
    char ssl_dn[1024];
    char *cert_host;
    char *ssl_host;
    char *p;
    char *msg = NULL;
    int status_sslcertcheck;
    char *ssl_dn_start;
    char *ssl_all_cns = NULL;

#ifdef USE_GNUTLS_INCL
    int ret;
    unsigned tls_status;
#endif

#if SSLEAY_VERSION_NUMBER >= 0x0900
    BOOL try_tls = TRUE;
#endif /* SSLEAY_VERSION_NUMBER >= 0x0900 */
    SSL_handle = NULL;
#else
    void *handle = NULL;
#endif /* USE_SSL */

    if (anAnchor->isHEAD)
	do_head = TRUE;
    else if (anAnchor->post_data)
	do_post = TRUE;

    if (!url) {
	status = -3;
	_HTProgress(BAD_REQUEST);
	goto done;
    }
    if (!*url) {
	status = -2;
	_HTProgress(BAD_REQUEST);
	goto done;
    }
#ifdef USE_SSL
    if (using_proxy && !StrNCmp(url, "http://", 7)) {
	int portnumber;

	if ((connect_url = strstr((url + 7), "https://"))) {
	    do_connect = TRUE;
	    connect_host = HTParse(connect_url, "https", PARSE_HOST);
	    if (!HTParsePort(connect_host, &portnumber)) {
		sprintf(temp, ":%d", HTTPS_PORT);
		StrAllocCat(connect_host, temp);
	    }
	    CTRACE((tfp, "HTTP: connect_url = '%s'\n", connect_url));
	    CTRACE((tfp, "HTTP: connect_host = '%s'\n", connect_host));
	} else if ((connect_url = strstr((url + 7), "snews://"))) {
	    do_connect = TRUE;
	    connect_host = HTParse(connect_url, "snews", PARSE_HOST);
	    if (!HTParsePort(connect_host, &portnumber)) {
		sprintf(temp, ":%d", SNEWS_PORT);
		StrAllocCat(connect_host, temp);
	    }
	    CTRACE((tfp, "HTTP: connect_url = '%s'\n", connect_url));
	    CTRACE((tfp, "HTTP: connect_host = '%s'\n", connect_host));
	}
    }
#endif /* USE_SSL */

    sprintf(crlf, "%c%c", CR, LF);

    /*
     * At this point, we're talking HTTP/1.0.
     */
    extensions = YES;

  try_again:
    /*
     * All initializations are moved down here from up above, so we can start
     * over here...
     */
    eol = 0;
    length = 0;
    doing_redirect = FALSE;
    permanent_redirection = FALSE;
    redirect_post_content = FALSE;
    target = NULL;
    line_buffer = NULL;
    line_kept_clean = NULL;

#ifdef USE_SSL
    if (!StrNCmp(url, "https", 5))
	status = HTDoConnect(url, "HTTPS", HTTPS_PORT, &s);
    else
	status = HTDoConnect(url, "HTTP", HTTP_PORT, &s);
#else
    if (!StrNCmp(url, "https", 5)) {
	HTAlert(gettext("This client does not contain support for HTTPS URLs."));
	status = HT_NOT_LOADED;
	goto done;
    }
    status = HTDoConnect(arg, "HTTP", HTTP_PORT, &s);
#endif /* USE_SSL */
    if (status == HT_INTERRUPTED) {
	/*
	 * Interrupt cleanly.
	 */
	CTRACE((tfp, "HTTP: Interrupted on connect; recovering cleanly.\n"));
	_HTProgress(CONNECTION_INTERRUPTED);
	status = HT_NOT_LOADED;
	goto done;
    }
    if (status < 0) {
#ifdef _WINDOWS
	CTRACE((tfp, "HTTP: Unable to connect to remote host for `%s'\n"
		" (status = %d, sock_errno = %d).\n",
		url, status, SOCKET_ERRNO));
#else
	CTRACE((tfp,
		"HTTP: Unable to connect to remote host for `%s' (errno = %d).\n",
		url, SOCKET_ERRNO));
#endif
	HTAlert(gettext("Unable to connect to remote host."));
	status = HT_NOT_LOADED;
	goto done;
    }
#ifdef USE_SSL
  use_tunnel:
    /*
     * If this is an https document, then do the SSL stuff here.
     */
    if (did_connect || !StrNCmp(url, "https", 5)) {
	SSL_handle = handle = HTGetSSLHandle();
	SSL_set_fd(handle, s);
	/* get host we're connecting to */
	ssl_host = HTParse(url, "", PARSE_HOST);
	ssl_host = StripIpv6Brackets(ssl_host);
#if defined(USE_GNUTLS_FUNCS)
	ret = gnutls_server_name_set(handle->gnutls_state,
				     GNUTLS_NAME_DNS,
				     ssl_host, strlen(ssl_host));
	CTRACE((tfp, "...called gnutls_server_name_set(%s) ->%d\n", ssl_host, ret));
#elif SSLEAY_VERSION_NUMBER >= 0x0900
#ifndef USE_NSS_COMPAT_INCL
	if (!try_tls) {
	    handle->options |= SSL_OP_NO_TLSv1;
#if OPENSSL_VERSION_NUMBER >= 0x0090806fL && !defined(OPENSSL_NO_TLSEXT)
	} else {
	    int ret = (int) SSL_set_tlsext_host_name(handle, ssl_host);

	    CTRACE((tfp, "...called SSL_set_tlsext_host_name(%s) ->%d\n",
		    ssl_host, ret));
#endif
	}
#endif
#endif /* SSLEAY_VERSION_NUMBER >= 0x0900 */
	HTSSLInitPRNG();
	status = SSL_connect(handle);

	if (status <= 0) {
#if SSLEAY_VERSION_NUMBER >= 0x0900
	    if (try_tls) {
		_HTProgress(gettext("Retrying connection without TLS."));
		try_tls = FALSE;
		if (did_connect)
		    HTTP_NETCLOSE(s, handle);
		goto try_again;
	    } else {
		CTRACE((tfp,
			"HTTP: Unable to complete SSL handshake for '%s', SSL_connect=%d, SSL error stack dump follows\n",
			url, status));
		SSL_load_error_strings();
		while ((SSLerror = ERR_get_error()) != 0) {
		    CTRACE((tfp, "HTTP: SSL: %s\n", ERR_error_string(SSLerror, NULL)));
		}
		HTAlert("Unable to make secure connection to remote host.");
		if (did_connect)
		    HTTP_NETCLOSE(s, handle);
		status = HT_NOT_LOADED;
		goto done;
	    }
#else
	    unsigned long SSLerror;

	    CTRACE((tfp,
		    "HTTP: Unable to complete SSL handshake for '%s', SSL_connect=%d, SSL error stack dump follows\n",
		    url, status));
	    SSL_load_error_strings();
	    while ((SSLerror = ERR_get_error()) != 0) {
		CTRACE((tfp, "HTTP: SSL: %s\n", ERR_error_string(SSLerror, NULL)));
	    }
	    HTAlert("Unable to make secure connection to remote host.");
	    if (did_connect)
		HTTP_NETCLOSE(s, handle);
	    status = HT_NOT_LOADED;
	    goto done;
#endif /* SSLEAY_VERSION_NUMBER >= 0x0900 */
	}
#ifdef USE_GNUTLS_INCL
	gnutls_certificate_set_verify_flags(handle->gnutls_cred,
					    GNUTLS_VERIFY_DO_NOT_ALLOW_SAME |
					    GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);
	ret = gnutls_certificate_verify_peers2(handle->gnutls_state, &tls_status);
	if (ret < 0 || (ret == 0 &&
			tls_status & GNUTLS_CERT_SIGNER_NOT_FOUND)) {
	    int flag_continue = 1;
	    char *msg2;

	    if (ret == 0 && tls_status & GNUTLS_CERT_SIGNER_NOT_FOUND) {
		msg2 = gettext("the certificate has no known issuer");
	    } else if (tls_status & GNUTLS_CERT_SIGNER_NOT_FOUND) {
		msg2 = gettext("no issuer was found");
	    } else if (tls_status & GNUTLS_CERT_SIGNER_NOT_CA) {
		msg2 = gettext("issuer is not a CA");
	    } else if (tls_status & GNUTLS_CERT_REVOKED) {
		msg2 = gettext("the certificate has been revoked");
	    } else {
		msg2 = gettext("the certificate is not trusted");
	    }
	    HTSprintf0(&msg, SSL_FORCED_PROMPT, msg2);
	    CTRACE((tfp, "HTLoadHTTP: %s\n", msg));
	    if (!ssl_noprompt) {
		if (!HTForcedPrompt(ssl_noprompt, msg, YES)) {
		    flag_continue = 0;
		}
	    } else if (ssl_noprompt == FORCE_PROMPT_NO) {
		flag_continue = 0;
	    }
	    FREE(msg);
	    if (flag_continue == 0) {
		status = HT_NOT_LOADED;
		FREE(msg);
		goto done;
	    }
	}
#endif

	peer_cert = SSL_get_peer_certificate(handle);
#if defined(USE_OPENSSL_INCL) || defined(USE_GNUTLS_FUNCS)
	X509_NAME_oneline(X509_get_subject_name(peer_cert),
			  ssl_dn, (int) sizeof(ssl_dn));
#elif defined(USE_GNUTLS_INCL)
	X509_NAME_oneline(X509_get_subject_name(peer_cert),
			  ssl_dn + 1, (int) sizeof(ssl_dn) - 1);

	/* Iterate over DN in incompatible GnuTLS format to bring it into OpenSSL format */
	ssl_dn[0] = '/';
	ssl_dn_start = ssl_dn;
	while (*ssl_dn_start) {
	    if ((*ssl_dn_start == ',') && (*(ssl_dn_start + 1) == ' ')) {
		*ssl_dn_start++ = '/';
		if (*(p = ssl_dn_start) != 0) {
		    while ((p[0] = p[1]) != 0)
			++p;
		}
	    } else {
		ssl_dn_start++;
	    }
	}
#endif

	/*
	 * X.509 DN validation taking ALL CN fields into account
	 * (c) 2006 Thorsten Glaser <tg@mirbsd.de>
	 */

	/* initialise status information */
	status_sslcertcheck = 0;	/* 0 = no CN found in DN */
	ssl_dn_start = ssl_dn;

	/* validate all CNs found in DN */
	CTRACE((tfp, "Validating CNs in '%s'\n", ssl_dn_start));
	while ((cert_host = strstr(ssl_dn_start, "/CN=")) != NULL) {
	    status_sslcertcheck = 1;	/* 1 = could not verify CN */
	    /* start of CommonName */
	    cert_host += 4;
	    /* find next part of DistinguishedName */
	    if ((p = StrChr(cert_host, '/')) != NULL) {
		*p = '\0';
		ssl_dn_start = p;	/* yes this points to the NUL byte */
	    } else
		ssl_dn_start = NULL;
	    cert_host = StripIpv6Brackets(cert_host);

	    /* verify this CN */
	    CTRACE((tfp, "Matching\n\tssl_host  '%s'\n\tcert_host '%s'\n",
		    ssl_host, cert_host));
	    if (!strcasecomp_asterisk(ssl_host, cert_host)) {
		status_sslcertcheck = 2;	/* 2 = verified peer */
		/* I think this is cool to have in the logs -TG */
		HTSprintf0(&msg,
			   gettext("Verified connection to %s (cert=%s)"),
			   ssl_host, cert_host);
		_HTProgress(msg);
		FREE(msg);
		/* no need to continue the verification loop */
		break;
	    }

	    /* add this CN to list of failed CNs */
	    if (ssl_all_cns == NULL)
		StrAllocCopy(ssl_all_cns, "CN<");
	    else
		StrAllocCat(ssl_all_cns, ":CN<");
	    StrAllocCat(ssl_all_cns, cert_host);
	    StrAllocCat(ssl_all_cns, ">");
	    /* if we cannot retry, don't try it */
	    if (ssl_dn_start == NULL)
		break;
	    /* now retry next CN found in DN */
	    *ssl_dn_start = '/';	/* formerly NUL byte */
	}

	/* check the X.509v3 Subject Alternative Name */
#ifdef USE_GNUTLS_INCL
	if (status_sslcertcheck < 2) {
	    int i;
	    size_t size;
	    gnutls_x509_crt_t cert;
	    static char buf[2048];

	    /* import the certificate to the x509_crt format */
	    if (gnutls_x509_crt_init(&cert) == 0) {

		if (gnutls_x509_crt_import(cert, peer_cert,
					   GNUTLS_X509_FMT_DER) < 0) {
		    gnutls_x509_crt_deinit(cert);
		    goto done;
		}

		ret = 0;
		for (i = 0; !(ret < 0); i++) {
		    size = sizeof(buf);
		    ret = gnutls_x509_crt_get_subject_alt_name(cert, i, buf,
							       &size, NULL);

		    if (strcasecomp_asterisk(ssl_host, buf) == 0) {
			status_sslcertcheck = 2;
			HTSprintf0(&msg,
				   gettext("Verified connection to %s (subj=%s)"),
				   ssl_host, buf);
			_HTProgress(msg);
			FREE(msg);
			break;
		    }

		}
	    }
	}
#endif
#ifdef USE_OPENSSL_INCL
	if (status_sslcertcheck < 2) {
	    STACK_OF(GENERAL_NAME) * gens;
	    int i, numalts;
	    const GENERAL_NAME *gn;

	    gens = (STACK_OF(GENERAL_NAME) *)
		X509_get_ext_d2i(peer_cert, NID_subject_alt_name, NULL, NULL);

	    if (gens != NULL) {
		numalts = sk_GENERAL_NAME_num(gens);
		for (i = 0; i < numalts; ++i) {
		    gn = sk_GENERAL_NAME_value(gens, i);
		    if (gn->type == GEN_DNS)
			cert_host = (char *) ASN1_STRING_data(gn->d.ia5);
		    else if (gn->type == GEN_IPADD) {
			/* XXX untested -TG */
			size_t j = (size_t) ASN1_STRING_length(gn->d.ia5);

			cert_host = (char *) malloc(j + 1);
			MemCpy(cert_host, ASN1_STRING_data(gn->d.ia5), j);
			cert_host[j] = '\0';
		    } else
			continue;
		    status_sslcertcheck = 1;	/* got at least one */
		    /* verify this SubjectAltName (see above) */
		    cert_host = StripIpv6Brackets(cert_host);
		    if (!(gn->type == GEN_IPADD ? strcasecomp :
			  strcasecomp_asterisk) (ssl_host, cert_host)) {
			status_sslcertcheck = 2;
			HTSprintf0(&msg,
				   gettext("Verified connection to %s (subj=%s)"),
				   ssl_host, cert_host);
			_HTProgress(msg);
			FREE(msg);
			if (gn->type == GEN_IPADD)
			    free(cert_host);
			break;
		    }
		    /* add to list of failed CNs */
		    if (ssl_all_cns == NULL)
			StrAllocCopy(ssl_all_cns, "SAN<");
		    else
			StrAllocCat(ssl_all_cns, ":SAN<");
		    if (gn->type == GEN_DNS)
			StrAllocCat(ssl_all_cns, "DNS=");
		    else if (gn->type == GEN_IPADD)
			StrAllocCat(ssl_all_cns, "IP=");
		    StrAllocCat(ssl_all_cns, cert_host);
		    StrAllocCat(ssl_all_cns, ">");
		    if (gn->type == GEN_IPADD)
			free(cert_host);
		}
		sk_GENERAL_NAME_free(gens);
	    }
	}
#endif /* USE_OPENSSL_INCL */

	/* if an error occurred, format the appropriate message */
	if (status_sslcertcheck == 0) {
	    HTSprintf0(&msg, SSL_FORCED_PROMPT,
		       gettext("Can't find common name in certificate"));
	} else if (status_sslcertcheck == 1) {
	    HTSprintf0(&msg,
		       gettext("SSL error:host(%s)!=cert(%s)-Continue?"),
		       ssl_host, ssl_all_cns);
	}

	/* if an error occurred, let the user decide how much he trusts */
	if (status_sslcertcheck < 2) {
	    if (!HTForcedPrompt(ssl_noprompt, msg, YES)) {
		status = HT_NOT_LOADED;
		FREE(msg);
		FREE(ssl_all_cns);
		goto done;
	    }
	    HTSprintf0(&msg,
		       gettext("UNVERIFIED connection to %s (cert=%s)"),
		       ssl_host, ssl_all_cns ? ssl_all_cns : "NONE");
	    _HTProgress(msg);
	    FREE(msg);
	}

	show_cert_issuer(peer_cert);

	HTSprintf0(&msg,
		   gettext("Secure %d-bit %s (%s) HTTP connection"),
		   SSL_get_cipher_bits(handle, NULL),
		   SSL_get_cipher_version(handle),
		   SSL_get_cipher(handle));
	_HTProgress(msg);
	FREE(msg);
    }
#endif /* USE_SSL */

    /* Ask that node for the document, omitting the host name & anchor
     */
    {
	char *p1 = (HTParse(url, "", PARSE_PATH | PARSE_PUNCTUATION));

#ifdef USE_SSL
	if (do_connect) {
	    METHOD = "CONNECT";
	    BStrCopy0(command, "CONNECT ");
	} else
#endif /* USE_SSL */
	if (do_post) {
	    METHOD = "POST";
	    BStrCopy0(command, "POST ");
	} else if (do_head) {
	    METHOD = "HEAD";
	    BStrCopy0(command, "HEAD ");
	} else {
	    METHOD = "GET";
	    BStrCopy0(command, "GET ");
	}

	/*
	 * If we are using a proxy gateway don't copy in the first slash of
	 * say:  /gopher://a;lkdjfl;ajdf;lkj/;aldk/adflj so that just
	 * gopher://....  is sent.
	 */
#ifdef USE_SSL
	if (using_proxy && !did_connect) {
	    if (do_connect)
		BStrCat0(command, connect_host);
	    else
		BStrCat0(command, p1 + 1);
	}
#else
	if (using_proxy)
	    BStrCat0(command, p1 + 1);
#endif /* USE_SSL */
	else
	    BStrCat0(command, p1);
	FREE(p1);
    }
    if (extensions) {
	BStrCat0(command, " ");
	BStrCat0(command, HTTP_VERSION);
    }

    BStrCat0(command, crlf);	/* CR LF, as in rfc 977 */

    if (extensions) {
	int n, i;
	char *host = NULL;

	if ((host = HTParse(anAnchor->address, "", PARSE_HOST)) != NULL) {
	    strip_userid(host);
	    HTBprintf(&command, "Host: %s%c%c", host, CR, LF);
	    FREE(host);
	}

	if (!HTPresentations)
	    HTFormatInit();
	n = HTList_count(HTPresentations);

	first_Accept = TRUE;
	len = 0;
	for (i = 0; i < n; i++) {
	    HTPresentation *pres =
	    (HTPresentation *) HTList_objectAt(HTPresentations, i);

	    if (pres->get_accept) {
		if (pres->quality < 1.0) {
		    if (pres->maxbytes > 0) {
			sprintf(temp, ";q=%4.3f;mxb=%" PRI_off_t "",
				pres->quality, CAST_off_t (pres->maxbytes));
		    } else {
			sprintf(temp, ";q=%4.3f", pres->quality);
		    }
		} else if (pres->maxbytes > 0) {
		    sprintf(temp, ";mxb=%" PRI_off_t "", CAST_off_t (pres->maxbytes));
		} else {
		    temp[0] = '\0';
		}
		HTSprintf0(&linebuf, "%s%s%s",
			   (first_Accept ?
			    "Accept: " : ", "),
			   HTAtom_name(pres->rep),
			   temp);
		len += (int) strlen(linebuf);
		if (len > 252 && !first_Accept) {
		    BStrCat0(command, crlf);
		    HTSprintf0(&linebuf, "Accept: %s%s",
			       HTAtom_name(pres->rep),
			       temp);
		    len = (int) strlen(linebuf);
		}
		BStrCat0(command, linebuf);
		first_Accept = FALSE;
	    }
	}
	HTBprintf(&command, "%s*/*;q=0.01%c%c",
		  (first_Accept ?
		   "Accept: " : ", "), CR, LF);

	/*
	 * FIXME:  suppressing the "Accept-Encoding" in this case is done to
	 * work around limitations of the presentation logic used for the
	 * command-line "-base" option.  The remote site may transmit the
	 * document gzip'd, but the ensuing logic in HTSaveToFile() would see
	 * the mime-type as gzip rather than text/html, and not prepend the
	 * base URL.  This is less efficient than accepting the compressed data
	 * and uncompressing it, adding the base URL but is simpler than
	 * augmenting the dump's presentation logic -TD
	 */
	if (LYPrependBaseToSource && dump_output_immediately) {
	    CTRACE((tfp,
		    "omit Accept-Encoding to work-around interaction with -source\n"));
	} else {
	    char *list = 0;
	    int j, k;

	    for (j = 1; j < encodingALL; j <<= 1) {
		if (acceptEncoding(j)) {
		    for (k = 0; tbl_preferred_encoding[k].name != 0; ++k) {
			if (tbl_preferred_encoding[k].value == j) {
			    if (list != 0)
				StrAllocCat(list, ", ");
			    StrAllocCat(list, tbl_preferred_encoding[k].name);
			    break;
			}
		    }
		}
	    }

	    if (list != 0) {
		HTBprintf(&command, "Accept-Encoding: %s%c%c", list, CR, LF);
		free(list);
	    }
	}

	if (language && *language) {
	    HTBprintf(&command, "Accept-Language: %s%c%c", language, CR, LF);
	}

	if (pref_charset && *pref_charset) {
	    BStrCat0(command, "Accept-Charset: ");
	    StrAllocCopy(linebuf, pref_charset);
	    if (linebuf[strlen(linebuf) - 1] == ',')
		linebuf[strlen(linebuf) - 1] = '\0';
	    LYLowerCase(linebuf);
	    if (strstr(linebuf, "iso-8859-1") == NULL)
		StrAllocCat(linebuf, ", iso-8859-1;q=0.01");
	    if (strstr(linebuf, "us-ascii") == NULL)
		StrAllocCat(linebuf, ", us-ascii;q=0.01");
	    BStrCat0(command, linebuf);
	    HTBprintf(&command, "%c%c", CR, LF);
	}
#if 0
	/*
	 * Promote 300 (Multiple Choices) replies, if supported, over 406 (Not
	 * Acceptable) replies.  - FM
	 *
	 * This used to be done in versions 2.7 and 2.8*, but violates the
	 * specs for transparent content negotiation and has the effect that
	 * servers supporting those specs will send 300 (Multiple Choices)
	 * instead of a normal response (e.g.  200 OK), since they will assume
	 * that the client wants to make the choice.  It is not clear whether
	 * there are any servers or sites for which sending this header really
	 * improves anything.
	 *
	 * If there ever is a need to send "Negotiate:  trans" and really mean
	 * it, we should send "Negotiate:  trans,trans" or similar, since that
	 * is semantically equivalent and some servers may ignore "Negotiate:
	 * trans" as a special case when it comes from Lynx (to work around the
	 * old faulty behavior).  - kw
	 *
	 * References:
	 * RFC 2295 (see also RFC 2296), and mail to lynx-dev and
	 * new-httpd@apache.org from Koen Holtman, Jan 1999.
	 */
	if (!do_post) {
	    HTBprintf(&command, "Negotiate: trans%c%c", CR, LF);
	}
#endif /* 0 */

	/*
	 * When reloading give no-cache pragma to proxy server to make it
	 * refresh its cache.  -- Ari L.  <luotonen@dxcern.cern.ch>
	 *
	 * Also send it as a Cache-Control header for HTTP/1.1.  - FM
	 */
	if (reloading) {
	    HTBprintf(&command, "Pragma: no-cache%c%c", CR, LF);
	    HTBprintf(&command, "Cache-Control: no-cache%c%c", CR, LF);
	}

	if (LYSendUserAgent || no_useragent) {
	    if (non_empty(LYUserAgent)) {
		char *cp = LYSkipBlanks(LYUserAgent);

		/* Won't send it at all if all blank - kw */
		if (*cp != '\0')
		    HTBprintf(&command, "User-Agent: %.*s%c%c",
			      INIT_LINE_SIZE - 15, LYUserAgent, CR, LF);
	    } else {
		HTBprintf(&command, "User-Agent: %s/%s  libwww-FM/%s%c%c",
			  HTAppName ? HTAppName : "unknown",
			  HTAppVersion ? HTAppVersion : "0.0",
			  HTLibraryVersion, CR, LF);
	    }
	}

	if (personal_mail_address && !LYNoFromHeader) {
	    HTBprintf(&command, "From: %s%c%c", personal_mail_address, CR, LF);
	}

	if (!(LYUserSpecifiedURL ||
	      LYNoRefererHeader || LYNoRefererForThis) &&
	    strcmp(HTLoadedDocumentURL(), "")) {
	    const char *cp = LYRequestReferer;

	    if (!cp)
		cp = HTLoadedDocumentURL();	/* @@@ Try both? - kw */
	    BStrCat0(command, "Referer: ");
	    if (isLYNXIMGMAP(cp)) {
		char *pound = findPoundSelector(cp);
		int nn = (pound ? (int) (pound - cp) : (int) strlen(cp));

		HTSABCat(&command, cp + LEN_LYNXIMGMAP, nn);
	    } else {
		BStrCat0(command, cp);
	    }
	    HTBprintf(&command, "%c%c", CR, LF);
	} {
	    char *abspath;
	    char *docname;
	    char *hostname;
	    char *colon;
	    int portnumber;
	    char *auth, *cookie = NULL;
	    BOOL secure = (BOOL) (StrNCmp(anAnchor->address, "https", 5)
				  ? FALSE
				  : TRUE);

	    abspath = HTParse(arg, "", PARSE_PATH | PARSE_PUNCTUATION);
	    docname = HTParse(arg, "", PARSE_PATH);
	    hostname = HTParse(arg, "", PARSE_HOST);
	    if (hostname &&
		NULL != (colon = HTParsePort(hostname, &portnumber))) {
		*colon = '\0';	/* Chop off port number */
	    } else if (!StrNCmp(arg, "https", 5)) {
		portnumber = HTTPS_PORT;
	    } else {
		portnumber = HTTP_PORT;
	    }

	    /*
	     * Add Authorization, Proxy-Authorization, and/or Cookie headers,
	     * if applicable.
	     */
	    if (using_proxy) {
		/*
		 * If we are using a proxy, first determine if we should
		 * include an Authorization header and/or Cookie header for the
		 * ultimate target of this request.  - FM & AJL
		 */
		char *host2 = NULL, *path2 = NULL;
		int port2 = (StrNCmp(docname, "https", 5) ?
			     HTTP_PORT : HTTPS_PORT);

		host2 = HTParse(docname, "", PARSE_HOST);
		path2 = HTParse(docname, "", PARSE_PATH | PARSE_PUNCTUATION);
		if ((colon = HTParsePort(host2, &port2)) != NULL) {
		    /* Use non-default port number */
		    *colon = '\0';
		}

		/*
		 * This composeAuth() does file access, i.e., for the ultimate
		 * target of the request.  - AJL
		 */
		auth_proxy = NO;
		auth = HTAA_composeAuth(host2, port2, path2, auth_proxy);
		if (auth == NULL) {
		    CTRACE((tfp, "HTTP: Not sending authorization (yet).\n"));
		} else if (*auth != '\0') {
		    /*
		     * We have an Authorization header to be included.
		     */
		    HTBprintf(&command, "%s%c%c", auth, CR, LF);
		    CTRACE((tfp, "HTTP: Sending authorization: %s\n", auth));
		} else {
		    /*
		     * The user either cancelled or made a mistake with the
		     * username and password prompt.
		     */
		    if (!(traversal || dump_output_immediately) &&
			HTConfirm(CONFIRM_WO_PASSWORD)) {
			show_401 = TRUE;
		    } else {
			if (traversal || dump_output_immediately)
			    HTAlert(FAILED_NEED_PASSWD);
#ifdef USE_SSL
			if (did_connect)
			    HTTP_NETCLOSE(s, handle);
#endif /* USE_SSL */
			BStrFree(command);
			FREE(hostname);
			FREE(docname);
			FREE(abspath);
			FREE(host2);
			FREE(path2);
			status = HT_NOT_LOADED;
			goto done;
		    }
		}
		/*
		 * Add 'Cookie:' header, if it's HTTP or HTTPS document being
		 * proxied.
		 */
		if (!StrNCmp(docname, "http", 4)) {
		    cookie = LYAddCookieHeader(host2, path2, port2, secure);
		}
		FREE(host2);
		FREE(path2);
		/*
		 * The next composeAuth() will be for the proxy.  - AJL
		 */
		auth_proxy = YES;
	    } else {
		/*
		 * Add cookie for a non-proxied request.  - FM
		 */
		cookie = LYAddCookieHeader(hostname, abspath, portnumber, secure);
		auth_proxy = NO;
	    }
	    /*
	     * If we do have a cookie set, add it to the request buffer.  - FM
	     */
	    if (cookie != NULL) {
		if (*cookie != '$') {
		    /*
		     * It's a historical cookie, so signal to the server that
		     * we support modern cookies.  - FM
		     */
		    BStrCat0(command, "Cookie2: $Version=\"1\"");
		    BStrCat0(command, crlf);
		    CTRACE((tfp, "HTTP: Sending Cookie2: $Version =\"1\"\n"));
		}
		if (*cookie != '\0') {
		    /*
		     * It's not a zero-length string, so add the header.  Note
		     * that any folding of long strings has been done already
		     * in LYCookie.c.  - FM
		     */
		    BStrCat0(command, "Cookie: ");
		    BStrCat0(command, cookie);
		    BStrCat0(command, crlf);
		    CTRACE((tfp, "HTTP: Sending Cookie: %s\n", cookie));
		}
		FREE(cookie);
	    }
	    FREE(abspath);

	    /*
	     * If we are using a proxy, auth_proxy should be YES, and we check
	     * here whether we want a Proxy-Authorization header for it.  If we
	     * are not using a proxy, auth_proxy should still be NO, and we
	     * check here for whether we want an Authorization header.  - FM &
	     * AJL
	     */
	    if ((auth = HTAA_composeAuth(hostname,
					 portnumber,
					 docname,
					 auth_proxy)) != NULL &&
		*auth != '\0') {
		/*
		 * If auth is not NULL nor zero-length, it's an Authorization
		 * or Proxy-Authorization header to be included.  - FM
		 */
		HTBprintf(&command, "%s%c%c", auth, CR, LF);
		CTRACE((tfp, (auth_proxy ?
			      "HTTP: Sending proxy authorization: %s\n" :
			      "HTTP: Sending authorization: %s\n"),
			auth));
	    } else if (auth && *auth == '\0') {
		/*
		 * If auth is a zero-length string, the user either cancelled
		 * or goofed at the username and password prompt.  - FM
		 */
		if (!(traversal || dump_output_immediately) && HTConfirm(CONFIRM_WO_PASSWORD)) {
		    if (auth_proxy == TRUE) {
			show_407 = TRUE;
		    } else {
			show_401 = TRUE;
		    }
		} else {
		    if (traversal || dump_output_immediately)
			HTAlert(FAILED_NEED_PASSWD);
		    BStrFree(command);
		    FREE(hostname);
		    FREE(docname);
		    status = HT_NOT_LOADED;
		    goto done;
		}
	    } else {
		CTRACE((tfp, (auth_proxy ?
			      "HTTP: Not sending proxy authorization (yet).\n" :
			      "HTTP: Not sending authorization (yet).\n")));
	    }
	    FREE(hostname);
	    FREE(docname);
	}
    }

    if (
#ifdef USE_SSL
	   !do_connect &&
#endif /* USE_SSL */
	   do_post) {
	CTRACE((tfp, "HTTP: Doing post, content-type '%s'\n",
		anAnchor->post_content_type
		? anAnchor->post_content_type
		: "lose"));
	HTBprintf(&command, "Content-type: %s%c%c",
		  anAnchor->post_content_type
		  ? anAnchor->post_content_type
		  : "lose",
		  CR, LF);

	HTBprintf(&command, "Content-length: %d%c%c",
		  !isBEmpty(anAnchor->post_data)
		  ? BStrLen(anAnchor->post_data)
		  : 0,
		  CR, LF);

	BStrCat0(command, crlf);	/* Blank line means "end" of headers */

	BStrCat(command, anAnchor->post_data);
    } else
	BStrCat0(command, crlf);	/* Blank line means "end" of headers */

    if (TRACE) {
	CTRACE((tfp, "Writing:\n"));
	trace_bstring(command);
#ifdef USE_SSL
	CTRACE((tfp, "%s",
		(anAnchor->post_data && !do_connect ? crlf : "")));
#else
	CTRACE((tfp, "%s",
		(anAnchor->post_data ? crlf : "")));
#endif /* USE_SSL */
	CTRACE((tfp, "----------------------------------\n"));
    }

    _HTProgress(gettext("Sending HTTP request."));

#ifdef    NOT_ASCII		/* S/390 -- gil -- 0548 */
    {
	char *p2;

	for (p2 = BStrData(command);
	     p2 < BStrData(command) + BStrLen(command);
	     p2++)
	    *p2 = TOASCII(*p2);
    }
#endif /* NOT_ASCII */
    status = (int) HTTP_NETWRITE(s,
				 BStrData(command),
				 BStrLen(command),
				 handle);
    BStrFree(command);
    FREE(linebuf);
    if (status <= 0) {
	if (status == 0) {
	    CTRACE((tfp, "HTTP: Got status 0 in initial write\n"));
	    /* Do nothing. */
	} else if ((SOCKET_ERRNO == ENOTCONN ||
		    SOCKET_ERRNO == ECONNRESET ||
		    SOCKET_ERRNO == EPIPE) &&
		   !already_retrying &&
	    /* Don't retry if we're posting. */ !do_post) {
	    /*
	     * Arrrrgh, HTTP 0/1 compatibility problem, maybe.
	     */
	    CTRACE((tfp,
		    "HTTP: BONZO ON WRITE Trying again with HTTP0 request.\n"));
	    _HTProgress(RETRYING_AS_HTTP0);
	    HTTP_NETCLOSE(s, handle);
	    extensions = NO;
	    already_retrying = TRUE;
	    goto try_again;
	} else {
	    CTRACE((tfp,
		    "HTTP: Hit unexpected network WRITE error; aborting connection.\n"));
	    HTTP_NETCLOSE(s, handle);
	    status = -1;
	    HTAlert(gettext("Unexpected network write error; connection aborted."));
	    goto done;
	}
    }

    CTRACE((tfp, "HTTP: WRITE delivered OK\n"));
    _HTProgress(gettext("HTTP request sent; waiting for response."));

    /*    Read the first line of the response
     * -----------------------------------
     */
    {
	/* Get numeric status etc */
	BOOL end_of_file = NO;
	int buffer_length = INIT_LINE_SIZE;

	line_buffer = typecallocn(char, (size_t) buffer_length);

	if (line_buffer == NULL)
	    outofmem(__FILE__, "HTLoadHTTP");

	HTReadProgress(bytes_already_read = 0, (off_t) 0);
	do {			/* Loop to read in the first line */
	    /*
	     * Extend line buffer if necessary for those crazy WAIS URLs ;-)
	     */
	    if (buffer_length - length < LINE_EXTEND_THRESH) {
		buffer_length = buffer_length + buffer_length;
		line_buffer =
		    (char *) realloc(line_buffer, ((unsigned) buffer_length *
						   sizeof(char)));

		if (line_buffer == NULL)
		    outofmem(__FILE__, "HTLoadHTTP");
	    }
	    CTRACE((tfp, "HTTP: Trying to read %d\n", buffer_length - length - 1));
	    status = HTTP_NETREAD(s,
				  line_buffer + length,
				  (buffer_length - length - 1),
				  handle);
	    CTRACE((tfp, "HTTP: Read %d\n", status));
	    if (status <= 0) {
		/*
		 * Retry if we get nothing back too.
		 * Bomb out if we get nothing twice.
		 */
		if (status == HT_INTERRUPTED) {
		    CTRACE((tfp, "HTTP: Interrupted initial read.\n"));
		    _HTProgress(CONNECTION_INTERRUPTED);
		    HTTP_NETCLOSE(s, handle);
		    status = HT_NO_DATA;
		    goto clean_up;
		} else if (status < 0 &&
			   (SOCKET_ERRNO == ENOTCONN ||
#ifdef _WINDOWS			/* 1997/11/09 (Sun) 16:59:58 */
			    SOCKET_ERRNO == ETIMEDOUT ||
#endif
			    SOCKET_ERRNO == ECONNRESET ||
			    SOCKET_ERRNO == EPIPE) &&
			   !already_retrying && !do_post) {
		    /*
		     * Arrrrgh, HTTP 0/1 compability problem, maybe.
		     */
		    CTRACE((tfp,
			    "HTTP: BONZO Trying again with HTTP0 request.\n"));
		    HTTP_NETCLOSE(s, handle);
		    FREE(line_buffer);
		    FREE(line_kept_clean);

		    extensions = NO;
		    already_retrying = TRUE;
		    _HTProgress(RETRYING_AS_HTTP0);
		    goto try_again;
		}
#ifdef USE_SSL
		else if ((SSLerror = ERR_get_error()) != 0) {
		    CTRACE((tfp,
			    "HTTP: Hit unexpected network read error; aborting connection; status %d:%s.\n",
			    status, ERR_error_string(SSLerror, NULL)));
		    HTAlert(gettext("Unexpected network read error; connection aborted."));
		    HTTP_NETCLOSE(s, handle);
		    status = -1;
		    goto clean_up;
		}
#endif
		else {
		    CTRACE((tfp,
			    "HTTP: Hit unexpected network read error; aborting connection; status %d.\n",
			    status));
		    HTAlert(gettext("Unexpected network read error; connection aborted."));
		    HTTP_NETCLOSE(s, handle);
		    status = -1;
		    goto clean_up;
		}
	    }
#ifdef    NOT_ASCII		/* S/390 -- gil -- 0564 */
	    {
		char *p2;

		for (p2 = line_buffer + length;
		     p2 < line_buffer + length + status;
		     p2++)
		    *p2 = FROMASCII(*p2);
	    }
#endif /* NOT_ASCII */

	    bytes_already_read += status;
	    HTReadProgress(bytes_already_read, (off_t) 0);

#ifdef UCX			/* UCX returns -1 on EOF */
	    if (status == 0 || status == -1)
#else
	    if (status == 0)
#endif
	    {
		break;
	    }
	    line_buffer[length + status] = 0;

	    if (line_buffer) {
		FREE(line_kept_clean);
		line_kept_clean = (char *) malloc((unsigned) buffer_length *
						  sizeof(char));

		if (line_kept_clean == NULL)
		    outofmem(__FILE__, "HTLoadHTTP");
		MemCpy(line_kept_clean, line_buffer, buffer_length);
#ifdef SH_EX			/* FIX BUG by kaz@maczuka.hitachi.ibaraki.jp */
		real_length_of_line = length + status;
#endif
	    }

	    eol = StrChr(line_buffer + length, LF);
	    /* Do we *really* want to do this? */
	    if (eol && eol != line_buffer && *(eol - 1) == CR)
		*(eol - 1) = ' ';

	    length = length + status;

	    /* Do we really want to do *this*? */
	    if (eol)
		*eol = 0;	/* Terminate the line */
	}
	/* All we need is the first line of the response.  If it's a HTTP/1.0
	 * response, then the first line will be absurdly short and therefore
	 * we can safely gate the number of bytes read through this code (as
	 * opposed to below) to ~1000.
	 *
	 * Well, let's try 100.
	 */
	while (!eol && !end_of_file && bytes_already_read < 100);
    }				/* Scope of loop variables */

    /* save total length, in case we decide later to show it all - kw */
    rawlength = length;

    /*    We now have a terminated unfolded line.  Parse it.
     * --------------------------------------------------
     */
    CTRACE((tfp, "HTTP: Rx: %s\n", line_buffer));

    /*
     * Kludge to work with old buggy servers and the VMS Help gateway.  They
     * can't handle the third word, so we try again without it.
     */
    if (extensions &&		/* Old buggy server or Help gateway? */
	(0 == StrNCmp(line_buffer, "<TITLE>Bad File Request</TITLE>", 31) ||
	 0 == StrNCmp(line_buffer, "Address should begin with", 25) ||
	 0 == StrNCmp(line_buffer, "<TITLE>Help ", 12) ||
	 0 == strcmp(line_buffer,
		     "Document address invalid or access not authorised"))) {
	FREE(line_buffer);
	FREE(line_kept_clean);
	extensions = NO;
	already_retrying = TRUE;
	CTRACE((tfp, "HTTP: close socket %d to retry with HTTP0\n", s));
	HTTP_NETCLOSE(s, handle);
	/* print a progress message */
	_HTProgress(RETRYING_AS_HTTP0);
	goto try_again;
    } {
	int fields;
	char server_version[VERSION_LENGTH + 1];

	server_version[0] = 0;

	fields = sscanf(line_buffer, "%20s %d",
			server_version,
			&server_status);

	CTRACE((tfp, "HTTP: Scanned %d fields from line_buffer\n", fields));

	if (http_error_file) {	/* Make the status code externally available */
	    FILE *error_file;

#ifdef SERVER_STATUS_ONLY
	    error_file = fopen(http_error_file, TXT_W);
	    if (error_file) {	/* Managed to open the file */
		fprintf(error_file, "error=%d\n", server_status);
		fclose(error_file);
	    }
#else
	    error_file = fopen(http_error_file, TXT_A);
	    if (error_file) {	/* Managed to open the file */
		fprintf(error_file, "   URL=%s (%s)\n", url, METHOD);
		fprintf(error_file, "STATUS=%s\n", line_buffer);
		fclose(error_file);
	    }
#endif /* SERVER_STATUS_ONLY */
	}

	/*
	 * Rule out a non-HTTP/1.n reply as best we can.
	 */
	if (fields < 2 || !server_version[0] || server_version[0] != 'H' ||
	    server_version[1] != 'T' || server_version[2] != 'T' ||
	    server_version[3] != 'P' || server_version[4] != '/' ||
	    server_version[6] != '.') {
	    /*
	     * Ugh!  An HTTP0 reply,
	     */
	    HTAtom *encoding;

	    CTRACE((tfp, "--- Talking HTTP0.\n"));

	    format_in = HTFileFormat(url, &encoding, NULL);
	    /*
	     * Treat all plain text as HTML.  This sucks but its the only
	     * solution without without looking at content.
	     */
	    if (!StrNCmp(HTAtom_name(format_in), "text/plain", 10)) {
		CTRACE((tfp, "HTTP: format_in being changed to text/HTML\n"));
		format_in = WWW_HTML;
	    }
	    if (!IsUnityEnc(encoding)) {
		/*
		 * Change the format to that for "www/compressed".
		 */
		CTRACE((tfp, "HTTP: format_in is '%s',\n", HTAtom_name(format_in)));
		StrAllocCopy(anAnchor->content_type, HTAtom_name(format_in));
		StrAllocCopy(anAnchor->content_encoding, HTAtom_name(encoding));
		format_in = HTAtom_for("www/compressed");
		CTRACE((tfp, "        Treating as '%s' with encoding '%s'\n",
			"www/compressed", HTAtom_name(encoding)));
	    }

	    start_of_data = line_kept_clean;
	} else {
	    /*
	     * Set up to decode full HTTP/1.n response.  - FM
	     */
	    format_in = HTAtom_for("www/mime");
	    CTRACE((tfp, "--- Talking HTTP1.\n"));

	    /*
	     * We set start_of_data to "" when !eol here because there will be
	     * a put_block done below; we do *not* use the value of
	     * start_of_data (as a pointer) in the computation of length (or
	     * anything else) when !eol.  Otherwise, set the value of length to
	     * what we have beyond eol (i.e., beyond the status line).  - FM
	     */
	    if (eol != 0) {
		start_of_data = (eol + 1);
	    } else {
		start_of_data = empty;
	    }
	    length = (eol
		      ? length - (int) (start_of_data - line_buffer)
		      : 0);

	    /*
	     * Trim trailing spaces in line_buffer so that we can use it in
	     * messages which include the status line.  - FM
	     */
	    while (line_buffer[strlen(line_buffer) - 1] == ' ')
		line_buffer[strlen(line_buffer) - 1] = '\0';

	    /*
	     * Take appropriate actions based on the status.  - FM
	     */
	    switch (server_status / 100) {
	    case 1:
		/*
		 * HTTP/1.1 Informational statuses.
		 * 100 Continue.
		 * 101 Switching Protocols.
		 * > 101 is unknown.
		 * We should never get these, and they have only the status
		 * line and possibly other headers, so we'll deal with them by
		 * showing the full header to the user as text/plain.  - FM
		 */
		HTAlert(gettext("Got unexpected Informational Status."));
		do_head = TRUE;
		break;

	    case 2:
		/*
		 * Good:  Got MIME object!  (Successful) - FM
		 */
		if (do_head) {
		    /*
		     * If HEAD was requested, show headers (and possibly bogus
		     * body) for all 2xx status codes as text/plain - KW
		     */
		    HTProgress(line_buffer);
		    break;
		}
		switch (server_status) {
		case 204:
		    /*
		     * No Content.
		     */
		    HTAlert(line_buffer);
		    HTTP_NETCLOSE(s, handle);
		    HTNoDataOK = 1;
		    status = HT_NO_DATA;
		    goto clean_up;

		case 205:
		    /*
		     * Reset Content.  The server has fulfilled the request but
		     * nothing is returned and we should reset any form
		     * content.  We'll instruct the user to do that, and
		     * restore the current document.  - FM
		     */
		    HTAlert(gettext("Request fulfilled.  Reset Content."));
		    HTTP_NETCLOSE(s, handle);
		    status = HT_NO_DATA;
		    goto clean_up;

		case 206:
		    /*
		     * Partial Content.  We didn't send a Range so something
		     * went wrong somewhere.  Show the status message and
		     * restore the current document.  - FM
		     */
		    HTAlert(line_buffer);
		    HTTP_NETCLOSE(s, handle);
		    status = HT_NO_DATA;
		    goto clean_up;

		default:
		    /*
		     * 200 OK.
		     * 201 Created.
		     * 202 Accepted.
		     * 203 Non-Authoritative Information.
		     * > 206 is unknown.
		     * All should return something to display.
		     */
#if defined(USE_SSL) && !defined(DISABLE_NEWS)
		    if (do_connect) {
			CTRACE((tfp,
				"HTTP: Proxy tunnel to '%s' established.\n",
				connect_host));
			do_connect = FALSE;
			url = connect_url;
			FREE(line_buffer);
			FREE(line_kept_clean);
			if (!StrNCmp(connect_url, "snews", 5)) {
			    CTRACE((tfp,
				    "      Will attempt handshake and snews connection.\n"));
			    status = HTNewsProxyConnect(s, url, anAnchor,
							format_out, sink);
			    goto done;
			}
			did_connect = TRUE;
			already_retrying = TRUE;
			eol = 0;
			length = 0;
			doing_redirect = FALSE;
			permanent_redirection = FALSE;
			target = NULL;
			CTRACE((tfp,
				"      Will attempt handshake and resubmit headers.\n"));
			goto use_tunnel;
		    }
#endif /* USE_SSL */
		    HTProgress(line_buffer);
		}		/* case 2 switch */
		break;

	    case 3:
		/*
		 * Various forms of Redirection.  - FM
		 * 300 Multiple Choices.
		 * 301 Moved Permanently.
		 * 302 Found (temporary; we can, and do, use GET).
		 * 303 See Other (temporary; always use GET).
		 * 304 Not Modified.
		 * 305 Use Proxy.
		 * 306 Set Proxy.
		 * 307 Temporary Redirect with method retained.
		 * > 308 is unknown.
		 */
		if (no_url_redirection || do_head || keep_mime_headers) {
		    /*
		     * If any of these flags are set, we do not redirect, but
		     * instead show what was returned to the user as
		     * text/plain.  - FM
		     */
		    HTProgress(line_buffer);
		    break;
		}

		if (server_status == 300) {	/* Multiple Choices */
		    /*
		     * For client driven content negotiation.  The server
		     * should be sending some way for the user-agent to make a
		     * selection, so we'll show the user whatever the server
		     * returns.  There might be a Location:  header with the
		     * server's preference present, but the choice should be up
		     * to the user, someday based on an Alternates:  header,
		     * and a body always should be present with descriptions
		     * and links for the choices (i.e., we use the latter, for
		     * now).  - FM
		     */
		    HTAlert(line_buffer);
		    if (traversal) {
			HTTP_NETCLOSE(s, handle);
			status = -1;
			goto clean_up;
		    }
		    if (!dump_output_immediately &&
			format_out == HTAtom_for("www/download")) {
			/*
			 * Convert a download request to a presentation request
			 * for interactive users.  - FM
			 */
			format_out = WWW_PRESENT;
		    }
		    break;
		}

		if (server_status == 304) {	/* Not Modified */
		    /*
		     * We didn't send an "If-Modified-Since" header, so this
		     * status is inappropriate.  We'll deal with it by showing
		     * the full header to the user as text/plain.  - FM
		     */
		    HTAlert(gettext("Got unexpected 304 Not Modified status."));
		    do_head = TRUE;
		    break;
		}

		if (server_status == 305 ||
		    server_status == 306 ||
		    server_status > 307) {
		    /*
		     * Show user the content, if any, for 305, 306, or unknown
		     * status.  - FM
		     */
		    HTAlert(line_buffer);
		    if (traversal) {
			HTTP_NETCLOSE(s, handle);
			status = -1;
			goto clean_up;
		    }
		    if (!dump_output_immediately &&
			format_out == HTAtom_for("www/download")) {
			/*
			 * Convert a download request to a presentation request
			 * for interactive users.  - FM
			 */
			format_out = WWW_PRESENT;
		    }
		    break;
		}

		/*
		 * We do not load the file, but read the headers for the
		 * "Location:", check out that redirecting_url and if it's
		 * acceptible (e.g., not a telnet URL when we have that
		 * disabled), initiate a new fetch.  If that's another
		 * redirecting_url, we'll repeat the checks, and fetch
		 * initiations if acceptible, until we reach the actual URL, or
		 * the redirection limit set in HTAccess.c is exceeded.  If the
		 * status was 301 indicating that the relocation is permanent,
		 * we set the permanent_redirection flag to make it permanent
		 * for the current anchor tree (i.e., will persist until the
		 * tree is freed or the client exits).  If the redirection
		 * would include POST content, we seek confirmation from an
		 * interactive user, with option to use 303 for 301 (but not
		 * for 307), and otherwise refuse the redirection.  We also
		 * don't allow permanent redirection if we keep POST content.
		 * If we don't find the Location header or it's value is
		 * zero-length, we display whatever the server returned, and
		 * the user should RELOAD that to try again, or make a
		 * selection from it if it contains links, or Left-Arrow to the
		 * previous document.  - FM
		 */
		{
		    if ((dump_output_immediately || traversal) &&
			do_post &&
			server_status != 303 &&
			server_status != 302 &&
			server_status != 301) {
			/*
			 * Don't redirect POST content without approval from an
			 * interactive user.  - FM
			 */
			HTTP_NETCLOSE(s, handle);
			status = -1;
			HTAlert(gettext("Redirection of POST content requires user approval."));
			if (traversal)
			    HTProgress(line_buffer);
			goto clean_up;
		    }

		    HTProgress(line_buffer);
		    if (server_status == 301) {		/* Moved Permanently */
			if (do_post) {
			    /*
			     * Don't make the redirection permanent if we have
			     * POST content.  - FM
			     */
			    CTRACE((tfp,
				    "HTTP: Have POST content.  Treating 301 (Permanent) as Temporary.\n"));
			    HTAlert(gettext("Have POST content.  Treating Permanent Redirection as Temporary.\n"));
			} else {
			    permanent_redirection = TRUE;
			}
		    }
		    doing_redirect = TRUE;

		    break;
		}

	    case 4:
		/*
		 * "I think I goofed!" (Client Error) - FM
		 */
		switch (server_status) {
		case 401:	/* Unauthorized */
		    /*
		     * Authorization for origin server required.  If show_401
		     * is set, proceed to showing the 401 body.  Otherwise, if
		     * we can set up authorization based on the
		     * WWW-Authenticate header, and the user provides a
		     * username and password, try again.  Otherwise, check
		     * whether to show the 401 body or restore the current
		     * document - FM
		     */
		    if (show_401)
			break;
		    if (HTAA_shouldRetryWithAuth(start_of_data, (size_t)
						 length, s, NO)) {

			HTTP_NETCLOSE(s, handle);
			if (dump_output_immediately && !authentication_info[0]) {
			    fprintf(stderr,
				    "HTTP: Access authorization required.\n");
			    fprintf(stderr,
				    "       Use the -auth=id:pw parameter.\n");
			    status = HT_NO_DATA;
			    goto clean_up;
			}

			CTRACE((tfp, "%s %d %s\n",
				"HTTP: close socket", s,
				"to retry with Access Authorization"));

			_HTProgress(gettext("Retrying with access authorization information."));
			FREE(line_buffer);
			FREE(line_kept_clean);
#ifdef USE_SSL
			if (using_proxy && !StrNCmp(url, "https://", 8)) {
			    url = arg;
			    do_connect = TRUE;
			    did_connect = FALSE;
			}
#endif /* USE_SSL */
			goto try_again;
		    } else if (!(traversal || dump_output_immediately) &&
			       HTConfirm(gettext("Show the 401 message body?"))) {
			break;
		    } else {
			if (traversal || dump_output_immediately)
			    HTAlert(FAILED_RETRY_WITH_AUTH);
			HTTP_NETCLOSE(s, handle);
			status = -1;
			goto clean_up;
		    }

		case 407:
		    /*
		     * Authorization for proxy server required.  If we are not
		     * in fact using a proxy, or show_407 is set, proceed to
		     * showing the 407 body.  Otherwise, if we can set up
		     * authorization based on the Proxy-Authenticate header,
		     * and the user provides a username and password, try
		     * again.  Otherwise, check whether to show the 401 body or
		     * restore the current document.  - FM & AJL
		     */
		    if (!using_proxy || show_407)
			break;
		    if (HTAA_shouldRetryWithAuth(start_of_data, (size_t)
						 length, s, YES)) {

			HTTP_NETCLOSE(s, handle);
			if (dump_output_immediately && !proxyauth_info[0]) {
			    fprintf(stderr,
				    "HTTP: Proxy authorization required.\n");
			    fprintf(stderr,
				    "       Use the -pauth=id:pw parameter.\n");
			    status = HT_NO_DATA;
			    goto clean_up;
			}

			CTRACE((tfp, "%s %d %s\n",
				"HTTP: close socket", s,
				"to retry with Proxy Authorization"));

			_HTProgress(HTTP_RETRY_WITH_PROXY);
			FREE(line_buffer);
			FREE(line_kept_clean);
			goto try_again;
		    } else if (!(traversal || dump_output_immediately) &&
			       HTConfirm(gettext("Show the 407 message body?"))) {
			if (!dump_output_immediately &&
			    format_out == HTAtom_for("www/download")) {
			    /*
			     * Convert a download request to a presentation
			     * request for interactive users.  - FM
			     */
			    format_out = WWW_PRESENT;
			}
			break;
		    } else {
			if (traversal || dump_output_immediately)
			    HTAlert(FAILED_RETRY_WITH_PROXY);
			HTTP_NETCLOSE(s, handle);
			status = -1;
			goto clean_up;
		    }

		case 408:
		    /*
		     * Request Timeout.  Show the status message and restore
		     * the current document.  - FM
		     */
		    HTAlert(line_buffer);
		    HTTP_NETCLOSE(s, handle);
		    status = HT_NO_DATA;
		    goto clean_up;

		default:
		    /*
		     * 400 Bad Request.
		     * 402 Payment Required.
		     * 403 Forbidden.
		     * 404 Not Found.
		     * 405 Method Not Allowed.
		     * 406 Not Acceptable.
		     * 409 Conflict.
		     * 410 Gone.
		     * 411 Length Required.
		     * 412 Precondition Failed.
		     * 413 Request Entity Too Large.
		     * 414 Request-URI Too Long.
		     * 415 Unsupported Media Type.
		     * 416 List Response (for content negotiation).
		     * > 416 is unknown.
		     * Show the status message, and display the returned text
		     * if we are not doing a traversal.  - FM
		     */
		    HTAlert(line_buffer);
		    if (traversal) {
			HTTP_NETCLOSE(s, handle);
			status = -1;
			goto clean_up;
		    }
		    if (!dump_output_immediately &&
			format_out == HTAtom_for("www/download")) {
			/*
			 * Convert a download request to a presentation request
			 * for interactive users.  - FM
			 */
			format_out = WWW_PRESENT;
		    }
		    break;
		}		/* case 4 switch */
		break;

	    case 5:
		/*
		 * "I think YOU goofed!" (server error)
		 * 500 Internal Server Error
		 * 501 Not Implemented
		 * 502 Bad Gateway
		 * 503 Service Unavailable
		 * 504 Gateway Timeout
		 * 505 HTTP Version Not Supported
		 * > 505 is unknown.
		 * Should always include a message, which we always should
		 * display.  - FM
		 */
		HTAlert(line_buffer);
		if (traversal) {
		    HTTP_NETCLOSE(s, handle);
		    status = -1;
		    goto clean_up;
		}
		if (!dump_output_immediately &&
		    format_out == HTAtom_for("www/download")) {
		    /*
		     * Convert a download request to a presentation request for
		     * interactive users.  - FM
		     */
		    format_out = WWW_PRESENT;
		}
		break;

	    default:
		/*
		 * Bad or unknown server_status number.  Take a chance and hope
		 * there is something to display.  - FM
		 */
		HTAlert(gettext("Unknown status reply from server!"));
		HTAlert(line_buffer);
		if (traversal) {
		    HTTP_NETCLOSE(s, handle);
		    status = -1;
		    goto clean_up;
		}
		if (!dump_output_immediately &&
		    format_out == HTAtom_for("www/download")) {
		    /*
		     * Convert a download request to a presentation request for
		     * interactive users.  - FM
		     */
		    format_out = WWW_PRESENT;
		}
		break;
	    }			/* Switch on server_status/100 */

	}			/* Full HTTP reply */
    }				/* scope of fields */

    /*
     * The user may have pressed the 'z'ap key during the pause caused by one
     * of the HTAlerts above if the server reported an error, to skip loading
     * of the error response page.  Checking here before setting up the stream
     * stack and feeding it data avoids doing unnecessary work, it also can
     * avoid unnecessarily pushing a loaded document out of the cache to make
     * room for the unwanted error page.  - kw
     */
    if (HTCheckForInterrupt()) {
	HTTP_NETCLOSE(s, handle);
	if (doing_redirect) {
	    /*
	     * Impatient user.  - FM
	     */
	    CTRACE((tfp, "HTTP: Interrupted followup read.\n"));
	    _HTProgress(CONNECTION_INTERRUPTED);
	}
	status = HT_INTERRUPTED;
	goto clean_up;
    }
    /*
     * Set up the stream stack to handle the body of the message.
     */
    if (do_head || keep_mime_headers) {
	/*
	 * It was a HEAD request, or we want the headers and source.
	 */
	start_of_data = line_kept_clean;
#ifdef SH_EX			/* FIX BUG by kaz@maczuka.hitachi.ibaraki.jp */
/* GIF file contains \0, so strlen does not return the data length */
	length = real_length_of_line;
#else
	length = rawlength;
#endif
	format_in = HTAtom_for("text/plain");

    } else if (doing_redirect) {

	format_in = HTAtom_for("message/x-http-redirection");
	StrAllocCopy(anAnchor->content_type, HTAtom_name(format_in));
	if (traversal) {
	    format_out = WWW_DEBUG;
	    if (!sink)
		sink = HTErrorStream();
	} else if (!dump_output_immediately &&
		   format_out == HTAtom_for("www/download")) {
	    /*
	     * Convert a download request to a presentation request for
	     * interactive users.  - FM
	     */
	    format_out = WWW_PRESENT;
	}
    }

    target = HTStreamStack(format_in,
			   format_out,
			   sink, anAnchor);

    if (target == NULL) {
	char *buffer = NULL;

	HTTP_NETCLOSE(s, handle);
	HTSprintf0(&buffer, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(format_in), HTAtom_name(format_out));
	_HTProgress(buffer);
	FREE(buffer);
	status = -1;
	goto clean_up;
    }

    /*
     * Recycle the first chunk of data, in all cases.
     */
    (*target->isa->put_block) (target, start_of_data, length);

    /*
     * Go pull the bulk of the data down.
     */
    rv = HTCopy(anAnchor, s, (void *) handle, target);

    /*
     * If we get here with doing_redirect set, it means that we were looking
     * for a Location header.  We either have got it now in redirecting_url -
     * in that case the stream should not have loaded any data.  Or we didn't
     * get it, in that case the stream may have presented the message body
     * normally.  - kw
     */

    if (rv == -1) {
	/*
	 * Intentional interrupt before data were received, not an error
	 */
	if (doing_redirect && traversal)
	    status = -1;
	else
	    status = HT_INTERRUPTED;
	HTTP_NETCLOSE(s, handle);
	goto clean_up;
    }

    if (rv == -2) {
	/*
	 * Aw hell, a REAL error, maybe cuz it's a dumb HTTP0 server
	 */
	(*target->isa->_abort) (target, NULL);
	if (doing_redirect && redirecting_url) {
	    /*
	     * Got a location before the error occurred?  Then consider it an
	     * interrupt but proceed below as normal.  - kw
	     */
	    /* do nothing here */
	} else {
	    HTTP_NETCLOSE(s, handle);
	    if (!doing_redirect && !already_retrying && !do_post) {
		CTRACE((tfp, "HTTP: Trying again with HTTP0 request.\n"));
		/*
		 * May as well consider it an interrupt -- right?
		 */
		FREE(line_buffer);
		FREE(line_kept_clean);
		extensions = NO;
		already_retrying = TRUE;
		_HTProgress(RETRYING_AS_HTTP0);
		goto try_again;
	    } else {
		status = HT_NOT_LOADED;
		goto clean_up;
	    }
	}
    }

    /*
     * Free if complete transmission (socket was closed before return).  Close
     * socket if partial transmission (was freed on abort).
     */
    if (rv != HT_INTERRUPTED && rv != -2) {
	(*target->isa->_free) (target);
    } else {
	HTTP_NETCLOSE(s, handle);
    }

    if (doing_redirect) {
	if (redirecting_url) {
	    /*
	     * Set up for checking redirecting_url in LYGetFile.c for
	     * restrictions before we seek the document at that Location.  - FM
	     */
	    CTRACE((tfp, "HTTP: Picked up location '%s'\n",
		    redirecting_url));
	    if (rv == HT_INTERRUPTED) {
		/*
		 * Intentional interrupt after data were received, not an error
		 * (probably).  We take it as a user request to abandon the
		 * redirection chain.
		 *
		 * This could reasonably be changed (by just removing this
		 * block), it would make sense if there are redirecting
		 * resources that "hang" after sending the headers.  - kw
		 */
		FREE(redirecting_url);
		CTRACE((tfp, "HTTP: Interrupted followup read.\n"));
		status = HT_INTERRUPTED;
		goto clean_up;
	    }
	    HTProgress(line_buffer);
	    if (server_status == 305) {		/* Use Proxy */
		/*
		 * Make sure the proxy field ends with a slash.  - FM
		 */
		if (redirecting_url[strlen(redirecting_url) - 1]
		    != '/')
		    StrAllocCat(redirecting_url, "/");
		/*
		 * Append our URL.  - FM
		 */
		StrAllocCat(redirecting_url, anAnchor->address);
		CTRACE((tfp, "HTTP: Proxy URL is '%s'\n",
			redirecting_url));
	    }
	    if (!do_post ||
		server_status == 303 ||
		server_status == 302) {
		/*
		 * We don't have POST content (nor support PUT or DELETE), or
		 * the status is "See Other" or "General Redirection" and we
		 * can convert to GET, so go back and check out the new URL.  -
		 * FM
		 */
		status = HT_REDIRECTING;
		goto clean_up;
	    }
	    /*
	     * Make sure the user wants to redirect the POST content, or treat
	     * as GET - FM & DK
	     */
	    switch (HTConfirmPostRedirect(redirecting_url,
					  server_status)) {
		/*
		 * User failed to confirm.  Abort the fetch.
		 */
	    case 0:
		FREE(redirecting_url);
		status = HT_NO_DATA;
		goto clean_up;

		/*
		 * User wants to treat as GET with no content.  Go back to
		 * check out the URL.
		 */
	    case 303:
		break;

		/*
		 * Set the flag to retain the POST content and go back to check
		 * out the URL.  - FM
		 */
	    default:
		redirect_post_content = TRUE;
	    }

	    /* Lou's old comment:  - FM */
	    /* OK, now we've got the redirection URL temporarily stored
	       in external variable redirecting_url, exported from HTMIME.c,
	       since there's no straightforward way to do this in the library
	       currently.  Do the right thing. */

	    status = HT_REDIRECTING;

	} else {
	    status = traversal ? -1 : HT_LOADED;
	}

    } else {
	/*
	 * If any data were received, treat as a complete transmission
	 */
	status = HT_LOADED;
    }

    /*
     * Clean up
     */
  clean_up:
    FREE(line_buffer);
    FREE(line_kept_clean);

  done:
    /*
     * Clear out on exit, just in case.
     */
    reloading = FALSE;
#ifdef USE_SSL
    FREE(connect_host);
    if (handle) {
	SSL_free(handle);
	SSL_handle = handle = NULL;
    }
#endif /* USE_SSL */
    dump_server_status = server_status;
    return status;
}

/*	Protocol descriptor
*/
#ifdef GLOBALDEF_IS_MACRO
#define _HTTP_C_GLOBALDEF_1_INIT { "http", HTLoadHTTP, 0}
GLOBALDEF(HTProtocol, HTTP, _HTTP_C_GLOBALDEF_1_INIT);
#define _HTTP_C_GLOBALDEF_2_INIT { "https", HTLoadHTTP, 0}
GLOBALDEF(HTProtocol, HTTPS, _HTTP_C_GLOBALDEF_2_INIT);
#else
GLOBALDEF HTProtocol HTTP =
{"http", HTLoadHTTP, 0};
GLOBALDEF HTProtocol HTTPS =
{"https", HTLoadHTTP, 0};
#endif /* GLOBALDEF_IS_MACRO */
