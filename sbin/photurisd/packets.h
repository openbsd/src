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
/* $Id: packets.h,v 1.1 1998/11/14 23:37:26 deraadt Exp $ */
/*
 * packets.h:
 */

#ifndef _PACKETS_H_
#define _PACKETS_H_

#define COOKIE_REQUEST       0
#define COOKIE_RESPONSE      1
#define VALUE_REQUEST        2
#define VALUE_RESPONSE       3
#define IDENTITY_REQUEST     4
#define SECRET_RESPONSE      5
#define SECRET_REQUEST       6
#define IDENTITY_RESPONSE    7
#define SPI_NEEDED           8
#define SPI_UPDATE           9
#define BAD_COOKIE           10
#define RESOURCE_LIMIT       11
#define VERIFICATION_FAILURE 12
#define MESSAGE_REJECT       13

#define COOKIE_SIZE 16
#define SPI_SIZE 4

/* General packet definition */

#define FLD_CONST	0
#define FLD_VARPRE	1
#define FLD_ATTRIB	2

#define FMD_ATT_ONE	0
#define FMD_ATT_FILL	1

struct packet_sub {
        char *field;		/* Name of Field */
        int type;		/* Type of Field */
        int mod;		/* Modifier: */
        u_int16_t size;		/* Pointer to start of Field */
        void *where;		/* Pointer to start of Field */
};

struct packet {
        char *name;
        int min, max;
        struct packet_sub *parts;
};

struct cookie_request {
	u_int8_t icookie[COOKIE_SIZE];
	u_int8_t rcookie[COOKIE_SIZE];
	u_int8_t type;
	u_int8_t counter;
};

#define COOKIE_REQUEST_PACKET_SIZE sizeof(struct cookie_request)

struct cookie_response {
	u_int8_t icookie[COOKIE_SIZE];
	u_int8_t rcookie[COOKIE_SIZE];
	u_int8_t type;
	u_int8_t counter;
};

#define COOKIE_RESPONSE_MIN sizeof(struct cookie_response)
#define COOKIE_RESPONSE_SCHEMES(p) (((u_int8_t *)(p))+COOKIE_RESPONSE_MIN)

#define SCHEME_SIZE(p) (4 + ((u_int16_t)*((p)+2))*256 + (*((p)+3)))

struct value_request {
	u_int8_t icookie[COOKIE_SIZE];
	u_int8_t rcookie[COOKIE_SIZE];
	u_int8_t type;
	u_int8_t counter;
	u_int8_t scheme[2];
};

#define VALUE_REQUEST_MIN sizeof(struct value_request)
#define VALUE_REQUEST_VALUE(p) (((u_int8_t *)(p))+VALUE_REQUEST_MIN)

struct value_response {
	u_int8_t icookie[COOKIE_SIZE];
	u_int8_t rcookie[COOKIE_SIZE];
	u_int8_t type;
	u_int8_t reserved[3];
};

#define VALUE_RESPONSE_MIN sizeof(struct value_response)
#define VALUE_RESPONSE_VALUE(p) (((u_int8_t *)(p))+VALUE_RESPONSE_MIN)

struct identity_message {
	u_int8_t icookie[COOKIE_SIZE];
	u_int8_t rcookie[COOKIE_SIZE];
	u_int8_t type;
	u_int8_t lifetime[3];
	u_int8_t SPI[SPI_SIZE];
};

#define IDENTITY_MESSAGE_MIN sizeof(struct identity_message)
#define IDENTITY_MESSAGE_CHOICE(p) (((u_int8_t *)(p))+IDENTITY_MESSAGE_MIN)
#define IDENTITY_MESSAGE_IDENT(p) (IDENTITY_MESSAGE_CHOICE(p)+*((u_int8_t *)(p)+1))

struct spi_needed { 
        u_int8_t icookie[COOKIE_SIZE]; 
        u_int8_t rcookie[COOKIE_SIZE]; 
        u_int8_t type; 
        u_int8_t reserved[7]; 
}; 

#define SPI_NEEDED_MIN sizeof(struct spi_needed)
#define SPI_NEEDED_VERIFICATION(p) (((u_int8_t *)(p))+SPI_NEEDED_MIN)

struct spi_update { 
        u_int8_t icookie[COOKIE_SIZE]; 
        u_int8_t rcookie[COOKIE_SIZE]; 
        u_int8_t type; 
        u_int8_t lifetime[3]; 
        u_int8_t SPI[SPI_SIZE]; 
}; 

#define SPI_UPDATE_MIN sizeof(struct spi_update) 
#define SPI_UPDATE_VERIFICATION(p) (((u_int8_t *)(p))+SPI_UPDATE_MIN) 

struct error_message {  
        u_int8_t icookie[COOKIE_SIZE];  
        u_int8_t rcookie[COOKIE_SIZE];  
        u_int8_t type;  
};

#define ERROR_MESSAGE_PACKET_SIZE sizeof(struct error_message)

struct message_reject {  
        u_int8_t icookie[COOKIE_SIZE];  
        u_int8_t rcookie[COOKIE_SIZE];  
        u_int8_t type;  
        u_int8_t badtype;
        u_int16_t offset;
};

#define MESSAGE_REJECT_PACKET_SIZE sizeof(struct message_reject)

#endif /* _PACKETS_H_ */
