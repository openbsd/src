/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * handle_packet.c:
 * handle messages from other photuris daemons.
 */

#ifndef lint
static char rcsid[] = "$Id: packet.c,v 1.1.1.1 1997/07/18 22:48:50 provos Exp $";
#endif

#define _PACKET_C_

#include <stdlib.h>
#include <stdio.h> 
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h> 
#include <arpa/inet.h> 
#include "state.h"
#include "photuris.h"
#include "packets.h"
#include "errlog.h"
#include "buffer.h"
#include "config.h"
#include "packet.h"
#include "server.h"

#define RECV_BUFFER_SIZE 8192

/* We have a serialised daemon */
static struct sockaddr_in sin;

int handle_packet(int sock, char *address)
{
	struct cookie_request *header;
	static char recv_buffer[RECV_BUFFER_SIZE];
	int i, size;

	bzero(recv_buffer, RECV_BUFFER_SIZE);

	i = sizeof(struct sockaddr_in);
	if ((size = recvfrom(sock, recv_buffer, RECV_BUFFER_SIZE, 0,
			     (struct sockaddr *) &sin, &i)) == -1)
	     crit_error(1, "recvfrom() in handle_packet()");

	header = (struct cookie_request *)recv_buffer;
#ifdef DEBUG
	i = BUFFER_SIZE;
	bin2hex(buffer, &i, header->icookie, 16);
	printf("%s: Received %d bytes from %s, type %d with icookie: 0x%s\n", 
	       address, size,
	       inet_ntoa(sin.sin_addr), header->type, buffer);
#endif

	switch(header->type) {
	case COOKIE_REQUEST:
	     if (handle_cookie_request(recv_buffer, size, 
				       inet_ntoa(sin.sin_addr), 
				       ntohs(sin.sin_port),
				       global_schemes, global_schemesize) 
		 == -1) {
		  log_error(0, "handle_cookie_request() in handle_packet()");
		  return -1;
	     }
	     break;
	case COOKIE_RESPONSE:
	     if (handle_cookie_response(recv_buffer, size,
					inet_ntoa(sin.sin_addr),
					ntohs(sin.sin_port)) == -1) {
		  log_error(0, "handle_cookie_response() in handle_packet()"); 
                  return -1; 
             } 
	     break;
	case VALUE_REQUEST:
	     if (handle_value_request(recv_buffer, size,
				       inet_ntoa(sin.sin_addr),
				       ntohs(sin.sin_port),
				       global_schemes, global_schemesize)
		 == -1) {
		  log_error(0, "handle_value_request() in handle_packet()"); 
                  return -1; 
             } 
             break;
	case VALUE_RESPONSE:
	     if (handle_value_response(recv_buffer, size, 
				       inet_ntoa(sin.sin_addr),
				       address) == -1) { 
                  log_error(0, "handle_value_response() in handle_packet()");  
                  return -1;  
             }  
             break;
	case IDENTITY_REQUEST:
	     if (handle_identity_request(recv_buffer, size,  
					 inet_ntoa(sin.sin_addr),
					 address) == -1) {  
                  log_error(0, "handle_identity_request() in handle_packet()");   
                  return -1;   
             }   
             break;
        case IDENTITY_RESPONSE: 
             if (handle_identity_response(recv_buffer, size,   
					  inet_ntoa(sin.sin_addr), 
					  address) == -1) {   
                  log_error(0, "handle_identity_response() in handle_packet()");
                  return -1;    
             }    
             break; 
	case SPI_UPDATE:
	     if (handle_spi_update(recv_buffer, size,
				   inet_ntoa(sin.sin_addr),
				   address) == -1) {
                  log_error(0, "handle_spi_update() in handle_packet()");
                  return -1;    
             }    
             break;
	case SPI_NEEDED:
	     if (handle_spi_needed(recv_buffer, size,
				   inet_ntoa(sin.sin_addr),
				   address) == -1) {
                  log_error(0, "handle_spi_needed() in handle_packet()");
                  return -1;    
             }    
             break;
	case BAD_COOKIE:
	     if (handle_bad_cookie(recv_buffer, size,
				   inet_ntoa(sin.sin_addr)) == -1) {
		  log_error(0, "handle_bad_cookie() in handle_packet()");
		  return -1;
	     }
	     break;
	case RESOURCE_LIMIT:
             if (handle_resource_limit(recv_buffer, size, 
				       inet_ntoa(sin.sin_addr)) == -1) { 
                  log_error(0, "handle_resource_limit() in handle_packet()"); 
                  return -1; 
             } 
	     break;
	case VERIFICATION_FAILURE:
             if (handle_verification_failure(recv_buffer, size,  
					     inet_ntoa(sin.sin_addr)) == -1) {  
                  log_error(0, "handle_verification_failure() in handle_packet()");
                  return -1;  
             }  
             break; 
	case MESSAGE_REJECT:
	     if (handle_message_reject(recv_buffer, size,   
				       inet_ntoa(sin.sin_addr)) == -1) {
                  log_error(0, "handle_message_reject() in handle_packet()");
                  return -1;
             }
	     break;
	default:
	     log_error(0, "Unknown packet type %d in handle_packet()", 
		       header->type);
	     return 0;
	}

	return 0;
}

void
send_packet(void)
{
#ifdef DEBUG 
     struct cookie_request *header = (struct cookie_request *)packet_buffer;
     int i = BUFFER_SIZE; 
     bin2hex(buffer, &i, header->icookie, 16); 
     printf("Sending %d bytes to %s, type %d with icookie: 0x%s\n", 
	    packet_size, inet_ntoa(sin.sin_addr), header->type, buffer); 
#endif 
     /* We constructed a valid response packet here, send it off. */
     if (sendto(global_socket, packet_buffer, packet_size, 0,  
		(struct sockaddr *) &sin, sizeof(sin)) != packet_size) { 
	  /* XXX Code to notify kernel of failure */ 
	  log_error(1, "sendto() in handle_packet()"); 
	  return; 
     } 
}

void
packet_save(struct stateob *st, u_int8_t *buffer, u_int16_t len)
{
     if (st->packet != NULL)
	  free(st->packet);

     if ((st->packet = calloc(len, sizeof(u_int8_t))) == NULL) {
	  st->packetlen = 0;
	  return;
     }

     bcopy(buffer, st->packet, len);
     st->packetlen = len;
}
