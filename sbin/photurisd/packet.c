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
static char rcsid[] = "$Id: packet.c,v 1.1 1998/11/14 23:37:25 deraadt Exp $";
#endif

#define _PACKET_C_

#include <stdlib.h>
#include <stdio.h> 
#include <string.h>
#include <ctype.h>
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
#include "scheme.h"
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

/*
 * packet_check() checks the format of the received packet against
 * the specified logical format. The position and size of the fields
 * are returned.
 */

int
packet_check(u_char *packet, u_int16_t size, struct packet *format)
{
     struct packet_sub *parts = format->parts;
     u_int16_t off, val, fsize;

     if (format->max != 0 && size > format->max)
	  return -1;
     if (size < format->min)
	  return -1;

     off = format->min;
     packet += off;

     while (off < size && parts != NULL && parts->field != NULL) {
	  parts->where = packet;
	  switch (parts->type) {
	  case FLD_CONST:
	       off += parts->size;
	       packet += parts->size;
	       fsize = parts->size;
	       break;
	  case FLD_VARPRE:
	       val = varpre2octets(packet);
	       off += val;
	       packet += val;
	       fsize = val;
	       break;
	  case FLD_ATTRIB:
	       if (parts->mod == FMD_ATT_FILL) {
		    fsize = 0;
		    while (off < size) {
			 val = packet[1] + 2;
			 off += val;
			 packet += val;
			 fsize += val;
		    }
	       } else {
		    val = packet[1] + 2;
		    off += val;
		    packet += val;
		    fsize = val;
	       }
	       break;
	  default:
	       return -1;
	  }
	  if (parts->size == 0)
	       parts->size = fsize;
	  else if(parts->size != fsize)
	       return -1;
	  parts++;
     }

     if (off != size || (parts != NULL && parts->field != NULL))
	  return -1;

     return 0;
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

#ifdef DEBUG
void
packet_ordered_dump(u_int8_t *packet, u_int16_t size, struct packet *format)
{
     struct packet_sub *parts = format->parts;
     u_int16_t off = 0;

     printf("Packet Header (%s):\n", format->name);
     packet_dump(packet, format->min, off);

     off += format->min;
     packet += format->min;
     while (off < size) {
	  printf("%s (%d):\n", parts->field, parts->size);
	  packet_dump(packet, parts->size, off);
	  off += parts->size;
	  packet += parts->size;

	  parts++;
     }
}

void
packet_dump(u_int8_t *packet, u_int16_t plen, u_int16_t start)
{
     char tmp[73], dump[33];
     int i, size, len, off;
     
     off = 0;
     while (off < plen) {
	  memset(tmp, ' ', sizeof(tmp));
	  tmp[72] = 0;

	  sprintf(tmp, "%04x ", (u_int32_t)(off + start));

	  len = 33;
	  size = plen - off > 16 ? 16 : plen - off;
	  bin2hex(dump, &len, packet, size);
	  for (i=0; i<size; i++) {
	       bcopy(dump+i*2, tmp+5+i*3, 2);
	       tmp[5 + 16*3 + 3 + i] = isprint(packet[i]) ? packet[i] : '.';
	  }
	  printf("%s\n", tmp);

	  off += size;
	  packet += size;
     }
}
#endif
