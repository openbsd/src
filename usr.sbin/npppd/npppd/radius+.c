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
 * THIS SOFTWARE IS PROVIDED BY THE"AUTHOR" AND CONTRIBUTORS AS IS'' AND
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
 * radius+.cc : 
 *   yet another RADIUS library
 */
#ifdef WITH_MPATROL
#include <mpatrol.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <md5.h>
#include "radius+.h"
#include "radiusconst.h"

#include "radius+_local.h"

u_int8_t radius_id_counter = 0;

static int radius_check_packet_data(const RADIUS_PACKET_DATA* pdata,
                                    size_t length)
{
	const RADIUS_ATTRIBUTE* attr;
	const RADIUS_ATTRIBUTE* end;

	if(length < sizeof(RADIUS_PACKET_DATA))
		return 1;
	if(length > 0xffff)
		return 1;
	if(length != (size_t)ntohs(pdata->length))
		return 1;

	attr = ATTRS_BEGIN(pdata);
	end  = ATTRS_END(pdata);
	for(; attr<end; ADVANCE(attr))
	{
		if(attr->length < 2)
			return 1;
		if(attr->type == RADIUS_TYPE_VENDOR_SPECIFIC)
		{
			if(attr->length < 8)
				return 1;
			if((attr->vendor & htonl(0xff000000U)) != 0)
				return 1;
			if(attr->length != attr->vlength + 6)
				return 1;
		}
	}

	if(attr != end)
		return 1;

	return 0;
}

static int radius_ensure_add_capacity(RADIUS_PACKET* packet, size_t capacity)
{
	size_t newsize;
	void* newptr;

	/*
	 * The maximum size is 64KB.
	 * We use little bit smaller value for our safety(?).
	 */
	if(ntohs(packet->pdata->length) + capacity > 0xfe00)
		return 1;

	if(ntohs(packet->pdata->length) + capacity > packet->capacity)
	{
		newsize = ntohs(packet->pdata->length) + capacity +
			RADIUS_PACKET_CAPACITY_INCREMENT;
		newptr = realloc(packet->pdata, newsize);
		if(newptr == NULL)
			return 1;
		packet->capacity = newsize;
		packet->pdata = (RADIUS_PACKET_DATA*)newptr;
	}

	return 0;
}

RADIUS_PACKET* radius_new_request_packet(u_int8_t code)
{
	RADIUS_PACKET* packet;
	unsigned int i;

	packet = (RADIUS_PACKET*)malloc(sizeof(RADIUS_PACKET));
	if(packet == NULL)
		return NULL;
	packet->pdata = (RADIUS_PACKET_DATA*)malloc(RADIUS_PACKET_CAPACITY_INITIAL);
	if(packet->pdata == NULL)
	{
		free(packet);
		return NULL;
	}
	packet->capacity = RADIUS_PACKET_CAPACITY_INITIAL;
	packet->request = NULL;
	packet->pdata->code = code;
	packet->pdata->id = radius_id_counter++;
	packet->pdata->length = htons(sizeof(RADIUS_PACKET_DATA));
	for(i=0; i<countof(packet->pdata->authenticator); i++)
		packet->pdata->authenticator[i] = rand()&0xff;

	return packet;
}

RADIUS_PACKET* radius_new_response_packet(u_int8_t code,
                                          const RADIUS_PACKET* request)
{
	RADIUS_PACKET* packet;

	packet = radius_new_request_packet(code);
	if(packet == NULL)
		return NULL;
	packet->request = request;
	packet->pdata->id = request->pdata->id;

	return packet;
}

RADIUS_PACKET* radius_convert_packet(const void* pdata, size_t length)
{
	RADIUS_PACKET* packet;

	if(radius_check_packet_data((const RADIUS_PACKET_DATA*)pdata, length) != 0)
		return NULL;
	packet = (RADIUS_PACKET*)malloc(sizeof(RADIUS_PACKET));
	if(packet == NULL)
		return NULL;
	packet->pdata = (RADIUS_PACKET_DATA*)malloc(length);
	packet->capacity = length;
	packet->request = NULL;
	if(packet->pdata == NULL)
	{
		free(packet);
		return NULL;
	}
	memcpy(packet->pdata, pdata, length);

	return packet;
}

int radius_delete_packet(RADIUS_PACKET* packet)
{
	free(packet->pdata);
	free(packet);
	return 0;
}

u_int8_t radius_get_code(const RADIUS_PACKET* packet)
{
	return packet->pdata->code;
}

u_int8_t radius_get_id(const RADIUS_PACKET* packet)
{
	return packet->pdata->id;
}

void radius_get_authenticator(const RADIUS_PACKET* packet, char* authenticator)
{
	memcpy(authenticator, packet->pdata->authenticator, 16);
}

const char* radius_get_authenticator_retval(const RADIUS_PACKET* packet)
{
	return packet->pdata->authenticator;
}

void radius_set_request_packet(RADIUS_PACKET* packet,
                               const RADIUS_PACKET* request)
{
	packet->request = request;
}

int radius_check_response_authenticator(const RADIUS_PACKET* packet,
                                        const char* secret)
{
	MD5_CTX ctx;
	unsigned char authenticator0[16];

	/* Assume radius_set_request_packet() was called before calling */
	if (packet->request == NULL)
		return -1;

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)packet->pdata, 4);
	MD5Update(&ctx, (unsigned char*)packet->request->pdata->authenticator,
	    	  16);
	MD5Update(&ctx,
		  (unsigned char*)packet->pdata->attributes,
		  radius_get_length(packet) - 20);
	MD5Update(&ctx, (unsigned char*)secret, strlen(secret));
	MD5Final((unsigned char *)authenticator0, &ctx);

	return memcmp(authenticator0, packet->pdata->authenticator, 16);
}

void radius_set_response_authenticator(RADIUS_PACKET* packet,
                                       const char* secret)
{
	MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)packet->pdata, 4);
	MD5Update(&ctx,
		  (unsigned char*)packet->request->pdata->authenticator, 16);
	MD5Update(&ctx,
		  (unsigned char*)packet->pdata->attributes,
		  radius_get_length(packet) - 20);
	MD5Update(&ctx, (unsigned char*)secret, strlen(secret));
	MD5Final((unsigned char*)packet->pdata->authenticator ,&ctx);
}

u_int16_t radius_get_length(const RADIUS_PACKET* packet)
{
	return ntohs(packet->pdata->length);
}


const void* radius_get_data(const RADIUS_PACKET* packet)
{
	return packet->pdata;
}

int radius_get_raw_attr(const RADIUS_PACKET* packet, u_int8_t type,
                        void* buf, u_int8_t* length)
{
	const RADIUS_ATTRIBUTE* attr;
	const RADIUS_ATTRIBUTE* end;

	attr = ATTRS_BEGIN(packet->pdata);
	end  = ATTRS_END(packet->pdata);

	for(; attr<end; ADVANCE(attr))
	{
		if(attr->type != type)
			continue;
		*length = attr->length - 2;
		memcpy(buf, attr->data, attr->length - 2);
		return 0;
	}

	return 1;
}

/*
 * To determine the length of the data, set the buf = NULL.
 */
int radius_get_raw_attr_all(const RADIUS_PACKET* packet, u_int8_t type,
                            caddr_t buf, int *length)
{
	int off;
	const RADIUS_ATTRIBUTE* attr;
	const RADIUS_ATTRIBUTE* end;

	attr = ATTRS_BEGIN(packet->pdata);
	end  = ATTRS_END(packet->pdata);

	for(off = 0; attr<end; ADVANCE(attr))
	{
		if(attr->type != type)
			continue;
		if (buf != NULL) {
			if (off + attr->length - 2 <= *length)
				memcpy(buf + off, attr->data, attr->length - 2);
			else
				return 1;
		}
		off += attr->length - 2;
	}
	*length = off;

	return 0;
}

int radius_put_raw_attr(RADIUS_PACKET* packet, u_int8_t type,
                        const void* buf, u_int8_t length)
{
	RADIUS_ATTRIBUTE* newattr;

	if(length > 255-2)
		return 1;

	if(radius_ensure_add_capacity(packet, length+2) != 0)
		return 1;

	newattr = ATTRS_END(packet->pdata);
	newattr->type = type;
	newattr->length = length + 2;
	memcpy(newattr->data, buf, length);
	packet->pdata->length = htons(radius_get_length(packet) + length + 2);

	return 0;
}

int radius_put_raw_attr_all(RADIUS_PACKET* packet, u_int8_t type,
                            caddr_t buf, int length)
{
	int off, len0;
	RADIUS_ATTRIBUTE* newattr;

	off = 0;
	while (off < length) {
		len0 = MIN(length - off, 255-2);

		if(radius_ensure_add_capacity(packet, len0+2) != 0)
			return 1;

		newattr = ATTRS_END(packet->pdata);
		newattr->type = type;
		newattr->length = len0 + 2;
		memcpy(newattr->data, buf, len0);
		packet->pdata->length = htons(radius_get_length(packet) +
		    len0 + 2);

		off += len0;
	}

	return 0;
}

int radius_get_vs_raw_attr(const RADIUS_PACKET* packet, u_int32_t vendor,
                           u_int8_t vtype, void* buf, u_int8_t* length)
{
	const RADIUS_ATTRIBUTE* attr;
	const RADIUS_ATTRIBUTE* end;

	attr = ATTRS_BEGIN(packet->pdata);
	end  = ATTRS_END(packet->pdata);

	for(; attr<end; ADVANCE(attr))
	{
		if(attr->type != RADIUS_TYPE_VENDOR_SPECIFIC)
			continue;
		if(attr->vendor != htonl(vendor))
			continue;
		if(attr->vtype != vtype)
			continue;

		*length = attr->vlength - 2;
		memcpy(buf, attr->vdata, attr->vlength - 2);
		return 0;
	}

	return 1;
}

/*
 * To determine the length of the data, set the buf = NULL.
 */
int radius_get_vs_raw_attr_all(const RADIUS_PACKET* packet, u_int32_t vendor,
                               u_int8_t vtype, caddr_t buf, int *length)
{
	int off;
	const RADIUS_ATTRIBUTE* attr;
	const RADIUS_ATTRIBUTE* end;

	attr = ATTRS_BEGIN(packet->pdata);
	end  = ATTRS_END(packet->pdata);

	off = 0;
	for(; attr<end; ADVANCE(attr))
	{
		if(attr->type != RADIUS_TYPE_VENDOR_SPECIFIC)
			continue;
		if(attr->vendor != htonl(vendor))
			continue;
		if(attr->vtype != vtype)
			continue;
		
		if (buf != NULL) {
			if (off + attr->vlength - 2 <= *length)
				memcpy(off + buf, attr->vdata,
				    attr->vlength - 2);
			else
				return 1;
		}
		off += attr->vlength;
	}
	*length = off;

	return 0;
}

int radius_get_vs_raw_attr_ptr(const RADIUS_PACKET* packet, u_int32_t vendor,
                           u_int8_t vtype, void** ptr, u_int8_t* length)
{
	const RADIUS_ATTRIBUTE* attr;
	const RADIUS_ATTRIBUTE* end;

	attr = ATTRS_BEGIN(packet->pdata);
	end  = ATTRS_END(packet->pdata);

	for(; attr<end; ADVANCE(attr))
	{
		if(attr->type != RADIUS_TYPE_VENDOR_SPECIFIC)
			continue;
		if(attr->vendor != htonl(vendor))
			continue;
		if(attr->vtype != vtype)
			continue;

		*length = attr->vlength - 2;
		*ptr = (void *)attr->vdata;
		return 0;
	}

	return 1;
}

int radius_put_vs_raw_attr(RADIUS_PACKET* packet, u_int32_t vendor,
                           u_int8_t vtype, const void* buf, u_int8_t length)
{
	RADIUS_ATTRIBUTE* newattr;

	if(length > 255-8)
		return 1;

	if(radius_ensure_add_capacity(packet, length+8) != 0)
		return 1;

	newattr = ATTRS_END(packet->pdata);
	newattr->type = RADIUS_TYPE_VENDOR_SPECIFIC;
	newattr->length = length + 8;
	newattr->vendor = htonl(vendor);
	newattr->vtype = vtype;
	newattr->vlength = length + 2;
	memcpy(newattr->vdata, buf, length);
	packet->pdata->length = htons(radius_get_length(packet) + length + 8);

	return 0;
}

int radius_put_vs_raw_attr_all(RADIUS_PACKET* packet, u_int32_t vendor,
                               u_int8_t vtype, const void* buf, int length)
{
	int off, len0;
	RADIUS_ATTRIBUTE* newattr;

	off = 0;
	while (off < length) {
		len0 = MIN(length - off, 255-8);

		if(radius_ensure_add_capacity(packet, len0+8) != 0)
			return 1;

		newattr = ATTRS_END(packet->pdata);
		newattr->type = RADIUS_TYPE_VENDOR_SPECIFIC;
		newattr->length = len0 + 8;
		newattr->vendor = htonl(vendor);
		newattr->vtype = vtype;
		newattr->vlength = len0 + 2;
		memcpy(newattr->vdata, buf, len0);
		packet->pdata->length = htons(radius_get_length(packet) +
		    len0 + 8);

		off += len0;
	}

	return 0;
}

int radius_get_uint32_attr(const RADIUS_PACKET* packet, u_int8_t type,
                           u_int32_t* val)
{
	u_int32_t nval;
	u_int8_t len;

	if(radius_get_raw_attr(packet, type, &nval, &len) != 0)
		return 1;
	if(len != sizeof(u_int32_t))
		return 1;
	*val = ntohl(nval);
	return 0;
}

u_int32_t radius_get_uint32_attr_retval(const RADIUS_PACKET* packet,
                                        u_int8_t type)
{
	u_int32_t nval;
	u_int8_t len;

	if(radius_get_raw_attr(packet, type, &nval, &len) != 0)
		return 0xffffffff;
	if(len != sizeof(u_int32_t))
		return 0xffffffff;
	return ntohl(nval);
}
	
int radius_put_uint32_attr(RADIUS_PACKET* packet, u_int8_t type, u_int32_t val)
{
	u_int32_t nval;

	nval = htonl(val);
	return radius_put_raw_attr(packet, type, &nval, sizeof(u_int32_t));
}

int radius_get_string_attr(const RADIUS_PACKET* packet, u_int8_t type,
                           char* str)
{
	u_int8_t len;

	if(radius_get_raw_attr(packet, type, str, &len) != 0)
		return 1;
	str[len] = '\0';
	return 0;
}

int radius_put_string_attr(RADIUS_PACKET* packet, u_int8_t type,
                           const char* str)
{
	return radius_put_raw_attr(packet, type, str, strlen(str));
}

int radius_get_vs_string_attr(const RADIUS_PACKET* packet, u_int32_t vendor,
                              u_int8_t vtype, char* str)
{
	u_int8_t len;

	if(radius_get_vs_raw_attr(packet, vendor, vtype, str, &len) != 0)
		return 1;
	str[len] = '\0';
	return 0;
}

int radius_put_vs_string_attr(RADIUS_PACKET* packet, u_int32_t vendor,
                             u_int8_t vtype, const char* str)
{
	return radius_put_vs_raw_attr(packet, vendor, vtype, str, strlen(str));
}

int radius_get_ipv4_attr(const RADIUS_PACKET* packet, u_int8_t type,
                         struct in_addr* addr)
{
	struct in_addr tmp;
	u_int8_t len;

	if(radius_get_raw_attr(packet, type, &tmp, &len) != 0)
		return 1;
	if(len != sizeof(struct in_addr))
		return 1;
	*addr = tmp;
	return 0;
}

struct in_addr radius_get_ipv4_attr_retval(const RADIUS_PACKET* packet,
                                    u_int8_t type)
{
	struct in_addr addr;
	u_int8_t len;

	if(radius_get_raw_attr(packet, type, &addr, &len) != 0)
		addr.s_addr = htonl(INADDR_ANY);
	if(len != sizeof(struct in_addr))
		addr.s_addr = htonl(INADDR_ANY);
	return addr;
}
	
int radius_put_ipv4_attr(RADIUS_PACKET* packet, u_int8_t type, struct in_addr addr)
{
	return radius_put_raw_attr(packet, type, &addr, sizeof(struct in_addr));
}

RADIUS_PACKET* radius_recvfrom(int s, int flags, struct sockaddr* addr, socklen_t* len)
{
	char buf[0x10000];
	ssize_t n;

	n = recvfrom(s, buf, sizeof(buf), flags, addr, len);
	if(n <= 0)
		return NULL;

	return radius_convert_packet(buf, (size_t)n);
}

int radius_sendto(int s, const RADIUS_PACKET* packet,
                  int flags, const struct sockaddr* addr, socklen_t len)
{
	ssize_t n;

	n = sendto(s, packet->pdata, radius_get_length(packet), flags, addr, len);
	if(n != radius_get_length(packet))
		return 1;
	return 0;
}

/**
 * Calculate keyed-hashing for message authenticaiton using md5.
 *
 * @param packet	pointer to target RADIUS_PACKET.
 * @param text_len	length of data stream
 * @param key		pointer to authentication key
 * @param key_len	length of authentication key
 * @param digest	caller digest to be filled in
 */
static void
radius_hmac_md5(RADIUS_PACKET *packet, const char * key, int key_len,
                caddr_t digest, int check)
{
	const RADIUS_ATTRIBUTE* attr;
	const RADIUS_ATTRIBUTE* end;
	MD5_CTX  context;
	u_char   k_ipad[65];	/* inner padding - key XORd with ipad */
	u_char   k_opad[65];	/* outer padding - key XORd with opad */
	u_char   key0[64];
	int      i;
	u_char zero16[16];

	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64)
	{
		MD5_CTX         tctx;

		MD5Init(&tctx);
		MD5Update(&tctx, (const u_char *)key, key_len);
		MD5Final((u_char *)key0, &tctx);

		key_len = 16;
	} else
		memcpy(key0, key, key_len);

	key = (const char *)key0;

	/*
         * the HMAC_MD5 transform looks like:
         *
         * MD5(K XOR opad, MD5(K XOR ipad, text))
         *
         * where K is an n byte key
         * ipad is the byte 0x36 repeated 64 times
         * opad is the byte 0x5c repeated 64 times
         * and text is the data being protected
         */

	/* start out by storing key in pads */
	memset(k_ipad, 0, sizeof k_ipad);
	memset(k_opad, 0, sizeof k_opad);
	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++)
	{
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/* perform inner MD5 */
	MD5Init(&context);			/* init context for 1st pass */
	MD5Update(&context, k_ipad, 64);	/* start with inner pad */

	/*
	 * Traverse the radius packet.
	 */
	if (check)
	{
		MD5Update(&context, (const u_char *)packet->pdata, 4);
		MD5Update(&context, (unsigned char*)packet->request->pdata
		    ->authenticator, 16);
	}
	else
	{
		MD5Update(&context, (const u_char *)packet->pdata,
		    sizeof(RADIUS_PACKET_DATA));
	}

	attr = ATTRS_BEGIN(packet->pdata);
	end  = ATTRS_END(packet->pdata);
	memset(zero16, 0, sizeof(zero16));

	for(; attr<end; ADVANCE(attr))
	{
		if (attr->type == RADIUS_TYPE_MESSAGE_AUTHENTICATOR)
		{
			MD5Update(&context, (u_char *)attr, 2);
			MD5Update(&context, (u_char *)zero16, sizeof(zero16));
		} else
			MD5Update(&context, (u_char *)attr, (int)attr->length);
	}

	MD5Final((u_char *)digest, &context);	/* finish up 1st pass */
	/*
         * perform outer MD5
         */
	MD5Init(&context);			/* init context for 2nd pass */
	MD5Update(&context, k_opad, 64);	/* start with outer pad */
	MD5Update(&context, (u_char *)digest, 16);/* then results of 1st hash */
	MD5Final((u_char *)digest, &context);	/* finish up 2nd pass */
}

/* RFC 3579 */
int radius_put_message_authenticator(RADIUS_PACKET *packet, const char *secret)
{
	int rval;
	u_char md5result[16];
	RADIUS_ATTRIBUTE* attr;
	RADIUS_ATTRIBUTE* end;

	if ((rval = radius_put_raw_attr(packet,
	    RADIUS_TYPE_MESSAGE_AUTHENTICATOR, md5result, sizeof(md5result)))
	    != 0)
		return rval;

	radius_hmac_md5(packet, secret, strlen(secret), (caddr_t)md5result, 0);

	attr = ATTRS_BEGIN(packet->pdata);
	end  = ATTRS_END(packet->pdata);

	for(; attr<end; ADVANCE(attr))
	{
		if (attr->type == RADIUS_TYPE_MESSAGE_AUTHENTICATOR)
		{
			memcpy(attr->data, md5result, sizeof(md5result));
			break;
		}
	}

	return 0;
}

int radius_check_message_authenticator(RADIUS_PACKET *packet,
                                       const char *secret)
{
	int rval;
	u_char len, md5result0[16], md5result1[16];

	radius_hmac_md5(packet, secret, strlen(secret), (caddr_t)md5result0,
	    1);

	if ((rval = radius_get_raw_attr(packet,
	    RADIUS_TYPE_MESSAGE_AUTHENTICATOR, md5result1, &len)) != 0)
		return rval;

	if (len != sizeof(md5result1))
		return -1;

	return memcmp(md5result0, md5result1, sizeof(md5result1));
}
