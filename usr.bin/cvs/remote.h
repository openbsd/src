/*	$OpenBSD: remote.h,v 1.21 2007/06/26 02:24:10 niallo Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef H_REMOTE
#define H_REMOTE

struct cvs_req {
	char	name[32];
	int	supported;

	void	(*hdlr)(char *);
	int	flags;
};

struct cvs_resp {
	char	name[32];
	int	supported;

	void	(*hdlr)(char *);
	int	flags;
};

#define	REQ_NEEDED	0x01
#define RESP_NEEDED	0x01

extern int server_response;

#define SERVER_OK	0
#define SERVER_ERROR	1

void	cvs_client_connect_to_server(void);
void	cvs_client_disconnect(void);
void	cvs_client_send_request(char *, ...);
void	cvs_client_read_response(void);
void	cvs_client_get_responses(void);

void	cvs_client_ok(char *);
void	cvs_client_error(char *);
void	cvs_client_validreq(char *);
void	cvs_client_e(char *);
void	cvs_client_m(char *);
void	cvs_client_checkedin(char *);
void	cvs_client_updated(char *);
void	cvs_client_merged(char *);
void	cvs_client_removed(char *);
void	cvs_client_remove_entry(char *);
void	cvs_client_set_static_directory(char *);
void	cvs_client_clear_static_directory(char *);
void	cvs_client_set_sticky(char *);
void	cvs_client_clear_sticky(char *);

void	cvs_client_senddir(const char *);
void	cvs_client_sendfile(struct cvs_file *);
void	cvs_client_send_files(char **, int);

void	cvs_server_root(char *);
void	cvs_server_send_response(char *, ...);
void	cvs_server_validresp(char *);
void	cvs_server_validreq(char *);
void	cvs_server_globalopt(char *);
void	cvs_server_directory(char *);
void	cvs_server_entry(char *);
void	cvs_server_modified(char *);
void	cvs_server_useunchanged(char *);
void	cvs_server_unchanged(char *);
void	cvs_server_questionable(char *);
void	cvs_server_argument(char *);
void	cvs_server_argumentx(char *);
void	cvs_server_set(char *);
void	cvs_server_static_directory(char *);
void	cvs_server_sticky(char *);
void	cvs_server_update_patches(char *);
void	cvs_server_update_entry(const char *, struct cvs_file *cf);

void	cvs_server_add(char *);
void	cvs_server_import(char *);
void	cvs_server_admin(char *);
void	cvs_server_annotate(char *);
void	cvs_server_commit(char *);
void	cvs_server_checkout(char *);
void	cvs_server_diff(char *);
void	cvs_server_init(char *);
void	cvs_server_log(char *);
void	cvs_server_rlog(char *);
void	cvs_server_remove(char *);
void	cvs_server_status(char *);
void	cvs_server_tag(char *);
void	cvs_server_update(char *);
void	cvs_server_version(char *);

void	cvs_remote_classify_file(struct cvs_file *);
void	cvs_remote_output(const char *);
char	*cvs_remote_input(void);
void	cvs_remote_receive_file(int, size_t);
void	cvs_remote_send_file(const char *);

extern int cvs_client_inlog_fd;
extern int cvs_client_outlog_fd;

extern struct cvs_req cvs_requests[];
extern struct cvs_resp cvs_responses[];

struct cvs_req *cvs_remote_get_request_info(const char *);
struct cvs_resp *cvs_remote_get_response_info(const char *);

#endif
