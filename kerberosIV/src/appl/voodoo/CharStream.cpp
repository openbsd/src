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

/* $KTH: CharStream.cpp,v 1.3 1999/12/02 16:58:34 joda Exp $ */

// CharStream.cpp
// Author: Jörgen Karlsson - d93-jka@nada.kth.se

#include <Windows.h>
#include "CharStream.h"

CharStream::CharStream(unsigned int vecSize)
{
	bufVecSize = 0;//vecSize;
	bufVec = 0;
	if(!AllocBufVec(vecSize))
		OutputDebugString("CharStream out of memory.");
	reset();
}

CharStream::~CharStream(void)
{
	delete []bufVec;
}


BOOL CharStream::GetChar(unsigned char *c)
{
	if(CharExist())
	{
		*c = bufVec[currBuf].buf[currBufIndex++];
		if(currBufIndex == bufVec[currBuf].bufSize)
		{
			currBuf = (currBuf+1)%bufVecSize;
			currBufIndex = 0;
		}
		return TRUE;
	}
	else 
	{
		//RestoreToMark();
		//mThereAreMore = FALSE;
		return FALSE;
	}
}

void CharStream::UngetChar(void)
{
	if(currBufIndex>markBufIndex || currBuf != markBuf)
	{
		currBufIndex--;
		if(currBufIndex<0)
		{
			if(currBuf!=firstBuf)
			{
				currBuf = (currBuf-1)%bufVecSize;
				currBufIndex = bufVec[currBuf].bufSize - 1;
			}
			else currBufIndex = 0;
		}
	}	
}

// MarkAsRead - mark everything read so far as read,
// meaning it can't be read again.
// Also frees alocated memory, if possible.
void CharStream::MarkAsRead(void)
{
	int oldCurrBuf = firstBuf;
	markBuf = currBuf;
	markBufIndex = currBufIndex;
	while(CharExist() && oldCurrBuf != currBuf)
	{
		delete [](bufVec[oldCurrBuf].buf);
		oldCurrBuf = (oldCurrBuf+1)%bufVecSize;
		bufVecFull = FALSE;
	}
	
	if(!CharExist())
		reset();
	else
		firstBuf = currBuf;
}

void CharStream::RestoreToMark(void)
{
	currBuf = markBuf;
	currBufIndex = markBufIndex;
}


void CharStream::PutChars(unsigned char *data, unsigned int size)
{
	if(size>0)
	{
		if(!bufVecFull)
		{
			unsigned int i;
			
			// Insert new data. 
			unsigned char *newData = new unsigned char[size];
			if(!newData)
			{
				OutputDebugString("CharStream::PutChars: Out of memory.\n");
				exit(1);
			}
			for(i=0;i<size;i++)
				newData[i] = data[i];
			int nextBuf = (lastBuf + 1) % bufVecSize;
			bufVec[nextBuf].buf = newData;
			bufVec[nextBuf].bufSize = size;

			// Make new data available for reading.
			lastBuf = nextBuf;
			if(firstBuf < 0) firstBuf = 0;
			if(firstBuf == ((lastBuf+1)%bufVecSize))
				bufVecFull = TRUE;
			mThereAreMore = TRUE;
		}
//		else
			/*if(!AllocBufVec(2*bufVecSize))
			{
				OutputDebugString("CharStream::PutChars: bufVecFull.");
				exit(1);
			}
			else*/
			//PutChars(data, size);
	}
}

//----------------------------------------------------------------------------------
// SkipTo - Skip characters in stream until char c is found or end of current buffer
// Returns true if char c was found, false otherwise
// Skipped chars is found in data, and number of chars skipped is put in size

// Attention!!!
//		If stream is empty, SkipTo returns false and size == 0.
//		*data points to new memory that must be freed.
//----------------------------------------------------------------------------------
BOOL CharStream::SkipTo (unsigned char c, unsigned char **data, unsigned int *size) 
{
	BOOL WasFound = FALSE;
	int oldBufIndex = currBufIndex;
	if(CharExist())
	{
		while(currBufIndex < bufVec[currBuf].bufSize)
		{
			if (bufVec[currBuf].buf[currBufIndex] == c)
			{
				WasFound = TRUE;
				break;
			}
			currBufIndex++;
		}
		
		// Set return values.
		*size = currBufIndex - oldBufIndex;
		*data = new unsigned char[*size]; 
		CopyMemory(*data, bufVec[currBuf].buf+oldBufIndex, *size);
		
		// Update internal data.
		if(!WasFound) // If we read to end of buffer, set next buffer as current.
		{
			currBuf = (currBuf+1)%bufVecSize;
			currBufIndex = 0;
		}
		MarkAsRead();
	}
	else
	{
		*data = 0;
		*size = 0;
	}
	return WasFound;
}

//-----------------------------------------------------------------
// GetBuffer - Get all of current buffer, without mark as read.
// Returns true if char exists, false otherwise
//-----------------------------------------------------------------
BOOL CharStream::GetBuffer(unsigned char **buf, unsigned int *size)
{
	if(CharExist())
	{
 		*buf = bufVec[currBuf].buf+currBufIndex;
		*size = bufVec[currBuf].bufSize - currBufIndex;
		currBuf = (currBuf+1)%bufVecSize;
		currBufIndex = 0;
		return TRUE;
	}
	else
	{
		*buf = NULL;
		*size = 0;
		return FALSE;
	}
}

// AreThereMore - returns false if getchar ran into end of stream.
// This can only be checked once, next time AreThereMore returns true.
// Attention: Only use these function together with getchar.
// No other functions sets the result value of AreThereMore.
// (This will be changed in the future.)
BOOL CharStream::AreThereMore(void)
{
	BOOL res = mThereAreMore;
	mThereAreMore = TRUE;
	return res;
}

BOOL CharStream::AllocBufVec(unsigned __int64 VecSize)
{
	if(bufVecSize && bufVec) return TRUE;
	if(VecSize >= ((unsigned __int64)1)<<(sizeof(bufVecSize)*8))
		return FALSE; 

	bufNode *newBufVec = new bufNode[VecSize];
	if(!newBufVec)
		return FALSE;

	bufVec = newBufVec; 
	bufVecSize = (unsigned int)VecSize;
	bufVecFull = FALSE;
	return TRUE;
}

void CharStream::CopyStream(CharStream* copy)
{
	unsigned char* buffer;
	unsigned int size;
	while(GetBuffer(&buffer, &size))
	{
		copy->PutChars(buffer, size);
		MarkAsRead();
	}
}
