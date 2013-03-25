
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :  nwhashcls.h
 * DESCRIPTION  :  Equivalent of Hash class
 *                 
 * Author       :  Srivathsa M
 * Date	Created :  July 26 2001
 */
#include <stdio.h>
#include <conio.h>
#include <process.h>

#define BUCKET_SIZE 37

struct HASHNODE
{
	void *data;
	struct HASHNODE	*next;
};

typedef void (*HASHFORALLFUN)(void *, void *);

class NWPerlHashList
{
private:
	HASHNODE*	MemListHash[BUCKET_SIZE];
    void removeAll() const;

public:
	~NWPerlHashList();
	NWPerlHashList();
	int insert(void *lData);
	int remove(void *lData);
    void forAll( void (*)(void *, void*), void * ) const;
};

struct KEYHASHNODE
{
	void *key;
	void *data;
	KEYHASHNODE	*next;
};

/**
typedef void (*KEYHASHFORALLFUN)(void *, void *);

class NWPerlKeyHashList
{
private:
	KEYHASHNODE*	MemListHash[BUCKET_SIZE];
    void removeAll() const;

public:
	~NWPerlKeyHashList();
	NWPerlKeyHashList();
	int insert(void *key, void *lData);
	int remove(void *key);
    void forAll( void (*)(void *, void*), void * ) const;
	int find(void *key, void **pData);
};
**/

//#define DEBUG_HASH 1

#ifdef DEBUG_HASH
#define DEBUGPRINT	ConsolePrintf
#else
#define DEBUGPRINT
#endif


