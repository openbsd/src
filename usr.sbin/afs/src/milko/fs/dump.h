/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $arla: dump.h,v 1.4 2002/01/29 03:54:54 lha Exp $
 */


#ifndef __FS_DUMP_H
#define __FS_DUMP_H 1


#define DUMPVERSION     1
#define DUMPBEGINMAGIC  0xb3a11322
#define DUMPENDMAGIC    0x3a214b6e

#define TAG_DUMPHEADER  1
#define TAG_VOLHEADER   2
#define TAG_VNODE       3
#define TAG_DUMPEND     4

#define DHTAG_VOLNAME    'n'
#define DHTAG_VOLID      'v'
#define DHTAG_DUMPTIMES  't'

#define VHTAG_VOLID      'i'
#define VHTAG_VERS       'v'
#define VHTAG_VOLNAME    'n'
#define VHTAG_INSERV     's'
#define VHTAG_BLESSED    'b'
#define VHTAG_VUNIQ      'u'
#define VHTAG_TYPE       't'
#define VHTAG_PARENT     'p'
#define VHTAG_CLONE      'c'
#define VHTAG_MAXQUOTA   'q'
#define VHTAG_MINQUOTA   'm'
#define VHTAG_DISKUSED   'd'
#define VHTAG_FILECNT    'f'
#define VHTAG_ACCOUNT    'a'
#define VHTAG_OWNER      'o'
#define VHTAG_CREAT      'C'
#define VHTAG_ACCESS     'A'
#define VHTAG_UPDATE     'U'
#define VHTAG_EXPIRE     'E'
#define VHTAG_BACKUP     'B'
#define VHTAG_OFFLINE    'O'
#define VHTAG_MOTD       'M'
#define VHTAG_WEEKUSE    'W'
#define VHTAG_DUDATE     'D'
#define VHTAG_DAYUSE     'Z'

#define VTAG_TYPE        't'
#define VTAG_NLINKS      'l'
#define VTAG_DVERS       'v'
#define VTAG_CLIENT_DATE 'm'
#define VTAG_AUTHOR      'a'
#define VTAG_OWNER       'o'
#define VTAG_GROUP       'g'
#define VTAG_MODE        'b'
#define VTAG_PARENT      'p'
#define VTAG_SERVER_DATE 's'
#define VTAG_ACL         'A'
#define VTAG_DATA        'f'

#define SIZEOF_LARGEDISKVNODE    256
#define SIZEOF_SMALLDISKVNODE    64
#define SIZEOF_ACL		(SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE)

#define GOT_VTAG_DVERS		0x0001
#define GOT_VTAG_AUTHOR		0x0002
#define GOT_VTAG_OWNER		0x0004
#define GOT_VTAG_GROUP		0x0008
#define GOT_VTAG_PARENT		0x0010
#define GOT_VTAG_CLIENT_DATE	0x0020
#define GOT_VTAG_SERVER_DATE	0x0040
#define GOT_VTAG_NLINKS		0x0080
#define GOT_VTAG_MODE		0x0100
#define GOT_VTAG_TYPE		0x0200
#define GOT_VTAG_ACL		0x0400

#define GOT_ALL_FILE		0x03F7

int
parse_dump(struct rx_call *call, volume_handle *vh);

int
generate_dump(struct rx_call *call, volume_handle *vh);

#endif /* __FS_DUMP_H */
