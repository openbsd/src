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
#ifndef __CHARSTREAM_H__
#define __CHARSTREAM_H__

#include <Windows.h>

struct bufNode {
	unsigned char *buf;
	unsigned int bufSize;
};
typedef struct bufNode bufNode;

class CharStream {

public:
    void CopyStream(CharStream* copy);
    BOOL AreThereMore(void);
    CharStream(unsigned int vecSize = 4);
    ~CharStream(void);
	
    void				PutChars(unsigned char *data, unsigned int size);
    BOOL				GetChar(unsigned char *c);
    void				UngetChar(void);
    void				MarkAsRead(void);
    void				RestoreToMark(void);
    BOOL				SkipTo (unsigned char c, unsigned char **data, unsigned int *size);
    BOOL				GetBuffer(unsigned char **buf, unsigned int *size);
    inline BOOL			CharExist()
    {
	return	
	    ((firstBuf >= 0) &&
	     (	((firstBuf <= currBuf) &&
		 (currBuf <= lastBuf) &&
		 (firstBuf <= lastBuf)) ||
								
		((currBuf <= firstBuf) &&
		 (currBuf <= lastBuf) &&
		 (lastBuf < firstBuf)) ||
								
		((currBuf >= firstBuf) &&
		 (currBuf >= lastBuf) &&
		 (lastBuf < firstBuf))));
    }
    BOOL bufVecFull;


private:
    BOOL AllocBufVec(unsigned __int64 VecSize);
    BOOL mThereAreMore;
    bufNode *bufVec;
    unsigned int bufVecSize;
    int currBuf, markBuf, firstBuf, lastBuf;
    int currBufIndex, markBufIndex;

    inline void			reset(void)
    {
	bufVecFull = FALSE;
	firstBuf = lastBuf = -1;
	currBuf = markBuf = 0;
	currBufIndex = markBufIndex = 0;
	mThereAreMore = FALSE;
    };
};

#endif /* __CHARSTREAM_H__ */
