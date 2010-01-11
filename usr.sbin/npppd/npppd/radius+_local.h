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
 * radius+.cc : 
 *   yet another RADIUS library
 */
#ifndef RADIUSPLUS_LOCAL_H
#define RADIUSPLUS_LOCAL_H

#include "nint.h"

#ifndef countof
#define countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

#pragma pack(1)
typedef struct _RADIUS_PACKET_DATA
{
	u_int8_t  code;
	u_int8_t  id;
	nuint16   length;
	char      authenticator[16];
	char      attributes[0];
} RADIUS_PACKET_DATA;

typedef struct _RADIUS_ATTRIBUTE
{
	u_int8_t  type;
	u_int8_t  length;
	char      data[0];
	nuint32   vendor;
	u_int8_t  vtype;
	u_int8_t  vlength;
	char      vdata[0];
} RADIUS_ATTRIBUTE;
#pragma pack()

struct _RADIUS_PACKET
{
	RADIUS_PACKET_DATA* pdata;
	size_t capacity;
	const RADIUS_PACKET* request;
};

#define RADIUS_PACKET_CAPACITY_INITIAL   64
#define RADIUS_PACKET_CAPACITY_INCREMENT 64

extern u_int8_t radius_id_counter;

inline void ADVANCE(RADIUS_ATTRIBUTE*& rp)
{
	rp = (RADIUS_ATTRIBUTE*)(((char*)rp) + rp->length);
}

inline void ADVANCE(const RADIUS_ATTRIBUTE*& rp)
{
	rp = (const RADIUS_ATTRIBUTE*)(((const char*)rp) + rp->length);
}

inline RADIUS_ATTRIBUTE* ATTRS_BEGIN(RADIUS_PACKET_DATA* pdata)
{
	return (RADIUS_ATTRIBUTE*)pdata->attributes;
}

inline const RADIUS_ATTRIBUTE* ATTRS_BEGIN(const RADIUS_PACKET_DATA* pdata)
{
	return (const RADIUS_ATTRIBUTE*)pdata->attributes;
}

inline RADIUS_ATTRIBUTE* ATTRS_END(RADIUS_PACKET_DATA* pdata)
{
	return (RADIUS_ATTRIBUTE*)(((char*)pdata) + pdata->length);
}

inline const RADIUS_ATTRIBUTE* ATTRS_END(const RADIUS_PACKET_DATA* pdata)
{
	return (const RADIUS_ATTRIBUTE*)(((const char*)pdata) + pdata->length);
}

#ifndef	MIN
#define	MIN(m,n)	(((m) < (n))? (m) : (n))
#endif

#endif /* RADIUSPLUS_LOCAL_H */
