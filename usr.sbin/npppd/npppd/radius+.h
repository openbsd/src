/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * radius+.h : 
 *   yet another RADIUS library
 */
#ifndef RADIUS_PLUS_H
#define RADIUS_PLUS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/******* packet manipulation support *******/

typedef struct _RADIUS_PACKET RADIUS_PACKET;

/* constructors */
RADIUS_PACKET* radius_new_request_packet(u_int8_t code);
RADIUS_PACKET* radius_new_response_packet(u_int8_t code,
                                          const RADIUS_PACKET* request);
RADIUS_PACKET* radius_convert_packet(const void* pdata, size_t length);

/* destructor */
int radius_delete_packet(RADIUS_PACKET* packet);

/* accessors - header values */
u_int8_t radius_get_id(const RADIUS_PACKET* packet);
u_int8_t radius_get_code(const RADIUS_PACKET* packet);
void radius_get_authenticator(const RADIUS_PACKET* packet, char* authenticator);
void radius_set_request_packet(RADIUS_PACKET* packet, const RADIUS_PACKET* response);
int radius_check_response_authenticator(const RADIUS_PACKET* packet, const char *secret);
const char* radius_get_authenticator_retval(const RADIUS_PACKET* packet);
void radius_set_response_authenticator(RADIUS_PACKET* packet,
                                       const char* secret);
u_int16_t radius_get_length(const RADIUS_PACKET* packet);
const void* radius_get_data(const RADIUS_PACKET* packet);
const char* trim_ppp_username(const char *name);

/* accessors - raw attributes */
int radius_get_raw_attr(const RADIUS_PACKET* packet, u_int8_t type,
                        void* buf, u_int8_t* length);
int radius_put_raw_attr(RADIUS_PACKET* packet, u_int8_t type,
                        const void* buf, u_int8_t length);
int radius_get_raw_attr_all(const RADIUS_PACKET* packet, u_int8_t type,
                            caddr_t buf, int* length);
int radius_put_raw_attr_all(RADIUS_PACKET* packet, u_int8_t type,
                            const caddr_t buf, int length);
int radius_get_vs_raw_attr(const RADIUS_PACKET* packet, u_int32_t vendor,
                           u_int8_t vtype, void* buf, u_int8_t* length);
int radius_get_vs_raw_attr_ptr(const RADIUS_PACKET* packet, u_int32_t vendor,
                           u_int8_t vtype, void** ptr, u_int8_t* length);
int radius_put_vs_raw_attr(RADIUS_PACKET* packet, u_int32_t vendor,
                           u_int8_t vtype, const void* buf, u_int8_t length);
int radius_get_vs_raw_attr_all(const RADIUS_PACKET* packet, u_int8_t type,
                               caddr_t buf, int* length);
int radius_put_vs_raw_attr_all(RADIUS_PACKET* packet, u_int8_t type,
                               const caddr_t buf, int length);

/* accessors - typed attributes */
int radius_get_uint32_attr(const RADIUS_PACKET* packet, u_int8_t type,
                           u_int32_t* val);
u_int32_t radius_get_uint32_attr_retval(const RADIUS_PACKET* packet,
                                        u_int8_t type);
int radius_put_uint32_attr(RADIUS_PACKET* packet, u_int8_t type, u_int32_t val);

int radius_get_string_attr(const RADIUS_PACKET* packet, u_int8_t type,
                           char* str);
int radius_put_string_attr(RADIUS_PACKET* packet, u_int8_t type,
                           const char* str);
int radius_get_vs_string_attr(const RADIUS_PACKET* packet, u_int32_t vendor,
                              u_int8_t vtype, char* str);
int radius_put_vs_string_attr(RADIUS_PACKET* packet, u_int32_t vendor,
                              u_int8_t vtype, const char* str);

int radius_get_ipv4_attr(const RADIUS_PACKET* packet, u_int8_t type,
                         struct in_addr* addr);
struct in_addr radius_get_ipv4_attr_retval(const RADIUS_PACKET* packet, u_int8_t type);
int radius_put_ipv4_attr(RADIUS_PACKET* packet, u_int8_t type,
                         struct in_addr addr);
int radius_put_message_authenticator(RADIUS_PACKET *packet, const char *secret);
int radius_check_message_authenticator(RADIUS_PACKET *packet,
                                       const char *secret);

/* helpers */
RADIUS_PACKET* radius_recvfrom(int s, int flags,
                    struct sockaddr* saddr, socklen_t* slen);
int radius_sendto(int s, const RADIUS_PACKET* packet, int flags,
                  const struct sockaddr* saddr, socklen_t slen);


/******* client support (sending request / receiving response) *******/

typedef struct _RADIUS_SERVER RADIUS_SERVER;

/* constrcutors */
RADIUS_SERVER* radius_new_server(void);
RADIUS_SERVER* radius_new_auth_server(void);
RADIUS_SERVER* radius_new_acct_server(void);

/* destructors */
int radius_delete_server(RADIUS_SERVER* server);

/* synchronous requesting */
RADIUS_PACKET* radius_send_request(RADIUS_SERVER* server,
                                   const RADIUS_PACKET* packet);

/* asynchronous requesting - use select(2) */
int radius_async_request_init(RADIUS_SERVER* server,
                              const RADIUS_PACKET* packet, int* fd);
RADIUS_PACKET* radius_async_request_send(RADIUS_SERVER* server);

#ifdef __cplusplus
}
#endif

#endif
