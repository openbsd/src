/*
 * $LynxId: HTVMS_WaisProt.c,v 1.9 2010/09/24 23:51:22 tom Exp $
 *
 *							  HTVMS_WAISProt.c
 *
 *	Adaptation for Lynx by F.Macrides (macrides@sci.wfeb.edu)
 *
 *	31-May-1994 FM	Initial version.
 *
 *----------------------------------------------------------------------*/

/*
 *	Routines originally from WProt.c -- FM
 *
 *----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE:
 * No guarantees or restrictions.  See the readme file for the full standard
 * disclaimer.

 * 3.26.90	Harry Morris, morris@think.com
 * 3.30.90  Harry Morris
 * -	removed chunk code from WAISSearchAPDU,
 * -	added makeWAISQueryType1Query() and readWAISType1Query() which replace
 * makeWAISQueryTerms() and makeWAISQueryDocs().
 * 4.11.90  HWM - generalized conditional includes (see c-dialect.h)
 * - renamed makeWAISType1Query() to makeWAISTextQuery()
 * renamed readWAISType1Query() to readWAISTextQuery()
 * 5.29.90  TS - fixed bug in makeWAISQueryDocs
 * added CSTFreeWAISFoo functions
 */

#define _C_WAIS_protocol_

/*	This file implements the Z39.50 extensions required for WAIS
*/

#include <HTUtils.h>
#include <HTVMS_WaisUI.h>
#include <HTVMS_WaisProt.h>

#include <LYLeaks.h>

/* very rough estimates of the size of an object */
#define DefWAISInitResponseSize		(size_t)200
#define DefWAISSearchSize			(size_t)3000
#define DefWAISSearchResponseSize	(size_t)6000
#define DefWAISPresentSize			(size_t)1000
#define DefWAISPresentResponseSize	(size_t)6000
#define DefWAISDocHeaderSize		(size_t)500
#define DefWAISShortHeaderSize		(size_t)200
#define DefWAISLongHeaderSize		(size_t)800
#define DefWAISDocTextSize			(size_t)6000
#define DefWAISDocHeadlineSize		(size_t)500
#define DefWAISDocCodeSize			(size_t)500

#define RESERVE_SPACE_FOR_WAIS_HEADER(len)	\
     if (*len > 0)				\
	*len -= header_len;

#define S_MALLOC(type) (type*)s_malloc(sizeof(type))
#define S_MALLOC2(type) (type*)s_malloc(sizeof(type) * 2)

#define S_REALLOC2(type, ptr, num) (type*)s_realloc((char*)ptr, (sizeof(type) * (num + 2)))

/*----------------------------------------------------------------------*/

static unsigned long userInfoTagSize(data_tag tag,
				     unsigned long length)
/* return the number of bytes required to write the user info tag and
   length
 */
{
    unsigned long size;

    /* calculate bytes required to represent tag.  max tag is 16K */
    size = writtenCompressedIntSize(tag);
    size += writtenCompressedIntSize(length);

    return (size);
}

/*----------------------------------------------------------------------*/

static char *writeUserInfoHeader(data_tag tag,
				 long infoSize,
				 long estHeaderSize,
				 char *buffer,
				 long *len)
/* write the tag and size, making sure the info fits.  return the true end
   of the info (after adjustment) note that the argument infoSize includes
   estHeaderSize.  Note that the argument len is the number of bytes remaining
   in the buffer.  Since we write the tag and size at the begining of the
   buffer (in space that we reserved) we don't want to pass len the calls which
   do that writing.
 */
{
    long dummyLen = 100;	/* plenty of space for a tag and size */
    char *buf = buffer;
    long realSize = infoSize - estHeaderSize;
    long realHeaderSize = userInfoTagSize(tag, realSize);

    if (buffer == NULL || *len == 0)
	return (NULL);

    /* write the tag */
    buf = writeTag(tag, buf, &dummyLen);

    /* see if the if the header size was correct. if not,
       we have to shift the info to fit the real header size */
    if (estHeaderSize != realHeaderSize) {	/* make sure there is enough space */
	CHECK_FOR_SPACE_LEFT(realHeaderSize - estHeaderSize, len);
	memmove(buffer + realHeaderSize, buffer + estHeaderSize, (size_t) (realSize));
    }

    /* write the size */
    writeCompressedInteger(realSize, buf, &dummyLen);

    /* return the true end of buffer */
    return (buffer + realHeaderSize + realSize);
}

/*----------------------------------------------------------------------*/

static char *readUserInfoHeader(data_tag *tag,
				unsigned long *num,
				char *buffer)
/* read the tag and size */
{
    char *buf = buffer;

    buf = readTag(tag, buf);
    buf = readCompressedInteger(num, buf);
    return (buf);
}

/*----------------------------------------------------------------------*/

WAISInitResponse *makeWAISInitResponse(long chunkCode,
				       long chunkIDLen,
				       char *chunkMarker,
				       char *highlightMarker,
				       char *deHighlightMarker,
				       char *newLineChars)
/* create a WAIS init response object */
{
    WAISInitResponse *init = S_MALLOC(WAISInitResponse);

    init->ChunkCode = chunkCode;	/* note: none are copied! */
    init->ChunkIDLength = chunkIDLen;
    init->ChunkMarker = chunkMarker;
    init->HighlightMarker = highlightMarker;
    init->DeHighlightMarker = deHighlightMarker;
    init->NewlineCharacters = newLineChars;

    return (init);
}

/*----------------------------------------------------------------------*/

void freeWAISInitResponse(WAISInitResponse *init)
/* free an object made with makeWAISInitResponse */
{
    s_free(init->ChunkMarker);
    s_free(init->HighlightMarker);
    s_free(init->DeHighlightMarker);
    s_free(init->NewlineCharacters);
    s_free(init);
}

/*----------------------------------------------------------------------*/

char *writeInitResponseInfo(InitResponseAPDU *init,
			    char *buffer,
			    long *len)
/* write an init response object */
{
    unsigned long header_len = userInfoTagSize(DT_UserInformationLength,
					       DefWAISInitResponseSize);
    char *buf = buffer + header_len;
    WAISInitResponse *info = (WAISInitResponse *) init->UserInformationField;
    unsigned long size;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeNum(info->ChunkCode, DT_ChunkCode, buf, len);
    buf = writeNum(info->ChunkIDLength, DT_ChunkIDLength, buf, len);
    buf = writeString(info->ChunkMarker, DT_ChunkMarker, buf, len);
    buf = writeString(info->HighlightMarker, DT_HighlightMarker, buf, len);
    buf = writeString(info->DeHighlightMarker, DT_DeHighlightMarker, buf, len);
    buf = writeString(info->NewlineCharacters, DT_NewlineCharacters, buf, len);

    /* now write the header and size */
    size = buf - buffer;
    buf = writeUserInfoHeader(DT_UserInformationLength,
			      size,
			      header_len,
			      buffer,
			      len);

    return (buf);
}

/*----------------------------------------------------------------------*/

char *readInitResponseInfo(void **info,
			   char *buffer)
/* read an init response object */
{
    char *buf = buffer;
    unsigned long size;
    unsigned long headerSize;
    long chunkCode, chunkIDLen;
    data_tag tag1;
    char *chunkMarker = NULL;
    char *highlightMarker = NULL;
    char *deHighlightMarker = NULL;
    char *newLineChars = NULL;

    chunkCode = chunkIDLen = UNUSED;

    buf = readUserInfoHeader(&tag1, &size, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_ChunkCode:
	    buf = readNum(&chunkCode, buf);
	    break;
	case DT_ChunkIDLength:
	    buf = readNum(&chunkIDLen, buf);
	    break;
	case DT_ChunkMarker:
	    buf = readString(&chunkMarker, buf);
	    break;
	case DT_HighlightMarker:
	    buf = readString(&highlightMarker, buf);
	    break;
	case DT_DeHighlightMarker:
	    buf = readString(&deHighlightMarker, buf);
	    break;
	case DT_NewlineCharacters:
	    buf = readString(&newLineChars, buf);
	    break;
	default:
	    s_free(highlightMarker);
	    s_free(deHighlightMarker);
	    s_free(newLineChars);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *info = (void *) makeWAISInitResponse(chunkCode, chunkIDLen, chunkMarker,
					  highlightMarker, deHighlightMarker,
					  newLineChars);
    return (buf);
}

/*----------------------------------------------------------------------*/

WAISSearch *makeWAISSearch(char *seedWords,
			   DocObj **docs,
			   char **textList,
			   long dateFactor,
			   char *beginDateRange,
			   char *endDateRange,
			   long maxDocsRetrieved)

/* create a type 3 query object */
{
    WAISSearch *query = S_MALLOC(WAISSearch);

    query->SeedWords = seedWords;	/* not copied! */
    query->Docs = docs;		/* not copied! */
    query->TextList = textList;	/* not copied! */
    query->DateFactor = dateFactor;
    query->BeginDateRange = beginDateRange;
    query->EndDateRange = endDateRange;
    query->MaxDocumentsRetrieved = maxDocsRetrieved;

    return (query);
}

/*----------------------------------------------------------------------*/

void freeWAISSearch(WAISSearch *query)

/* destroy an object made with makeWAISSearch() */
{
    void *ptr = NULL;
    long i;

    s_free(query->SeedWords);

    if (query->Docs != NULL)
	for (i = 0, ptr = (void *) query->Docs[i];
	     ptr != NULL;
	     ptr = (void *) query->Docs[++i])
	    freeDocObj((DocObj *) ptr);
    s_free(query->Docs);

    if (query->TextList != NULL)	/* XXX revisit when textlist is fully defined */
	for (i = 0, ptr = (void *) query->TextList[i];
	     ptr != NULL;
	     ptr = (void *) query->TextList[++i])
	    s_free(ptr);
    s_free(query->TextList);

    s_free(query->BeginDateRange);
    s_free(query->EndDateRange);
    s_free(query);
}

/*----------------------------------------------------------------------*/

DocObj *makeDocObjUsingWholeDocument(any *docID,
				     char *type)

/* construct a document object using byte chunks - only for use by
   servers */
{
    DocObj *doc = S_MALLOC(DocObj);

    doc->DocumentID = docID;	/* not copied! */
    doc->Type = type;		/* not copied! */
    doc->ChunkCode = CT_document;
    return (doc);
}

/*----------------------------------------------------------------------*/

DocObj *makeDocObjUsingLines(any *docID,
			     char *type,
			     long start,
			     long end)

/* construct a document object using line chunks - only for use by
   servers */
{
    DocObj *doc = S_MALLOC(DocObj);

    doc->ChunkCode = CT_line;
    doc->DocumentID = docID;	/* not copied */
    doc->Type = type;		/* not copied! */
    doc->ChunkStart.Pos = start;
    doc->ChunkEnd.Pos = end;
    return (doc);
}

/*----------------------------------------------------------------------*/

DocObj *makeDocObjUsingBytes(any *docID,
			     char *type,
			     long start,
			     long end)

/* construct a document object using byte chunks - only for use by
   servers */
{
    DocObj *doc = S_MALLOC(DocObj);

    doc->ChunkCode = CT_byte;
    doc->DocumentID = docID;	/* not copied */
    doc->Type = type;		/* not copied! */
    doc->ChunkStart.Pos = start;
    doc->ChunkEnd.Pos = end;
    return (doc);
}

/*----------------------------------------------------------------------*/

DocObj *makeDocObjUsingParagraphs(any *docID,
				  char *type,
				  any *start,
				  any *end)

/* construct a document object using byte chunks - only for use by
   servers */
{
    DocObj *doc = S_MALLOC(DocObj);

    doc->ChunkCode = CT_paragraph;
    doc->DocumentID = docID;	/* not copied */
    doc->Type = type;
    doc->ChunkStart.ID = start;
    doc->ChunkEnd.ID = end;
    return (doc);
}

/*----------------------------------------------------------------------*/

void freeDocObj(DocObj *doc)

/* free a docObj */
{
    freeAny(doc->DocumentID);
    s_free(doc->Type);
    if (doc->ChunkCode == CT_paragraph) {
	freeAny(doc->ChunkStart.ID);
	freeAny(doc->ChunkEnd.ID);
    }
    s_free(doc);
}

/*----------------------------------------------------------------------*/

static char *writeDocObj(DocObj *doc,
			 char *buffer,
			 long *len)

/* write as little as we can about the doc obj */
{
    char *buf = buffer;

    /* we alwasy have to write the id, but its tag depends on if its a chunk */
    if (doc->ChunkCode == CT_document)
	buf = writeAny(doc->DocumentID, DT_DocumentID, buf, len);
    else
	buf = writeAny(doc->DocumentID, DT_DocumentIDChunk, buf, len);

    if (doc->Type != NULL)
	buf = writeString(doc->Type, DT_TYPE, buf, len);

    switch (doc->ChunkCode) {
    case CT_document:
	/* do nothing - there is no chunk data */
	break;
    case CT_byte:
    case CT_line:
	buf = writeNum(doc->ChunkCode, DT_ChunkCode, buf, len);
	buf = writeNum(doc->ChunkStart.Pos, DT_ChunkStartID, buf, len);
	buf = writeNum(doc->ChunkEnd.Pos, DT_ChunkEndID, buf, len);
	break;
    case CT_paragraph:
	buf = writeNum(doc->ChunkCode, DT_ChunkCode, buf, len);
	buf = writeAny(doc->ChunkStart.ID, DT_ChunkStartID, buf, len);
	buf = writeAny(doc->ChunkEnd.ID, DT_ChunkEndID, buf, len);
	break;
    default:
	panic("Implementation error: unknown chuck type %ld",
	      doc->ChunkCode);
	break;
    }

    return (buf);
}

/*----------------------------------------------------------------------*/

static char *readDocObj(DocObj **doc,
			char *buffer)

/* read whatever we have about the new document */
{
    char *buf = buffer;
    data_tag tag;

    *doc = S_MALLOC(DocObj);

    tag = peekTag(buf);
    buf = readAny(&((*doc)->DocumentID), buf);

    if (tag == DT_DocumentID) {
	(*doc)->ChunkCode = CT_document;
	tag = peekTag(buf);
	if (tag == DT_TYPE)	/* XXX depends on DT_TYPE != what comes next */
	    buf = readString(&((*doc)->Type), buf);
	/* ChunkStart and ChunkEnd are undefined */
    } else if (tag == DT_DocumentIDChunk) {
	boolean readParagraphs = false;		/* for cleanup */

	tag = peekTag(buf);
	if (tag == DT_TYPE)	/* XXX depends on DT_TYPE != CT_FOO */
	    buf = readString(&((*doc)->Type), buf);
	buf = readNum(&((*doc)->ChunkCode), buf);
	switch ((*doc)->ChunkCode) {
	case CT_byte:
	case CT_line:
	    buf = readNum(&((*doc)->ChunkStart.Pos), buf);
	    buf = readNum(&((*doc)->ChunkEnd.Pos), buf);
	    break;
	case CT_paragraph:
	    buf = readAny(&((*doc)->ChunkStart.ID), buf);
	    buf = readAny(&((*doc)->ChunkEnd.ID), buf);
	    break;
	default:
	    freeAny((*doc)->DocumentID);
	    if (readParagraphs) {
		freeAny((*doc)->ChunkStart.ID);
		freeAny((*doc)->ChunkEnd.ID);
	    }
	    s_free(doc);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    } else {
	freeAny((*doc)->DocumentID);
	s_free(*doc);
	REPORT_READ_ERROR(buf);
    }
    return (buf);
}

/*----------------------------------------------------------------------*/

char *writeSearchInfo(SearchAPDU *query,
		      char *buffer,
		      long *len)

/* write out a WAIS query (type 1 or 3) */
{
    if (strcmp(query->QueryType, QT_TextRetrievalQuery) == 0) {
	return (writeAny((any *) query->Query, DT_Query, buffer, len));
    } else {
	unsigned long header_len = userInfoTagSize(DT_UserInformationLength,
						   DefWAISSearchSize);
	char *buf = buffer + header_len;
	WAISSearch *info = (WAISSearch *) query->Query;
	unsigned long size;
	long i;

	RESERVE_SPACE_FOR_WAIS_HEADER(len);

	buf = writeString(info->SeedWords, DT_SeedWords, buf, len);

	if (info->Docs != NULL) {
	    for (i = 0; info->Docs[i] != NULL; i++) {
		buf = writeDocObj(info->Docs[i], buf, len);
	    }
	}

	/* XXX text list */

	buf = writeNum(info->DateFactor,
		       DT_DateFactor,
		       buf,
		       len);
	buf = writeString(info->BeginDateRange,
			  DT_BeginDateRange,
			  buf,
			  len);
	buf = writeString(info->EndDateRange,
			  DT_EndDateRange,
			  buf,
			  len);
	buf = writeNum(info->MaxDocumentsRetrieved,
		       DT_MaxDocumentsRetrieved,
		       buf,
		       len);

	/* now write the header and size */
	size = buf - buffer;
	buf = writeUserInfoHeader(DT_UserInformationLength,
				  size,
				  header_len,
				  buffer,
				  len);

	return (buf);
    }
}

/*----------------------------------------------------------------------*/

char *readSearchInfo(void **info,
		     char *buffer)

/* read a WAIS query (type 1 or 3) */
{
    data_tag type = peekTag(buffer);

    if (type == DT_Query)	/* this is a type 1 query */
    {
	char *buf = buffer;
	any *query = NULL;

	buf = readAny(&query, buf);
	*info = (void *) query;
	return (buf);
    } else {			/* a type 3 query */
	char *buf = buffer;
	unsigned long size;
	unsigned long headerSize;
	data_tag tag1;
	char *seedWords = NULL;
	char *beginDateRange = NULL;
	char *endDateRange = NULL;
	long dateFactor, maxDocsRetrieved;
	char **textList = NULL;
	DocObj **docIDs = NULL;
	DocObj *doc = NULL;
	long docs = 0;
	long i;
	void *ptr = NULL;

	dateFactor = maxDocsRetrieved = UNUSED;

	buf = readUserInfoHeader(&tag1, &size, buf);
	headerSize = buf - buffer;

	while (buf < (buffer + size + headerSize)) {
	    data_tag tag = peekTag(buf);

	    switch (tag) {
	    case DT_SeedWords:
		buf = readString(&seedWords, buf);
		break;
	    case DT_DocumentID:
	    case DT_DocumentIDChunk:
		if (docIDs == NULL)	/* create a new doc list */
		{
		    docIDs = S_MALLOC2(DocObj *);
		} else {	/* grow the doc list */
		    docIDs = S_REALLOC2(DocObj *, docIDs, docs);
		}
		buf = readDocObj(&doc, buf);
		if (buf == NULL) {
		    s_free(seedWords);
		    s_free(beginDateRange);
		    s_free(endDateRange);
		    if (docIDs != NULL)
			for (i = 0, ptr = (void *) docIDs[i];
			     ptr != NULL;
			     ptr = (void *) docIDs[++i])
			    freeDocObj((DocObj *) ptr);
		    s_free(docIDs);
		    /* XXX should also free textlist when it is fully defined */
		}
		RETURN_ON_NULL(buf);
		docIDs[docs++] = doc;	/* put it in the list */
		docIDs[docs] = NULL;
		break;
	    case DT_TextList:
		/* XXX */
		break;
	    case DT_DateFactor:
		buf = readNum(&dateFactor, buf);
		break;
	    case DT_BeginDateRange:
		buf = readString(&beginDateRange, buf);
		break;
	    case DT_EndDateRange:
		buf = readString(&endDateRange, buf);
		break;
	    case DT_MaxDocumentsRetrieved:
		buf = readNum(&maxDocsRetrieved, buf);
		break;
	    default:
		s_free(seedWords);
		s_free(beginDateRange);
		s_free(endDateRange);
		if (docIDs != NULL)
		    for (i = 0, ptr = (void *) docIDs[i];
			 ptr != NULL;
			 ptr = (void *) docIDs[++i])
			freeDocObj((DocObj *) ptr);
		s_free(docIDs);
		/* XXX should also free textlist when it is fully defined */
		REPORT_READ_ERROR(buf);
		break;
	    }
	}

	*info = (void *) makeWAISSearch(seedWords, docIDs, textList,
					dateFactor, beginDateRange, endDateRange,
					maxDocsRetrieved);
	return (buf);
    }
}

/*----------------------------------------------------------------------*/

WAISDocumentHeader *makeWAISDocumentHeader(any *docID,
					   long versionNumber,
					   long score,
					   long bestMatch,
					   long docLen,
					   long lines,
					   char **types,
					   char *source,
					   char *date,
					   char *headline,
					   char *originCity)

/* construct a standard document header, note that no fields are copied!
   if the application needs to save these fields, it should copy them,
   or set the field in this object to NULL before freeing it.
 */
{
    WAISDocumentHeader *header = S_MALLOC(WAISDocumentHeader);

    header->DocumentID = docID;
    header->VersionNumber = versionNumber;
    header->Score = score;
    header->BestMatch = bestMatch;
    header->DocumentLength = docLen;
    header->Lines = lines;
    header->Types = types;
    header->Source = source;
    header->Date = date;
    header->Headline = headline;
    header->OriginCity = originCity;

    return (header);
}

/*----------------------------------------------------------------------*/

void freeWAISDocumentHeader(WAISDocumentHeader *header)
{
    freeAny(header->DocumentID);
    doList((void **) header->Types, fs_free);	/* can't use the macro here ! */
    s_free(header->Types);
    s_free(header->Source);
    s_free(header->Date);
    s_free(header->Headline);
    s_free(header->OriginCity);
    s_free(header);
}

/*----------------------------------------------------------------------*/

char *writeWAISDocumentHeader(WAISDocumentHeader *header, char *buffer,
			      long *len)
{
    unsigned long header_len = userInfoTagSize(DT_DocumentHeaderGroup,
					       DefWAISDocHeaderSize);
    char *buf = buffer + header_len;
    unsigned long size1;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeAny(header->DocumentID, DT_DocumentID, buf, len);
    buf = writeNum(header->VersionNumber, DT_VersionNumber, buf, len);
    buf = writeNum(header->Score, DT_Score, buf, len);
    buf = writeNum(header->BestMatch, DT_BestMatch, buf, len);
    buf = writeNum(header->DocumentLength, DT_DocumentLength, buf, len);
    buf = writeNum(header->Lines, DT_Lines, buf, len);
    if (header->Types != NULL) {
	long size;
	char *ptr = NULL;
	long i;

	buf = writeTag(DT_TYPE_BLOCK, buf, len);
	for (i = 0, size = 0, ptr = header->Types[i];
	     ptr != NULL;
	     ptr = header->Types[++i]) {
	    long typeSize = strlen(ptr);

	    size += writtenTagSize(DT_TYPE);
	    size += writtenCompressedIntSize(typeSize);
	    size += typeSize;
	}
	buf = writeCompressedInteger((unsigned long) size, buf, len);
	for (i = 0, ptr = header->Types[i]; ptr != NULL; ptr = header->Types[++i])
	    buf = writeString(ptr, DT_TYPE, buf, len);
    }
    buf = writeString(header->Source, DT_Source, buf, len);
    buf = writeString(header->Date, DT_Date, buf, len);
    buf = writeString(header->Headline, DT_Headline, buf, len);
    buf = writeString(header->OriginCity, DT_OriginCity, buf, len);

    /* now write the header and size */
    size1 = buf - buffer;
    buf = writeUserInfoHeader(DT_DocumentHeaderGroup,
			      size1,
			      header_len,
			      buffer,
			      len);

    return (buf);
}

/*----------------------------------------------------------------------*/

char *readWAISDocumentHeader(WAISDocumentHeader **header, char *buffer)
{
    char *buf = buffer;
    unsigned long size1;
    unsigned long headerSize;
    data_tag tag1;
    any *docID = NULL;
    long versionNumber, score, bestMatch, docLength, lines;
    char **types = NULL;
    char *source = NULL;
    char *date = NULL;
    char *headline = NULL;
    char *originCity = NULL;

    versionNumber = score = bestMatch = docLength = lines = UNUSED;

    buf = readUserInfoHeader(&tag1, &size1, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size1 + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_DocumentID:
	    buf = readAny(&docID, buf);
	    break;
	case DT_VersionNumber:
	    buf = readNum(&versionNumber, buf);
	    break;
	case DT_Score:
	    buf = readNum(&score, buf);
	    break;
	case DT_BestMatch:
	    buf = readNum(&bestMatch, buf);
	    break;
	case DT_DocumentLength:
	    buf = readNum(&docLength, buf);
	    break;
	case DT_Lines:
	    buf = readNum(&lines, buf);
	    break;
	case DT_TYPE_BLOCK:
	    {
		unsigned long size = -1;
		long numTypes = 0;

		buf = readTag(&tag, buf);
		buf = readCompressedInteger(&size, buf);
		while (size > 0) {
		    char *type = NULL;
		    char *originalBuf = buf;

		    buf = readString(&type, buf);
		    types = S_REALLOC2(char *, types, numTypes);

		    types[numTypes++] = type;
		    types[numTypes] = NULL;
		    size -= (buf - originalBuf);
		}
	    }
	    /* FALLTHRU */
	case DT_Source:
	    buf = readString(&source, buf);
	    break;
	case DT_Date:
	    buf = readString(&date, buf);
	    break;
	case DT_Headline:
	    buf = readString(&headline, buf);
	    break;
	case DT_OriginCity:
	    buf = readString(&originCity, buf);
	    break;
	default:
	    freeAny(docID);
	    s_free(source);
	    s_free(date);
	    s_free(headline);
	    s_free(originCity);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *header = makeWAISDocumentHeader(docID, versionNumber, score, bestMatch,
				     docLength, lines, types, source, date, headline,
				     originCity);
    return (buf);
}

/*----------------------------------------------------------------------*/

WAISDocumentShortHeader *makeWAISDocumentShortHeader(any *docID,
						     long versionNumber,
						     long score,
						     long bestMatch,
						     long docLen,
						     long lines)
/* construct a short document header, note that no fields are copied!
   if the application needs to save these fields, it should copy them,
   or set the field in this object to NULL before freeing it.
 */
{
    WAISDocumentShortHeader *header = S_MALLOC(WAISDocumentShortHeader);

    header->DocumentID = docID;
    header->VersionNumber = versionNumber;
    header->Score = score;
    header->BestMatch = bestMatch;
    header->DocumentLength = docLen;
    header->Lines = lines;

    return (header);
}

/*----------------------------------------------------------------------*/

void freeWAISDocumentShortHeader(WAISDocumentShortHeader *header)
{
    freeAny(header->DocumentID);
    s_free(header);
}

/*----------------------------------------------------------------------*/

char *writeWAISDocumentShortHeader(WAISDocumentShortHeader *header, char *buffer,
				   long *len)
{
    unsigned long header_len = userInfoTagSize(DT_DocumentShortHeaderGroup,
					       DefWAISShortHeaderSize);
    char *buf = buffer + header_len;
    unsigned long size;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeAny(header->DocumentID, DT_DocumentID, buf, len);
    buf = writeNum(header->VersionNumber, DT_VersionNumber, buf, len);
    buf = writeNum(header->Score, DT_Score, buf, len);
    buf = writeNum(header->BestMatch, DT_BestMatch, buf, len);
    buf = writeNum(header->DocumentLength, DT_DocumentLength, buf, len);
    buf = writeNum(header->Lines, DT_Lines, buf, len);

    /* now write the header and size */
    size = buf - buffer;
    buf = writeUserInfoHeader(DT_DocumentShortHeaderGroup,
			      size,
			      header_len,
			      buffer,
			      len);

    return (buf);
}

/*----------------------------------------------------------------------*/

char *readWAISDocumentShortHeader(WAISDocumentShortHeader **header, char *buffer)
{
    char *buf = buffer;
    unsigned long size;
    unsigned long headerSize;
    data_tag tag1;
    any *docID = NULL;
    long versionNumber, score, bestMatch, docLength, lines;

    versionNumber = score = bestMatch = docLength = lines = UNUSED;

    buf = readUserInfoHeader(&tag1, &size, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_DocumentID:
	    buf = readAny(&docID, buf);
	    break;
	case DT_VersionNumber:
	    buf = readNum(&versionNumber, buf);
	    break;
	case DT_Score:
	    buf = readNum(&score, buf);
	    break;
	case DT_BestMatch:
	    buf = readNum(&bestMatch, buf);
	    break;
	case DT_DocumentLength:
	    buf = readNum(&docLength, buf);
	    break;
	case DT_Lines:
	    buf = readNum(&lines, buf);
	    break;
	default:
	    freeAny(docID);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *header = makeWAISDocumentShortHeader(docID, versionNumber, score, bestMatch,
					  docLength, lines);
    return (buf);
}

/*----------------------------------------------------------------------*/

WAISDocumentLongHeader *makeWAISDocumentLongHeader(any *docID,
						   long versionNumber,
						   long score,
						   long bestMatch,
						   long docLen,
						   long lines,
						   char **types,
						   char *source,
						   char *date,
						   char *headline,
						   char *originCity,
						   char *stockCodes,
						   char *companyCodes,
						   char *industryCodes)
/* construct a long document header, note that no fields are copied!
   if the application needs to save these fields, it should copy them,
   or set the field in this object to NULL before freeing it.
 */
{
    WAISDocumentLongHeader *header = S_MALLOC(WAISDocumentLongHeader);

    header->DocumentID = docID;
    header->VersionNumber = versionNumber;
    header->Score = score;
    header->BestMatch = bestMatch;
    header->DocumentLength = docLen;
    header->Lines = lines;
    header->Types = types;
    header->Source = source;
    header->Date = date;
    header->Headline = headline;
    header->OriginCity = originCity;
    header->StockCodes = stockCodes;
    header->CompanyCodes = companyCodes;
    header->IndustryCodes = industryCodes;

    return (header);
}

/*----------------------------------------------------------------------*/

void freeWAISDocumentLongHeader(WAISDocumentLongHeader *header)
{
    freeAny(header->DocumentID);
    doList((void **) header->Types, fs_free);	/* can't use the macro here! */
    s_free(header->Source);
    s_free(header->Date);
    s_free(header->Headline);
    s_free(header->OriginCity);
    s_free(header->StockCodes);
    s_free(header->CompanyCodes);
    s_free(header->IndustryCodes);
    s_free(header);
}

/*----------------------------------------------------------------------*/

char *writeWAISDocumentLongHeader(WAISDocumentLongHeader *header, char *buffer,
				  long *len)
{
    unsigned long header_len = userInfoTagSize(DT_DocumentLongHeaderGroup,
					       DefWAISLongHeaderSize);
    char *buf = buffer + header_len;
    unsigned long size1;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeAny(header->DocumentID, DT_DocumentID, buf, len);
    buf = writeNum(header->VersionNumber, DT_VersionNumber, buf, len);
    buf = writeNum(header->Score, DT_Score, buf, len);
    buf = writeNum(header->BestMatch, DT_BestMatch, buf, len);
    buf = writeNum(header->DocumentLength, DT_DocumentLength, buf, len);
    buf = writeNum(header->Lines, DT_Lines, buf, len);
    if (header->Types != NULL) {
	long size;
	char *ptr = NULL;
	long i;

	buf = writeTag(DT_TYPE_BLOCK, buf, len);
	for (i = 0, size = 0, ptr = header->Types[i];
	     ptr != NULL;
	     ptr = header->Types[++i]) {
	    long typeSize = strlen(ptr);

	    size += writtenTagSize(DT_TYPE);
	    size += writtenCompressedIntSize(typeSize);
	    size += typeSize;
	}
	buf = writeCompressedInteger((unsigned long) size, buf, len);
	for (i = 0, ptr = header->Types[i]; ptr != NULL; ptr = header->Types[++i])
	    buf = writeString(ptr, DT_TYPE, buf, len);
    }
    buf = writeString(header->Source, DT_Source, buf, len);
    buf = writeString(header->Date, DT_Date, buf, len);
    buf = writeString(header->Headline, DT_Headline, buf, len);
    buf = writeString(header->OriginCity, DT_OriginCity, buf, len);
    buf = writeString(header->StockCodes, DT_StockCodes, buf, len);
    buf = writeString(header->CompanyCodes, DT_CompanyCodes, buf, len);
    buf = writeString(header->IndustryCodes, DT_IndustryCodes, buf, len);

    /* now write the header and size */
    size1 = buf - buffer;
    buf = writeUserInfoHeader(DT_DocumentLongHeaderGroup,
			      size1,
			      header_len,
			      buffer,
			      len);

    return (buf);
}

/*----------------------------------------------------------------------*/

char *readWAISDocumentLongHeader(WAISDocumentLongHeader **header, char *buffer)
{
    char *buf = buffer;
    unsigned long size1;
    unsigned long headerSize;
    data_tag tag1;
    any *docID;
    long versionNumber, score, bestMatch, docLength, lines;
    char **types;
    char *source, *date, *headline, *originCity, *stockCodes, *companyCodes, *industryCodes;

    docID = NULL;
    versionNumber =
	score =
	bestMatch =
	docLength =
	lines = UNUSED;
    types = NULL;
    source =
	date =
	headline =
	originCity =
	stockCodes =
	companyCodes =
	industryCodes = NULL;

    buf = readUserInfoHeader(&tag1, &size1, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size1 + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_DocumentID:
	    buf = readAny(&docID, buf);
	    break;
	case DT_VersionNumber:
	    buf = readNum(&versionNumber, buf);
	    break;
	case DT_Score:
	    buf = readNum(&score, buf);
	    break;
	case DT_BestMatch:
	    buf = readNum(&bestMatch, buf);
	    break;
	case DT_DocumentLength:
	    buf = readNum(&docLength, buf);
	    break;
	case DT_Lines:
	    buf = readNum(&lines, buf);
	    break;
	case DT_TYPE_BLOCK:
	    {
		unsigned long size = -1;
		long numTypes = 0;

		buf = readTag(&tag, buf);
		readCompressedInteger(&size, buf);
		while (size > 0) {
		    char *type = NULL;
		    char *originalBuf = buf;

		    buf = readString(&type, buf);
		    types = S_REALLOC2(char *, types, numTypes);

		    types[numTypes++] = type;
		    types[numTypes] = NULL;
		    size -= (buf - originalBuf);
		}
	    }
	    /* FALLTHRU */
	case DT_Source:
	    buf = readString(&source, buf);
	    break;
	case DT_Date:
	    buf = readString(&date, buf);
	    break;
	case DT_Headline:
	    buf = readString(&headline, buf);
	    break;
	case DT_OriginCity:
	    buf = readString(&originCity, buf);
	    break;
	case DT_StockCodes:
	    buf = readString(&stockCodes, buf);
	    break;
	case DT_CompanyCodes:
	    buf = readString(&companyCodes, buf);
	    break;
	case DT_IndustryCodes:
	    buf = readString(&industryCodes, buf);
	    break;
	default:
	    freeAny(docID);
	    s_free(source);
	    s_free(date);
	    s_free(headline);
	    s_free(originCity);
	    s_free(stockCodes);
	    s_free(companyCodes);
	    s_free(industryCodes);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *header = makeWAISDocumentLongHeader(docID,
					 versionNumber,
					 score,
					 bestMatch,
					 docLength,
					 lines,
					 types,
					 source,
					 date,
					 headline,
					 originCity,
					 stockCodes,
					 companyCodes,
					 industryCodes);
    return (buf);
}

/*----------------------------------------------------------------------*/

WAISSearchResponse *
  makeWAISSearchResponse(
			    char *seedWordsUsed,
			    WAISDocumentHeader **docHeaders,
			    WAISDocumentShortHeader **shortHeaders,
			    WAISDocumentLongHeader **longHeaders,
			    WAISDocumentText **text,
			    WAISDocumentHeadlines **headlines,
			    WAISDocumentCodes **codes,
			    diagnosticRecord ** diagnostics)
{
    WAISSearchResponse *response = S_MALLOC(WAISSearchResponse);

    response->SeedWordsUsed = seedWordsUsed;
    response->DocHeaders = docHeaders;
    response->ShortHeaders = shortHeaders;
    response->LongHeaders = longHeaders;
    response->Text = text;
    response->Headlines = headlines;
    response->Codes = codes;
    response->Diagnostics = diagnostics;

    return (response);
}

/*----------------------------------------------------------------------*/

void freeWAISSearchResponse(WAISSearchResponse * response)
{
    void *ptr = NULL;
    long i;

    s_free(response->SeedWordsUsed);

    if (response->DocHeaders != NULL)
	for (i = 0, ptr = (void *) response->DocHeaders[i];
	     ptr != NULL;
	     ptr = (void *) response->DocHeaders[++i])
	    freeWAISDocumentHeader((WAISDocumentHeader *) ptr);
    s_free(response->DocHeaders);

    if (response->ShortHeaders != NULL)
	for (i = 0, ptr = (void *) response->ShortHeaders[i];
	     ptr != NULL;
	     ptr = (void *) response->ShortHeaders[++i])
	    freeWAISDocumentShortHeader((WAISDocumentShortHeader *) ptr);
    s_free(response->ShortHeaders);

    if (response->LongHeaders != NULL)
	for (i = 0, ptr = (void *) response->LongHeaders[i];
	     ptr != NULL;
	     ptr = (void *) response->LongHeaders[++i])
	    freeWAISDocumentLongHeader((WAISDocumentLongHeader *) ptr);
    s_free(response->LongHeaders);

    if (response->Text != NULL)
	for (i = 0, ptr = (void *) response->Text[i];
	     ptr != NULL;
	     ptr = (void *) response->Text[++i])
	    freeWAISDocumentText((WAISDocumentText *) ptr);
    s_free(response->Text);

    if (response->Headlines != NULL)
	for (i = 0, ptr = (void *) response->Headlines[i];
	     ptr != NULL;
	     ptr = (void *) response->Headlines[++i])
	    freeWAISDocumentHeadlines((WAISDocumentHeadlines *) ptr);
    s_free(response->Headlines);

    if (response->Codes != NULL)
	for (i = 0, ptr = (void *) response->Codes[i];
	     ptr != NULL;
	     ptr = (void *) response->Codes[++i])
	    freeWAISDocumentCodes((WAISDocumentCodes *) ptr);
    s_free(response->Codes);

    if (response->Diagnostics != NULL)
	for (i = 0, ptr = (void *) response->Diagnostics[i];
	     ptr != NULL;
	     ptr = (void *) response->Diagnostics[++i])
	    freeDiag((diagnosticRecord *) ptr);
    s_free(response->Diagnostics);

    s_free(response);
}

/*----------------------------------------------------------------------*/

char *writeSearchResponseInfo(SearchResponseAPDU *query,
			      char *buffer,
			      long *len)
{
    unsigned long header_len = userInfoTagSize(DT_UserInformationLength,
					       DefWAISSearchResponseSize);
    char *buf = buffer + header_len;
    WAISSearchResponse *info = (WAISSearchResponse *) query->DatabaseDiagnosticRecords;
    unsigned long size;
    void *header = NULL;
    long i;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeString(info->SeedWordsUsed, DT_SeedWordsUsed, buf, len);

    /* write out all the headers */
    if (info->DocHeaders != NULL) {
	for (i = 0, header = (void *) info->DocHeaders[i];
	     header != NULL;
	     header = (void *) info->DocHeaders[++i])
	    buf = writeWAISDocumentHeader((WAISDocumentHeader *) header, buf, len);
    }

    if (info->ShortHeaders != NULL) {
	for (i = 0, header = (void *) info->ShortHeaders[i];
	     header != NULL;
	     header = (void *) info->ShortHeaders[++i])
	    buf = writeWAISDocumentShortHeader((WAISDocumentShortHeader *) header,
					       buf,
					       len);
    }

    if (info->LongHeaders != NULL) {
	for (i = 0, header = (void *) info->LongHeaders[i];
	     header != NULL;
	     header = (void *) info->LongHeaders[++i])
	    buf = writeWAISDocumentLongHeader((WAISDocumentLongHeader *) header,
					      buf,
					      len);
    }

    if (info->Text != NULL) {
	for (i = 0, header = (void *) info->Text[i];
	     header != NULL;
	     header = (void *) info->Text[++i])
	    buf = writeWAISDocumentText((WAISDocumentText *) header, buf, len);
    }

    if (info->Headlines != NULL) {
	for (i = 0, header = (void *) info->Headlines[i];
	     header != NULL;
	     header = (void *) info->Headlines[++i])
	    buf = writeWAISDocumentHeadlines((WAISDocumentHeadlines *) header,
					     buf,
					     len);
    }

    if (info->Codes != NULL) {
	for (i = 0, header = (void *) info->Codes[i];
	     header != NULL;
	     header = (void *) info->Codes[++i])
	    buf = writeWAISDocumentCodes((WAISDocumentCodes *) header, buf, len);
    }

    if (info->Diagnostics != NULL) {
	for (i = 0, header = (void *) info->Diagnostics[i];
	     header != NULL;
	     header = (void *) info->Diagnostics[++i])
	    buf = writeDiag((diagnosticRecord *) header, buf, len);
    }

    /* now write the header and size */
    size = buf - buffer;
    buf = writeUserInfoHeader(DT_UserInformationLength,
			      size,
			      header_len,
			      buffer,
			      len);

    return (buf);
}

/*----------------------------------------------------------------------*/

static void cleanUpWaisSearchResponse(char *buf,
				      char *seedWordsUsed,
				      WAISDocumentHeader **docHeaders,
				      WAISDocumentShortHeader **shortHeaders,
				      WAISDocumentLongHeader **longHeaders,
				      WAISDocumentText **text,
				      WAISDocumentHeadlines **headlines,
				      WAISDocumentCodes **codes,
				      diagnosticRecord ** diags)
/* if buf is NULL, we have just gotten a read error, and need to clean up
   any state we have built.  If not, then everything is going fine, and
   we should just hang loose
 */
{
    void *ptr = NULL;
    long i;

    if (buf == NULL) {
	s_free(seedWordsUsed);
	if (docHeaders != NULL)
	    for (i = 0, ptr = (void *) docHeaders[i]; ptr != NULL;
		 ptr = (void *) docHeaders[++i])
		freeWAISDocumentHeader((WAISDocumentHeader *) ptr);
	s_free(docHeaders);
	if (shortHeaders != NULL)
	    for (i = 0, ptr = (void *) shortHeaders[i]; ptr != NULL;
		 ptr = (void *) shortHeaders[++i])
		freeWAISDocumentShortHeader((WAISDocumentShortHeader *) ptr);
	s_free(shortHeaders);
	if (longHeaders != NULL)
	    for (i = 0, ptr = (void *) longHeaders[i]; ptr != NULL;
		 ptr = (void *) longHeaders[++i])
		freeWAISDocumentLongHeader((WAISDocumentLongHeader *) ptr);
	s_free(longHeaders);
	if (text != NULL)
	    for (i = 0, ptr = (void *) text[i];
		 ptr != NULL;
		 ptr = (void *) text[++i])
		freeWAISDocumentText((WAISDocumentText *) ptr);
	s_free(text);
	if (headlines != NULL)
	    for (i = 0, ptr = (void *) headlines[i]; ptr != NULL;
		 ptr = (void *) headlines[++i])
		freeWAISDocumentHeadlines((WAISDocumentHeadlines *) ptr);
	s_free(headlines);
	if (codes != NULL)
	    for (i = 0, ptr = (void *) codes[i]; ptr != NULL;
		 ptr = (void *) codes[++i])
		freeWAISDocumentCodes((WAISDocumentCodes *) ptr);
	s_free(codes);
	if (diags != NULL)
	    for (i = 0, ptr = (void *) diags[i]; ptr != NULL;
		 ptr = (void *) diags[++i])
		freeDiag((diagnosticRecord *) ptr);
	s_free(diags);
    }
}

/*----------------------------------------------------------------------*/

char *readSearchResponseInfo(void **info,
			     char *buffer)
{
    char *buf = buffer;
    unsigned long size;
    unsigned long headerSize;
    data_tag tag1;
    void *header = NULL;
    WAISDocumentHeader **docHeaders = NULL;
    WAISDocumentShortHeader **shortHeaders = NULL;
    WAISDocumentLongHeader **longHeaders = NULL;
    WAISDocumentText **text = NULL;
    WAISDocumentHeadlines **headlines = NULL;
    WAISDocumentCodes **codes = NULL;
    long numDocHeaders, numLongHeaders, numShortHeaders, numText, numHeadlines;
    long numCodes;
    char *seedWordsUsed = NULL;
    diagnosticRecord **diags = NULL;
    diagnosticRecord *diag = NULL;
    long numDiags = 0;

    numDocHeaders =
	numLongHeaders =
	numShortHeaders =
	numText =
	numHeadlines =
	numCodes = 0;

    buf = readUserInfoHeader(&tag1, &size, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_SeedWordsUsed:
	    buf = readString(&seedWordsUsed, buf);
	    break;
	case DT_DatabaseDiagnosticRecords:
	    if (diags == NULL)	/* create a new diag list */
	    {
		diags = S_MALLOC2(diagnosticRecord *);
	    } else {		/* grow the diag list */
		diags = S_REALLOC2(diagnosticRecord *, diags, numDiags);
	    }
	    buf = readDiag(&diag, buf);
	    diags[numDiags++] = diag;	/* put it in the list */
	    diags[numDiags] = NULL;
	    break;
	case DT_DocumentHeaderGroup:
	    if (docHeaders == NULL)	/* create a new header list */
	    {
		docHeaders = S_MALLOC2(WAISDocumentHeader *);
	    } else {		/* grow the doc list */
		docHeaders = S_REALLOC2(WAISDocumentHeader *, docHeaders, numDocHeaders);
	    }
	    buf = readWAISDocumentHeader((WAISDocumentHeader **) &header, buf);
	    cleanUpWaisSearchResponse(buf,
				      seedWordsUsed,
				      docHeaders,
				      shortHeaders,
				      longHeaders,
				      text,
				      headlines,
				      codes,
				      diags);
	    RETURN_ON_NULL(buf);
	    docHeaders[numDocHeaders++] =
		(WAISDocumentHeader *) header;	/* put it in the list */
	    docHeaders[numDocHeaders] = NULL;
	    break;
	case DT_DocumentShortHeaderGroup:
	    if (shortHeaders == NULL)	/* create a new header list */
	    {
		shortHeaders = S_MALLOC2(WAISDocumentShortHeader *);
	    } else {		/* grow the doc list */
		shortHeaders = S_REALLOC2(WAISDocumentShortHeader *,
					  shortHeaders,
					  numShortHeaders);
	    }
	    buf = readWAISDocumentShortHeader((WAISDocumentShortHeader **) &header,
					      buf);
	    cleanUpWaisSearchResponse(buf,
				      seedWordsUsed,
				      docHeaders,
				      shortHeaders,
				      longHeaders,
				      text,
				      headlines,
				      codes,
				      diags);
	    RETURN_ON_NULL(buf);
	    shortHeaders[numShortHeaders++] =
		(WAISDocumentShortHeader *) header;	/* put it in the list */
	    shortHeaders[numShortHeaders] = NULL;
	    break;
	case DT_DocumentLongHeaderGroup:
	    if (longHeaders == NULL)	/* create a new header list */
	    {
		longHeaders = S_MALLOC2(WAISDocumentLongHeader *);
	    } else {		/* grow the doc list */
		longHeaders = S_REALLOC2(WAISDocumentLongHeader *,
					 longHeaders,
					 numLongHeaders);
	    }
	    buf = readWAISDocumentLongHeader((WAISDocumentLongHeader **) &header,
					     buf);
	    cleanUpWaisSearchResponse(buf,
				      seedWordsUsed,
				      docHeaders,
				      shortHeaders,
				      longHeaders,
				      text,
				      headlines,
				      codes,
				      diags);
	    RETURN_ON_NULL(buf);
	    longHeaders[numLongHeaders++] =
		(WAISDocumentLongHeader *) header;	/* put it in the list */
	    longHeaders[numLongHeaders] = NULL;
	    break;
	case DT_DocumentTextGroup:
	    if (text == NULL)	/* create a new list */
	    {
		text = S_MALLOC2(WAISDocumentText *);
	    } else {		/* grow the list */
		text = S_REALLOC2(WAISDocumentText *, text, numText);
	    }
	    buf = readWAISDocumentText((WAISDocumentText **) &header, buf);
	    cleanUpWaisSearchResponse(buf,
				      seedWordsUsed,
				      docHeaders,
				      shortHeaders,
				      longHeaders,
				      text,
				      headlines,
				      codes,
				      diags);
	    RETURN_ON_NULL(buf);
	    text[numText++] =
		(WAISDocumentText *) header;	/* put it in the list */
	    text[numText] = NULL;
	    break;
	case DT_DocumentHeadlineGroup:
	    if (headlines == NULL)	/* create a new list */
	    {
		headlines = S_MALLOC2(WAISDocumentHeadlines *);
	    } else {		/* grow the list */
		headlines = S_REALLOC2(WAISDocumentHeadlines *, headlines, numHeadlines);
	    }
	    buf = readWAISDocumentHeadlines((WAISDocumentHeadlines **) &header,
					    buf);
	    cleanUpWaisSearchResponse(buf,
				      seedWordsUsed,
				      docHeaders,
				      shortHeaders,
				      longHeaders,
				      text,
				      headlines,
				      codes,
				      diags);
	    RETURN_ON_NULL(buf);
	    headlines[numHeadlines++] =
		(WAISDocumentHeadlines *) header;	/* put it in the list */
	    headlines[numHeadlines] = NULL;
	    break;
	case DT_DocumentCodeGroup:
	    if (codes == NULL)	/* create a new list */
	    {
		codes = S_MALLOC2(WAISDocumentCodes *);
	    } else {		/* grow the list */
		codes = S_REALLOC2(WAISDocumentCodes *, codes, numCodes);
	    }
	    buf = readWAISDocumentCodes((WAISDocumentCodes **) &header, buf);
	    cleanUpWaisSearchResponse(buf,
				      seedWordsUsed,
				      docHeaders,
				      shortHeaders,
				      longHeaders,
				      text,
				      headlines,
				      codes,
				      diags);
	    RETURN_ON_NULL(buf);
	    codes[numCodes++] =
		(WAISDocumentCodes *) header;	/* put it in the list */
	    codes[numCodes] = NULL;
	    break;
	default:
	    cleanUpWaisSearchResponse(buf,
				      seedWordsUsed,
				      docHeaders,
				      shortHeaders,
				      longHeaders,
				      text,
				      headlines,
				      codes,
				      diags);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *info = (void *) makeWAISSearchResponse(seedWordsUsed,
					    docHeaders,
					    shortHeaders,
					    longHeaders,
					    text,
					    headlines,
					    codes,
					    diags);

    return (buf);
}

/*----------------------------------------------------------------------*/

WAISDocumentText *makeWAISDocumentText(any *docID,
				       long versionNumber,
				       any *documentText)
{
    WAISDocumentText *docText = S_MALLOC(WAISDocumentText);

    docText->DocumentID = docID;
    docText->VersionNumber = versionNumber;
    docText->DocumentText = documentText;

    return (docText);
}

/*----------------------------------------------------------------------*/

void freeWAISDocumentText(WAISDocumentText *docText)
{
    freeAny(docText->DocumentID);
    freeAny(docText->DocumentText);
    s_free(docText);
}

/*----------------------------------------------------------------------*/

char *writeWAISDocumentText(WAISDocumentText *docText, char *buffer,
			    long *len)
{
    unsigned long header_len = userInfoTagSize(DT_DocumentTextGroup,
					       DefWAISDocTextSize);
    char *buf = buffer + header_len;
    unsigned long size;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeAny(docText->DocumentID, DT_DocumentID, buf, len);
    buf = writeNum(docText->VersionNumber, DT_VersionNumber, buf, len);
    buf = writeAny(docText->DocumentText, DT_DocumentText, buf, len);

    /* now write the header and size */
    size = buf - buffer;
    buf = writeUserInfoHeader(DT_DocumentTextGroup, size, header_len, buffer, len);

    return (buf);
}

/*----------------------------------------------------------------------*/

char *readWAISDocumentText(WAISDocumentText **docText, char *buffer)
{
    char *buf = buffer;
    unsigned long size;
    unsigned long headerSize;
    data_tag tag1;
    any *docID, *documentText;
    long versionNumber;

    docID = documentText = NULL;
    versionNumber = UNUSED;

    buf = readUserInfoHeader(&tag1, &size, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_DocumentID:
	    buf = readAny(&docID, buf);
	    break;
	case DT_VersionNumber:
	    buf = readNum(&versionNumber, buf);
	    break;
	case DT_DocumentText:
	    buf = readAny(&documentText, buf);
	    break;
	default:
	    freeAny(docID);
	    freeAny(documentText);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *docText = makeWAISDocumentText(docID, versionNumber, documentText);
    return (buf);
}

/*----------------------------------------------------------------------*/

WAISDocumentHeadlines *makeWAISDocumentHeadlines(any *docID,
						 long versionNumber,
						 char *source,
						 char *date,
						 char *headline,
						 char *originCity)
{
    WAISDocumentHeadlines *docHeadline = S_MALLOC(WAISDocumentHeadlines);

    docHeadline->DocumentID = docID;
    docHeadline->VersionNumber = versionNumber;
    docHeadline->Source = source;
    docHeadline->Date = date;
    docHeadline->Headline = headline;
    docHeadline->OriginCity = originCity;

    return (docHeadline);
}

/*----------------------------------------------------------------------*/

void freeWAISDocumentHeadlines(WAISDocumentHeadlines *docHeadline)
{
    freeAny(docHeadline->DocumentID);
    s_free(docHeadline->Source);
    s_free(docHeadline->Date);
    s_free(docHeadline->Headline);
    s_free(docHeadline->OriginCity);
    s_free(docHeadline);
}

/*----------------------------------------------------------------------*/

char *writeWAISDocumentHeadlines(WAISDocumentHeadlines *docHeadline, char *buffer,
				 long *len)
{
    unsigned long header_len = userInfoTagSize(DT_DocumentHeadlineGroup,
					       DefWAISDocHeadlineSize);
    char *buf = buffer + header_len;
    unsigned long size;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeAny(docHeadline->DocumentID, DT_DocumentID, buf, len);
    buf = writeNum(docHeadline->VersionNumber, DT_VersionNumber, buf, len);
    buf = writeString(docHeadline->Source, DT_Source, buf, len);
    buf = writeString(docHeadline->Date, DT_Date, buf, len);
    buf = writeString(docHeadline->Headline, DT_Headline, buf, len);
    buf = writeString(docHeadline->OriginCity, DT_OriginCity, buf, len);

    /* now write the header and size */
    size = buf - buffer;
    buf = writeUserInfoHeader(DT_DocumentHeadlineGroup,
			      size,
			      header_len,
			      buffer,
			      len);

    return (buf);
}

/*----------------------------------------------------------------------*/

char *readWAISDocumentHeadlines(WAISDocumentHeadlines **docHeadline, char *buffer)
{
    char *buf = buffer;
    unsigned long size;
    unsigned long headerSize;
    data_tag tag1;
    any *docID;
    long versionNumber;
    char *source, *date, *headline, *originCity;

    docID = NULL;
    versionNumber = UNUSED;
    source = date = headline = originCity = NULL;

    buf = readUserInfoHeader(&tag1, &size, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_DocumentID:
	    buf = readAny(&docID, buf);
	    break;
	case DT_VersionNumber:
	    buf = readNum(&versionNumber, buf);
	    break;
	case DT_Source:
	    buf = readString(&source, buf);
	    break;
	case DT_Date:
	    buf = readString(&date, buf);
	    break;
	case DT_Headline:
	    buf = readString(&headline, buf);
	    break;
	case DT_OriginCity:
	    buf = readString(&originCity, buf);
	    break;
	default:
	    freeAny(docID);
	    s_free(source);
	    s_free(date);
	    s_free(headline);
	    s_free(originCity);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *docHeadline = makeWAISDocumentHeadlines(docID, versionNumber, source, date,
					     headline, originCity);
    return (buf);
}

/*----------------------------------------------------------------------*/

WAISDocumentCodes *makeWAISDocumentCodes(any *docID,
					 long versionNumber,
					 char *stockCodes,
					 char *companyCodes,
					 char *industryCodes)
{
    WAISDocumentCodes *docCodes = S_MALLOC(WAISDocumentCodes);

    docCodes->DocumentID = docID;
    docCodes->VersionNumber = versionNumber;
    docCodes->StockCodes = stockCodes;
    docCodes->CompanyCodes = companyCodes;
    docCodes->IndustryCodes = industryCodes;

    return (docCodes);
}

/*----------------------------------------------------------------------*/

void freeWAISDocumentCodes(WAISDocumentCodes *docCodes)
{
    freeAny(docCodes->DocumentID);
    s_free(docCodes->StockCodes);
    s_free(docCodes->CompanyCodes);
    s_free(docCodes->IndustryCodes);
    s_free(docCodes);
}

/*----------------------------------------------------------------------*/

char *writeWAISDocumentCodes(WAISDocumentCodes *docCodes,
			     char *buffer,
			     long *len)
{
    unsigned long header_len = userInfoTagSize(DT_DocumentCodeGroup,
					       DefWAISDocCodeSize);
    char *buf = buffer + header_len;
    unsigned long size;

    RESERVE_SPACE_FOR_WAIS_HEADER(len);

    buf = writeAny(docCodes->DocumentID, DT_DocumentID, buf, len);
    buf = writeNum(docCodes->VersionNumber, DT_VersionNumber, buf, len);
    buf = writeString(docCodes->StockCodes, DT_StockCodes, buf, len);
    buf = writeString(docCodes->CompanyCodes, DT_CompanyCodes, buf, len);
    buf = writeString(docCodes->IndustryCodes, DT_IndustryCodes, buf, len);

    /* now write the header and size */
    size = buf - buffer;
    buf = writeUserInfoHeader(DT_DocumentCodeGroup, size, header_len, buffer, len);

    return (buf);
}

/*----------------------------------------------------------------------*/

char *readWAISDocumentCodes(WAISDocumentCodes **docCodes,
			    char *buffer)
{
    char *buf = buffer;
    unsigned long size;
    unsigned long headerSize;
    data_tag tag1;
    any *docID;
    long versionNumber;
    char *stockCodes, *companyCodes, *industryCodes;

    docID = NULL;
    versionNumber = UNUSED;
    stockCodes = companyCodes = industryCodes = NULL;

    buf = readUserInfoHeader(&tag1, &size, buf);
    headerSize = buf - buffer;

    while (buf < (buffer + size + headerSize)) {
	data_tag tag = peekTag(buf);

	switch (tag) {
	case DT_DocumentID:
	    buf = readAny(&docID, buf);
	    break;
	case DT_VersionNumber:
	    buf = readNum(&versionNumber, buf);
	    break;
	case DT_StockCodes:
	    buf = readString(&stockCodes, buf);
	    break;
	case DT_CompanyCodes:
	    buf = readString(&companyCodes, buf);
	    break;
	case DT_IndustryCodes:
	    buf = readString(&industryCodes, buf);
	    break;
	default:
	    freeAny(docID);
	    s_free(stockCodes);
	    s_free(companyCodes);
	    s_free(industryCodes);
	    REPORT_READ_ERROR(buf);
	    break;
	}
    }

    *docCodes = makeWAISDocumentCodes(docID, versionNumber, stockCodes,
				      companyCodes, industryCodes);
    return (buf);
}

/*----------------------------------------------------------------------*/

char *writePresentInfo(PresentAPDU * present GCC_UNUSED, char *buffer,
		       long *len GCC_UNUSED)
{
    /* The WAIS protocol doesn't use present info */
    return (buffer);
}

/*----------------------------------------------------------------------*/

char *readPresentInfo(void **info,
		      char *buffer)
{
    /* The WAIS protocol doesn't use present info */
    *info = NULL;
    return (buffer);
}

/*----------------------------------------------------------------------*/

char *writePresentResponseInfo(PresentResponseAPDU * response GCC_UNUSED, char *buffer,
			       long *len GCC_UNUSED)
{
    /* The WAIS protocol doesn't use presentResponse info */
    return (buffer);
}

/*----------------------------------------------------------------------*/

char *readPresentResponseInfo(void **info,
			      char *buffer)
{
    /* The WAIS protocol doesn't use presentResponse info */
    *info = NULL;
    return (buffer);
}

/*----------------------------------------------------------------------*/

/* support for type 1 queries */

/* new use values (for the chunk types) */
#define	BYTE		"wb"
#define	LINE		"wl"
#define	PARAGRAPH	"wp"
#define DATA_TYPE	"wt"

/* WAIS supports the following semantics for type 1 queries:

     1.  retrieve the header/codes from a document:

	    System_Control_Number = docID
	    Data Type = type (optional)
	    And

     2.  retrieve a fragment of the text of a document:

	    System_Control_Number = docID
	    Data Type = type (optional)
	    And
		Chunk >= start
		And
		Chunk < end
		And

		Information from multiple documents may be requested by using
		groups of the above joined by:

	    OR

		( XXX does an OR come after every group but the first, or do they
	      all come at the end? )

	( XXX return type could be in the element set)
*/

static query_term **makeWAISQueryTerms(DocObj **docs)
/* given a null terminated list of docObjs, construct the appropriate
   query of the form given above
 */
{
    query_term **terms = NULL;
    long numTerms = 0;
    DocObj *doc = NULL;
    long i;

    if (docs == NULL)
	return ((query_term **) NULL);

    terms = (query_term **) s_malloc((size_t) (sizeof(query_term *) * 1));

    terms[numTerms] = NULL;

    /* loop through the docs making terms for them all */
    for (i = 0, doc = docs[i]; doc != NULL; doc = docs[++i]) {
	any *type = NULL;

	if (doc->Type != NULL)
	    type = stringToAny(doc->Type);

	if (doc->ChunkCode == CT_document)	/* a whole document */
	{
	    terms = S_REALLOC2(query_term *, terms, numTerms + 2);

	    terms[numTerms++] = makeAttributeTerm(SYSTEM_CONTROL_NUMBER,
						  EQUAL, IGNORE, IGNORE,
						  IGNORE, IGNORE, doc->DocumentID);
	    if (type != NULL) {
		terms[numTerms++] = makeAttributeTerm(DATA_TYPE, EQUAL,
						      IGNORE, IGNORE, IGNORE,
						      IGNORE, type);
		terms[numTerms++] = makeOperatorTerm(AND);
	    }
	    terms[numTerms] = NULL;
	} else {		/* a document fragment */
	    char chunk_att[ATTRIBUTE_SIZE];
	    any *startChunk = NULL;
	    any *endChunk = NULL;

	    terms = S_REALLOC2(query_term *, terms, numTerms + 6);

	    switch (doc->ChunkCode) {
	    case CT_byte:
	    case CT_line:
		{
		    char start[20], end[20];

		    (doc->ChunkCode == CT_byte) ?
			StrNCpy(chunk_att, BYTE, ATTRIBUTE_SIZE) :
			StrNCpy(chunk_att, LINE, ATTRIBUTE_SIZE);
		    sprintf(start, "%ld", doc->ChunkStart.Pos);
		    startChunk = stringToAny(start);
		    sprintf(end, "%ld", doc->ChunkEnd.Pos);
		    endChunk = stringToAny(end);
		}
		break;
	    case CT_paragraph:
		StrNCpy(chunk_att, PARAGRAPH, ATTRIBUTE_SIZE);
		startChunk = doc->ChunkStart.ID;
		endChunk = doc->ChunkEnd.ID;
		break;
	    default:
		/* error */
		break;
	    }

	    terms[numTerms++] = makeAttributeTerm(SYSTEM_CONTROL_NUMBER,
						  EQUAL, IGNORE, IGNORE,
						  IGNORE,
						  IGNORE, doc->DocumentID);
	    if (type != NULL) {
		terms[numTerms++] = makeAttributeTerm(DATA_TYPE, EQUAL, IGNORE,
						      IGNORE, IGNORE, IGNORE,
						      type);
		terms[numTerms++] = makeOperatorTerm(AND);
	    }
	    terms[numTerms++] = makeAttributeTerm(chunk_att,
						  GREATER_THAN_OR_EQUAL,
						  IGNORE, IGNORE, IGNORE,
						  IGNORE,
						  startChunk);
	    terms[numTerms++] = makeOperatorTerm(AND);
	    terms[numTerms++] = makeAttributeTerm(chunk_att, LESS_THAN,
						  IGNORE, IGNORE, IGNORE,
						  IGNORE,
						  endChunk);
	    terms[numTerms++] = makeOperatorTerm(AND);
	    terms[numTerms] = NULL;

	    if (doc->ChunkCode == CT_byte || doc->ChunkCode == CT_line) {
		freeAny(startChunk);
		freeAny(endChunk);
	    }
	}

	freeAny(type);

	if (i != 0)		/* multiple independent queries, need a disjunction */
	{
	    terms = S_REALLOC2(query_term *, terms, numTerms);

	    terms[numTerms++] = makeOperatorTerm(OR);
	    terms[numTerms] = NULL;
	}
    }

    return (terms);
}

/*----------------------------------------------------------------------*/

static DocObj **makeWAISQueryDocs(query_term **terms)
/* given a list of terms in the form given above, convert them to
   DocObjs.
 */
{
    query_term *docTerm = NULL;
    query_term *fragmentTerm = NULL;
    DocObj **docs = NULL;
    DocObj *doc = NULL;
    long docNum, termNum;

    docNum = termNum = 0;

    docs = S_MALLOC(DocObj *);

    docs[docNum] = NULL;

    /* translate the terms into DocObjs */
    while (true) {
	query_term *typeTerm = NULL;
	char *type = NULL;
	long startTermOffset;

	docTerm = terms[termNum];

	if (docTerm == NULL)
	    break;		/* we're done converting */

	typeTerm = terms[termNum + 1];	/* get the lead Term if it exists */

	if (strcmp(typeTerm->Use, DATA_TYPE) == 0)	/* we do have a type */
	{
	    startTermOffset = 3;
	    type = anyToString(typeTerm->Term);
	} else {		/* no type */
	    startTermOffset = 1;
	    typeTerm = NULL;
	    type = NULL;
	}

	/* grow the doc list */
	docs = S_REALLOC2(DocObj *, docs, docNum);

	/* figure out what kind of docObj to build - and build it */
	fragmentTerm = terms[termNum + startTermOffset];
	if (fragmentTerm != NULL && fragmentTerm->TermType == TT_Attribute) {	/* build a document fragment */
	    query_term *startTerm = fragmentTerm;
	    query_term *endTerm = terms[termNum + startTermOffset + 2];

	    if (strcmp(startTerm->Use, BYTE) == 0) {	/* a byte chunk */
		doc = makeDocObjUsingBytes(duplicateAny(docTerm->Term),
					   type,
					   anyToLong(startTerm->Term),
					   anyToLong(endTerm->Term));
		log_write("byte");
	    } else if (strcmp(startTerm->Use, LINE) == 0) {	/* a line chunk */
		doc = makeDocObjUsingLines(duplicateAny(docTerm->Term),
					   type,
					   anyToLong(startTerm->Term),
					   anyToLong(endTerm->Term));
		log_write("line");
	    } else {
		log_write("chunk");	/* a paragraph chunk */
		doc = makeDocObjUsingParagraphs(duplicateAny(docTerm->Term),
						type,
						duplicateAny(startTerm->Term),
						duplicateAny(endTerm->Term));
	    }
	    termNum += (startTermOffset + 4);	/* point to next term */
	} else {		/* build a full document */
	    doc = makeDocObjUsingWholeDocument(duplicateAny(docTerm->Term),
					       type);
	    log_write("whole doc");
	    termNum += startTermOffset;		/* point to next term */
	}

	docs[docNum++] = doc;	/* insert the new document */

	docs[docNum] = NULL;	/* keep the doc list terminated */

	if (terms[termNum] != NULL)
	    termNum++;		/* skip the OR operator it necessary */
	else
	    break;		/* we are done */
    }

    return (docs);
}

/*----------------------------------------------------------------------*/

any *makeWAISTextQuery(DocObj **docs)
/* given a list of DocObjs, return an any whose contents is the corresponding
   type 1 query
 */
{
    any *buf = NULL;
    query_term **terms = NULL;

    terms = makeWAISQueryTerms(docs);
    buf = writeQuery(terms);

    doList((void **) terms, freeTerm);
    s_free(terms);

    return (buf);
}

/*----------------------------------------------------------------------*/

DocObj **readWAISTextQuery(any *buf)
/* given an any whose contents are type 1 queries of the WAIS sort,
   construct a list of the corresponding DocObjs
 */
{
    query_term **terms = NULL;
    DocObj **docs = NULL;

    terms = readQuery(buf);
    docs = makeWAISQueryDocs(terms);

    doList((void **) terms, freeTerm);
    s_free(terms);

    return (docs);
}

/*----------------------------------------------------------------------*/
/* Customized free WAIS object routines:                                */
/*                                                                      */
/*   This set of procedures is for applications to free a WAIS object   */
/*   which was made with makeWAISFOO.                                   */
/*   Each procedure frees only the memory that was allocated in its     */
/*   associated makeWAISFOO routine, thus it's not necessary for the    */
/*   caller to assign nulls to the pointer fields of the WAIS object.  */
/*----------------------------------------------------------------------*/

void CSTFreeWAISInitResponse(WAISInitResponse *init)
/* free an object made with makeWAISInitResponse */
{
    s_free(init);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISSearch(WAISSearch *query)
/* destroy an object made with makeWAISSearch() */
{
    s_free(query);
}

/*----------------------------------------------------------------------*/

void CSTFreeDocObj(DocObj *doc)
/* free a docObj */
{
    s_free(doc);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISDocumentHeader(WAISDocumentHeader *header)
{
    s_free(header);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISDocumentShortHeader(WAISDocumentShortHeader *header)
{
    s_free(header);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISDocumentLongHeader(WAISDocumentLongHeader *header)
{
    s_free(header);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISSearchResponse(WAISSearchResponse * response)
{
    s_free(response);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISDocumentText(WAISDocumentText *docText)
{
    s_free(docText);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISDocHeadlines(WAISDocumentHeadlines *docHeadline)
{
    s_free(docHeadline);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISDocumentCodes(WAISDocumentCodes *docCodes)
{
    s_free(docCodes);
}

/*----------------------------------------------------------------------*/

void CSTFreeWAISTextQuery(any *query)
{
    freeAny(query);
}

/*----------------------------------------------------------------------*/

/*
 *	Routines originally from WMessage.c -- FM
 *
 *----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE
 * No guarantees or restrictions.  See the readme file for the full standard
 * disclaimer.
 * 3.26.90
 */

/* This file is for reading and writing the wais packet header.
 * Morris@think.com
 */

/* to do:
 *  add check sum
 *  what do you do when checksum is wrong?
 */

/*---------------------------------------------------------------------*/

void readWAISPacketHeader(char *msgBuffer,
			  WAISMessage * header_struct)
{
    /* msgBuffer is a string containing at least HEADER_LENGTH bytes. */

    memmove(header_struct->msg_len, msgBuffer, (size_t) 10);
    header_struct->msg_type = char_downcase((unsigned long) msgBuffer[10]);
    header_struct->hdr_vers = char_downcase((unsigned long) msgBuffer[11]);
    memmove(header_struct->server, (void *) (msgBuffer + 12), (size_t) 10);
    header_struct->compression = char_downcase((unsigned long) msgBuffer[22]);
    header_struct->encoding = char_downcase((unsigned long) msgBuffer[23]);
    header_struct->msg_checksum = char_downcase((unsigned long) msgBuffer[24]);
}

/*---------------------------------------------------------------------*/

/* this modifies the header argument.  See wais-message.h for the different
 * options for the arguments.
 */

void writeWAISPacketHeader(char *header,
			   long dataLen,
			   long type,
			   char *server,
			   long compression,
			   long encoding,
			   long version)
/* Puts together the new wais before-the-z39-packet header. */
{
    char lengthBuf[11];
    char serverBuf[11];

    long serverLen = strlen(server);

    if (serverLen > 10)
	serverLen = 10;

    sprintf(lengthBuf, "%010ld", dataLen);
    StrNCpy(header, lengthBuf, 10);

    header[10] = type & 0xFF;
    header[11] = version & 0xFF;

    StrNCpy(serverBuf, server, serverLen);
    StrNCpy((char *) (header + 12), serverBuf, serverLen);

    header[22] = compression & 0xFF;
    header[23] = encoding & 0xFF;
    header[24] = '0';		/* checkSum(header + HEADER_LENGTH,dataLen);   XXX the result must be ascii */
}

/*---------------------------------------------------------------------*/
