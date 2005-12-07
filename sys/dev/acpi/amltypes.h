/* $OpenBSD: amltypes.h,v 1.1 2005/12/07 04:24:31 jordan Exp $ */
/*
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
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

#ifndef __AMLPARSE_H__
#define __AMLPARSE_H__

/* AML Opcodes */
#define AMLOP_ZERO             0x00
#define AMLOP_ONE              0x01
#define AMLOP_ALIAS            0x06
#define AMLOP_NAME             0x08
#define AMLOP_BYTEPREFIX       0x0A
#define AMLOP_WORDPREFIX       0x0B
#define AMLOP_DWORDPREFIX      0x0C
#define AMLOP_STRINGPREFIX     0x0D
#define AMLOP_QWORDPREFIX      0x0E
#define AMLOP_SCOPE            0x10
#define AMLOP_BUFFER           0x11
#define AMLOP_PACKAGE          0x12
#define AMLOP_VARPACKAGE       0x13
#define AMLOP_METHOD           0x14
#define AMLOP_DUALNAMEPREFIX   0x2E
#define AMLOP_MULTINAMEPREFIX  0x2F
#define AMLOP_EXTOPPREFIX      0x5B
#define AMLOP_MUTEX            0x5B01
#define AMLOP_EVENT            0x5B02
#define AMLOP_CONDREFOF        0x5B12
#define AMLOP_CREATEFIELD      0x5B13
#define AMLOP_LOADTABLE        0x5B1F
#define AMLOP_LOAD             0x5B20
#define AMLOP_STALL            0x5B21
#define AMLOP_SLEEP            0x5B22
#define AMLOP_ACQUIRE          0x5B23
#define AMLOP_SIGNAL           0x5B24
#define AMLOP_WAIT             0x5B25
#define AMLOP_RESET            0x5B26
#define AMLOP_RELEASE          0x5B27
#define AMLOP_FROMBCD          0x5B28
#define AMLOP_TOBCD            0x5B29
#define AMLOP_UNLOAD           0x5B2A
#define AMLOP_REVISION         0x5B30
#define AMLOP_DEBUG            0x5B31
#define AMLOP_FATAL            0x5B32
#define AMLOP_OPREGION         0x5B80
#define AMLOP_FIELD            0x5B81
#define AMLOP_DEVICE           0x5B82
#define AMLOP_PROCESSOR        0x5B83
#define AMLOP_POWERRSRC        0x5B84
#define AMLOP_THERMALZONE      0x5B85
#define AMLOP_INDEXFIELD       0x5B86
#define AMLOP_BANKFIELD        0x5B87
#define AMLOP_DATAREGION       0x5B88
#define AMLOP_ROOTCHAR         0x5C
#define AMLOP_PARENTPREFIX     0x5E
#define AMLOP_NAMECHAR         0x5F
#define AMLOP_LOCAL0           0x60
#define AMLOP_LOCAL1           0x61
#define AMLOP_LOCAL2           0x62
#define AMLOP_LOCAL3           0x63
#define AMLOP_LOCAL4           0x64
#define AMLOP_LOCAL5           0x65
#define AMLOP_LOCAL6           0x66
#define AMLOP_LOCAL7           0x67
#define AMLOP_ARG0             0x68
#define AMLOP_ARG1             0x69
#define AMLOP_ARG2             0x6A
#define AMLOP_ARG3             0x6B
#define AMLOP_ARG4             0x6C
#define AMLOP_ARG5             0x6D
#define AMLOP_ARG6             0x6E
#define AMLOP_STORE            0x70
#define AMLOP_REFOF            0x71
#define AMLOP_ADD              0x72
#define AMLOP_CONCAT           0x73
#define AMLOP_SUBTRACT         0x74
#define AMLOP_INCREMENT        0x75
#define AMLOP_DECREMENT        0x76
#define AMLOP_MULTIPLY         0x77
#define AMLOP_DIVIDE           0x78
#define AMLOP_SHL              0x79
#define AMLOP_SHR              0x7A
#define AMLOP_AND              0x7B
#define AMLOP_NAND             0x7C
#define AMLOP_OR               0x7D
#define AMLOP_NOR              0x7E
#define AMLOP_XOR              0x7F
#define AMLOP_NOT              0x80
#define AMLOP_FINDSETLEFTBIT   0x81
#define AMLOP_FINDSETRIGHTBIT  0x82
#define AMLOP_DEREFOF          0x83
#define AMLOP_CONCATRES        0x84
#define AMLOP_MOD              0x85
#define AMLOP_NOTIFY           0x86
#define AMLOP_SIZEOF           0x87
#define AMLOP_INDEX            0x88
#define AMLOP_DEREFOF          0x83
#define AMLOP_MATCH            0x89
#define AMLOP_CREATEDWORDFIELD 0x8A
#define AMLOP_CREATEWORDFIELD  0x8B
#define AMLOP_CREATEBYTEFIELD  0x8C
#define AMLOP_CREATEBITFIELD   0x8D
#define AMLOP_OBJECTTYPE       0x8E
#define AMLOP_CREATEQWORDFIELD 0x8F
#define AMLOP_LAND             0x90
#define AMLOP_LOR              0x91
#define AMLOP_LNOT             0x92
#define AMLOP_LNOTEQUAL        0x9293
#define AMLOP_LLESSEQUAL       0x9294
#define AMLOP_LGREATEREQUAL    0x9295
#define AMLOP_LEQUAL           0x93
#define AMLOP_LGREATER         0x94
#define AMLOP_LLESS            0x95
#define AMLOP_TOBUFFER         0x96
#define AMLOP_TODECSTRING      0x97
#define AMLOP_TOHEXSTRING      0x98
#define AMLOP_TOINTEGER        0x99
#define AMLOP_TOSTRING         0x9C
#define AMLOP_COPYOBJECT       0x9D
#define AMLOP_MID              0x9E
#define AMLOP_CONTINUE         0x9F
#define AMLOP_IF               0xA0
#define AMLOP_ELSE             0xA1
#define AMLOP_WHILE            0xA2
#define AMLOP_NOP              0xA3
#define AMLOP_RETURN           0xA4
#define AMLOP_BREAK            0xA5
#define AMLOP_BREAKPOINT       0xCC
#define AMLOP_ONES             0xFF

/*
 * Comparison types for Match()
 * 
 *  true,==,<=,<,>=,>
 */
#define AML_MATCH_TR          0
#define AML_MATCH_EQ          1
#define AML_MATCH_LE          2
#define AML_MATCH_LT          3
#define AML_MATCH_GE          4
#define AML_MATCH_GT          5

/* Defined types for ObjectType() */
enum aml_objecttype {
	AML_OBJTYPE_UNINITIALIZED = 0,
	AML_OBJTYPE_INTEGER,
	AML_OBJTYPE_STRING,
	AML_OBJTYPE_BUFFER,
	AML_OBJTYPE_PACKAGE,
	AML_OBJTYPE_FIELDUNIT,
	AML_OBJTYPE_DEVICE,
	AML_OBJTYPE_EVENT,
	AML_OBJTYPE_METHOD,
	AML_OBJTYPE_MUTEX,
	AML_OBJTYPE_OPREGION,
	AML_OBJTYPE_POWERRSRC,
	AML_OBJTYPE_PROCESSOR,
	AML_OBJTYPE_THERMZONE,
	AML_OBJTYPE_BUFFERFIELD,
	AML_OBJTYPE_DDBHANDLE,
	AML_OBJTYPE_DEBUGOBJ
};

#endif /* __AMLPARSE_H__ */
