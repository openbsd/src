/* -*- C++ -*- */
/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

#ifndef __CRYPTOENGINE_H__
#define __CRYPTOENGINE_H__


#include "des.h"
//#include "KClient.h"

typedef unsigned char	UChar,	*pUChar;

class CryptoEngine{
public:
    long lastLength;
    pUChar lastIn;
    int					mLastIVDecCount;
    des_cblock			mLastDecInitVec;

    void UndoDecrypt(pUChar out, long length);
    CryptoEngine(des_cblock* key);
    ~CryptoEngine();
    void Encrypt(pUChar in, pUChar out, long length);
    void Decrypt(pUChar in, pUChar out, long length);
    void RandData(pUChar buff, int length);
	
private:
    void cryptation(pUChar in, pUChar out, long length, int encrypt);
	
    int mIVEncCount;
    int mIVDecCount;
    des_cblock mSessionKey;
    des_cblock mEncInitVec;			// The send ivec
    des_cblock mDecInitVec;			// The recieved ivec
    des_key_schedule mDesSchedule;
    friend class EncryptOption;
};

#endif /* __CRYPTOENGINE_H__ */
