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

/* $KTH: CryptoEngine.cpp,v 1.3 1999/12/02 16:58:34 joda Exp $ */

#include <Windows.h>
#include "CryptoEngine.h"

CryptoEngine::CryptoEngine(des_cblock* key)
{
	for (int i = 0; i < 8; i++)
		mSessionKey[i] = (*key)[i];
	des_set_key (&mSessionKey, mDesSchedule);
	mIVEncCount = mIVDecCount = 0;
	lastIn = 0;
}

CryptoEngine::~CryptoEngine()
{
	if(lastIn)
		delete[] lastIn;
}

void
CryptoEngine::Encrypt(pUChar in, pUChar out, long length) // pUChar = unsigned char*
{
	des_cfb64_encrypt(in, out,  length, mDesSchedule, &mEncInitVec, &mIVEncCount, DES_ENCRYPT);
}

void
CryptoEngine::Decrypt(pUChar in, pUChar out, long length)
{
	// Save variables for UndoDecrypt.
	if(lastIn)
	{
		delete[] lastIn;
		lastIn = 0;
	}
	lastIn = new UChar[length];
	CopyMemory(lastIn, in, length);
	lastLength = length;
	CopyMemory(mLastDecInitVec, mDecInitVec, sizeof(mDecInitVec));
	mLastIVDecCount	= mIVDecCount;

	// Ok, Let's go cipher jkGs!".-&eEq¤*
	des_cfb64_encrypt(in, out,  length, mDesSchedule, &mDecInitVec, &mIVDecCount, DES_DECRYPT);
}


void
CryptoEngine::RandData(pUChar in, int length)
{
	for(int i=0; i<length; i+=8)
	{
		des_cblock data;
		int size = sizeof(data);
		des_generate_random_block(&data);
		if(i+size > length) size = length - i;
		CopyMemory(in+i, data, size);
	}
}

void CryptoEngine::UndoDecrypt(pUChar out, long length)
{
	if(lastIn)
	{
		// Restore variables.
		CopyMemory(mDecInitVec,mLastDecInitVec, sizeof(mDecInitVec));
		mIVDecCount = mLastIVDecCount;

		// Redo decryption, to proper length.
		des_cfb64_encrypt(	lastIn, lastIn, lastLength - length,
							mDesSchedule, &mDecInitVec, &mIVDecCount, DES_DECRYPT);
	
		// Copy clear text.
		CopyMemory(out, lastIn+lastLength-length, length);

		delete[] lastIn;
		lastIn = 0;
	}
}
