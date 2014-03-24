/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :  hashcls.cpp
 * DESCRIPTION  :  Implementation of Equivalent of Hash class, NWPerlHashList and 
					NWPerlKeyHashList
 *                 
 * Author       :  Srivathsa M
 * Date	Created :  July 26 2001
 */

#include "nwhashcls.h"

NWPerlHashList::NWPerlHashList()
{
	//initialize the hash list to null
	for(int i=0;i<BUCKET_SIZE;i++)
		MemListHash[i] = NULL;
	DEBUGPRINT("In constructor\n");
}
			
NWPerlHashList::~NWPerlHashList()
{
	DEBUGPRINT("In destructor\n");
	removeAll();
}

int
NWPerlHashList::insert(void *ldata)
{
	HASHNODE *list = new HASHNODE;
	if (list) {
		list->data = ldata;
		list->next = NULL;
		unsigned long Bucket = ((unsigned long)ldata) % BUCKET_SIZE;
		if (MemListHash[Bucket]) {
			//Elements existing, insert at the beginning
			list->next = MemListHash[Bucket];
			MemListHash[Bucket] = list;
			DEBUGPRINT("Inserted to %d\n",Bucket);
		} else {
			//First element
			MemListHash[Bucket] = list;
			DEBUGPRINT("Inserted first time to %d\n",Bucket);
		}
		return 1;
	} else 
		return 0;
}

int
NWPerlHashList::remove(void *ldata)
{
	unsigned long Bucket = ((unsigned long)ldata) % BUCKET_SIZE;
	HASHNODE *list = MemListHash[Bucket];
	if (list) {
		int found = 0;
		HASHNODE *next =list;
		HASHNODE *prev =NULL;
		do 
		{
			if (list->data != ldata) {
				prev = list;
				list = list->next;
			}
			else {
				found = 1;
				next = list->next;
				/*if(list->data)
				{
					free(list->data);
					list->data = NULL;
				}*/
				//ConsolePrintf ("A:%x;",list->data);
				delete list;
				list = NULL;
				if (prev) {
					prev->next = next;
				} else {
					MemListHash[Bucket]=next;
				}
				DEBUGPRINT("Removed element from %d\n",Bucket);
			}
			ThreadSwitchWithDelay();
		} while(list && !found);
//		if (!found)
//			ConsolePrintf("Couldn;t find %x in Bucket %d\n",ldata,Bucket);
		return(found);
	} 
	return 1;
}


void NWPerlHashList::forAll( void (*user_fn)(void *, void*), void *data ) const
{

	for(int i=0; i<BUCKET_SIZE; i++) 
	{
		HASHNODE *next = MemListHash[i];
		while(next)
		{
			HASHNODE *temp = next->next;
			if(next->data)
			{
				DEBUGPRINT("- To remove element from bucket %d\n",i);
	            user_fn( next->data, data );
			}
			next = temp;
			ThreadSwitchWithDelay();
		}
	}
	return ;
};

void NWPerlHashList::removeAll( ) const 
{

	for(int i=0; i<BUCKET_SIZE; i++) 
	{
		HASHNODE *next = MemListHash[i];
		while(next)
		{
			HASHNODE *temp = next->next;
			delete next;
			next = temp;
			ThreadSwitchWithDelay();
		}
	}
	return ;
};

/**
NWPerlKeyHashList::NWPerlKeyHashList()
{
	//initialize the hash list to null
	for(int i=0;i<BUCKET_SIZE;i++)
		MemListHash[i] = NULL;
	DEBUGPRINT("In constructor\n");
}
			
NWPerlKeyHashList::~NWPerlKeyHashList()
{
	DEBUGPRINT("In destructor\n");
	removeAll();
}

int
NWPerlKeyHashList::insert(void *key, void *ldata)
{
	KEYHASHNODE *list = new KEYHASHNODE;
	if (list) {
		list->key = key;
		list->data = ldata;
		list->next = NULL;
		unsigned long Bucket = ((unsigned long)key) % BUCKET_SIZE;
		if (MemListHash[Bucket]) {
			//Elements existing, insert at the beginning
			list->next = MemListHash[Bucket];
			MemListHash[Bucket] = list;
			DEBUGPRINT("Inserted to %d\n",Bucket);
		} else {
			//First element
			MemListHash[Bucket] = list;
			DEBUGPRINT("Inserted first time to %d\n",Bucket);
		}
		return 1;
	} else 
		return 0;
}

int
NWPerlKeyHashList::remove(void *key)
{
	unsigned long Bucket = ((unsigned long)key) % BUCKET_SIZE;
	KEYHASHNODE *list = MemListHash[Bucket];
	if (list) {
		int found = 0;
		KEYHASHNODE *next =list;
		KEYHASHNODE *prev =NULL;
		do 
		{
			if (list->key != key) {
				prev = list;
				list = list->next;
			}
			else {
				found = 1;
				next = list->next;
				delete list;
				list = NULL;
				if (prev) {
					prev->next = next;
				} else {
					MemListHash[Bucket]=next;
				}
				DEBUGPRINT("Removed element from %d\n",Bucket);
			}
		} while(list && !found);
//		if (!found)
//			ConsolePrintf("Couldn;t find %x in Bucket %d\n",key,Bucket);
		return(found);
	} 
	return 1;
}


void NWPerlKeyHashList::forAll( void (*user_fn)(void *, void*), void *data ) const
{

	for(int i=0; i<BUCKET_SIZE; i++) 
	{
		KEYHASHNODE *next = MemListHash[i];
		while(next)
		{
			KEYHASHNODE *temp = next->next;
			if(next->data)
			{
				DEBUGPRINT("- To remove element from bucket %d\n",i);
	            user_fn( next->data, data );
			}
			next = temp;
			ThreadSwitchWithDelay();
		}
	}
	return ;
};

int NWPerlKeyHashList::find(void *key,void **pData)
{
	for(int i=0; i<BUCKET_SIZE; i++) 
	{
		KEYHASHNODE *next = MemListHash[i];
		while(next)
		{
			if(next->key==key)
			{
				*pData=next->data;
				return 1;
			}
			next = next->next;
			ThreadSwitchWithDelay();
		}
	}
	return 0;
}

void NWPerlKeyHashList::removeAll( ) const 
{

	for(int i=0; i<BUCKET_SIZE; i++) 
	{
		KEYHASHNODE *next = MemListHash[i];
		while(next)
		{
			KEYHASHNODE *temp = next->next;
			delete next;
			next = temp;
			ThreadSwitchWithDelay();
		}
	}
	return ;
};
**/
