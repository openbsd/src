/*							HTVMS_WAISProt.h
**
**	Adaptation for Lynx by F.Macrides (macrides@sci.wfeb.edu)
**
**	31-May-1994 FM	Initial version.
**
**----------------------------------------------------------------------*/

/*
**	Routines originally from WProt.h -- FM
**
**----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE:
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   3.26.90	Harry Morris, morris@think.com
   3.30.90  Harry Morris
			-	removed chunk code from WAISSearchAPDU,
			-	added makeWAISQueryType1Query() and readWAISType1Query() which
				replace makeWAISQueryTerms() and makeWAISQueryDocs().
   4.11.90  HWM - added definitions of wais element set names
   4.14.90  HWM - changed symbol for relevance feedback query from QT_3 to
				  QT_RelevanceFeedbackQuery added QT_TextRetrievalQuery as a
				  synonym for QT_BooleanQuery
				- renamed makeWAISType1Query() to makeWAISTextQuery()
				  renamed readWAISType1Query() to readWAISTextQuery()
   5.29.90  TS - added CSTFreeWAISFoo functions
*/

#ifndef _H_WAIS_protocol_
#define _H_WAIS_protocol_

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <HTVMS_WaisUI.h>

/*----------------------------------------------------------------------*/
/* Data types / constants */

/* date factor constants */
#define	DF_INDEPENDENT		1
#define DF_LATER		2
#define DF_EARLIER		3
#define DF_SPECIFIED_RANGE	4

/* chunk types */
#define CT_document		0
#define CT_byte			1
#define CT_line			2
#define CT_paragraph	3

/* relevance feedback query */
#define QT_RelevanceFeedbackQuery	"3"
#define QT_TextRetrievalQuery		QT_BooleanQuery

/* new data tags */
#define DT_UserInformationLength	(data_tag)99
#define	DT_ChunkCode			(data_tag)100
#define	DT_ChunkIDLength		(data_tag)101
#define	DT_ChunkMarker			(data_tag)102
#define	DT_HighlightMarker		(data_tag)103
#define	DT_DeHighlightMarker		(data_tag)104
#define	DT_NewlineCharacters		(data_tag)105
#define	DT_SeedWords			(data_tag)106
#define	DT_DocumentIDChunk		(data_tag)107
#define	DT_ChunkStartID			(data_tag)108
#define	DT_ChunkEndID			(data_tag)109
#define	DT_TextList			(data_tag)110
#define	DT_DateFactor			(data_tag)111
#define	DT_BeginDateRange		(data_tag)112
#define	DT_EndDateRange			(data_tag)113
#define	DT_MaxDocumentsRetrieved	(data_tag)114
#define	DT_SeedWordsUsed		(data_tag)115
#define	DT_DocumentID			(data_tag)116
#define	DT_VersionNumber		(data_tag)117
#define	DT_Score			(data_tag)118
#define	DT_BestMatch			(data_tag)119
#define	DT_DocumentLength		(data_tag)120
#define	DT_Source			(data_tag)121
#define	DT_Date				(data_tag)122
#define	DT_Headline			(data_tag)123
#define	DT_OriginCity			(data_tag)124
#define	DT_PresentStartByte		(data_tag)125
#define	DT_TextLength			(data_tag)126
#define	DT_DocumentText			(data_tag)127
#define	DT_StockCodes			(data_tag)128
#define	DT_CompanyCodes			(data_tag)129
#define	DT_IndustryCodes		(data_tag)130

/* added by harry */
#define DT_DocumentHeaderGroup		(data_tag)150
#define DT_DocumentShortHeaderGroup	(data_tag)151
#define DT_DocumentLongHeaderGroup	(data_tag)152
#define DT_DocumentTextGroup		(data_tag)153
#define DT_DocumentHeadlineGroup	(data_tag)154
#define DT_DocumentCodeGroup		(data_tag)155
#define DT_Lines			(data_tag)131
#define	DT_TYPE_BLOCK			(data_tag)132
#define DT_TYPE				(data_tag)133

/* wais element sets */
#define ES_DocumentHeader		"Document Header"
#define ES_DocumentShortHeader		"Document Short Header"
#define ES_DocumentLongHeader		"Document Long Header"
#define ES_DocumentText			"Document Text"
#define ES_DocumentHeadline		"Document Headline"
#define ES_DocumentCodes		"Document Codes"

typedef struct DocObj { /* specifies a section of a document */
	any*	DocumentID;
	char*   Type;
	long	ChunkCode;
	union {
		long	Pos;
		any*	ID;
	} ChunkStart;
	union {
		long	Pos;
		any*	ID;
	} ChunkEnd;
	} DocObj;

/*----------------------------------------------------------------------*/
/* WAIS APDU extensions */

typedef struct WAISInitResponse {
	long				ChunkCode;
	long				ChunkIDLength;
	char*				ChunkMarker;
	char*				HighlightMarker;
	char*				DeHighlightMarker;
	char*				NewlineCharacters;
	/* XXX  need to add UpdateFrequency and Update Time */
	} WAISInitResponse;

typedef struct WAISSearch {
	char*				SeedWords;
	DocObj**			Docs;
	char**				TextList;
	long				DateFactor;
	char*				BeginDateRange;
	char*				EndDateRange;
	long				MaxDocumentsRetrieved;
	} WAISSearch;

typedef struct WAISDocumentHeader {
	any*				DocumentID;
	long				VersionNumber;
	long				Score;
	long				BestMatch;
	long				DocumentLength;
	long				Lines;
	char**				Types;
	char*				Source;
	char*				Date;
	char*				Headline;
	char*				OriginCity;
	} WAISDocumentHeader;

typedef struct WAISDocumentShortHeader {
	any*				DocumentID;
	long				VersionNumber;
	long				Score;
	long				BestMatch;
	long				DocumentLength;
	long				Lines;
	} WAISDocumentShortHeader;

typedef struct WAISDocumentLongHeader {
	any*				DocumentID;
	long				VersionNumber;
	long				Score;
	long				BestMatch;
	long				DocumentLength;
	long				Lines;
	char**				Types;
	char*				Source;
	char*				Date;
	char*				Headline;
	char*				OriginCity;
	char*				StockCodes;
	char*				CompanyCodes;
	char*				IndustryCodes;
	} WAISDocumentLongHeader;

typedef struct WAISDocumentText {
	any*				DocumentID;
	long				VersionNumber;
	any*				DocumentText;
	} WAISDocumentText;

typedef struct WAISDocumentHeadlines {
	any*				DocumentID;
	long				VersionNumber;
	char*				Source;
	char*				Date;
	char*				Headline;
	char*				OriginCity;
	} WAISDocumentHeadlines;

typedef struct WAISDocumentCodes {
	any*				DocumentID;
	long				VersionNumber;
	char*				StockCodes;
	char*				CompanyCodes;
	char*				IndustryCodes;
	} WAISDocumentCodes;

typedef struct WAISSearchResponse {
	char*					SeedWordsUsed;
	WAISDocumentHeader**		DocHeaders;
	WAISDocumentShortHeader**	ShortHeaders;
	WAISDocumentLongHeader**	LongHeaders;
	WAISDocumentText**			Text;
	WAISDocumentHeadlines**		Headlines;
	WAISDocumentCodes**			Codes;
	diagnosticRecord**			Diagnostics;
	} WAISSearchResponse;

/*----------------------------------------------------------------------*/
/* Functions */

char *
generate_search_apdu PARAMS((char* buff,  /* buffer to hold the apdu */
			  long *buff_len, /* number of bytes written to the buffer */
			  char *seed_words, /* string of the seed words */
			  char *database_name,
			  DocObj** docobjs,
			  long maxDocsRetrieved
			  ));

DocObj* makeDocObjUsingWholeDocument PARAMS((any* aDocID,char* type));
DocObj* makeDocObjUsingBytes PARAMS((any* aDocID,char* type,long start,long end));
DocObj* makeDocObjUsingLines PARAMS((any* aDocID,char* type,long start,long end));
DocObj* makeDocObjUsingParagraphs PARAMS((any* aDocID,char* type,any* start,any* end));
void freeDocObj PARAMS((DocObj* doc));

WAISInitResponse* makeWAISInitResponse PARAMS((long chunkCode,long chunkIDLen,
					    char* chunkMarker,char* highlightMarker,
					    char* deHighlightMarker,char* newLineChars));
void freeWAISInitResponse PARAMS((WAISInitResponse* init));

WAISSearch* makeWAISSearch PARAMS((
	char* seedWords,DocObj** docs,char** textList,
	long dateFactor,char* beginDateRange,char* endDateRange,
	long maxDocsRetrieved));
void freeWAISSearch PARAMS((WAISSearch* query));

WAISDocumentHeader* makeWAISDocumentHeader PARAMS((
	any* aDocID,long versionNumber,long score,long bestMatch,long docLen,
	long lines,char** types,char* source,char* date,char* headline,char* originCity));
void freeWAISDocumentHeader PARAMS((WAISDocumentHeader* header));
char* writeWAISDocumentHeader PARAMS((WAISDocumentHeader* header,char* buffer,long* len));
char* readWAISDocumentHeader PARAMS((WAISDocumentHeader** header,char* buffer));

WAISDocumentShortHeader* makeWAISDocumentShortHeader PARAMS((
	any* aDocID,long versionNumber,long score,long bestMatch,long docLen,long lines));
void freeWAISDocumentShortHeader PARAMS((WAISDocumentShortHeader* header));
char* writeWAISDocumentShortHeader PARAMS((WAISDocumentShortHeader* header,
                                   char* buffer,long* len));
char* readWAISDocumentShortHeader PARAMS((WAISDocumentShortHeader** header,char* buffer));

WAISDocumentLongHeader* makeWAISDocumentLongHeader PARAMS((
	any* aDocID,long versionNumber,long score,long bestMatch,long docLen,
	long lines,char** types,char* source,char* date, char* headline,char* originCity,
	char* stockCodes,char* companyCodes,char* industryCodes));
void freeWAISDocumentLongHeader PARAMS((WAISDocumentLongHeader* header));
char* writeWAISDocumentLongHeader PARAMS((WAISDocumentLongHeader* header,char* buffer,long* len));
char* readWAISDocumentLongHeader PARAMS((WAISDocumentLongHeader** header,char* buffer));

WAISSearchResponse* makeWAISSearchResponse PARAMS((
	char* seedWordsUsed,WAISDocumentHeader** docHeaders,
	WAISDocumentShortHeader** shortHeaders,
	WAISDocumentLongHeader** longHeaders,
	WAISDocumentText** text,WAISDocumentHeadlines** headlines,
	WAISDocumentCodes** codes,
	diagnosticRecord** diagnostics));
void freeWAISSearchResponse PARAMS((WAISSearchResponse* response));

WAISDocumentText* makeWAISDocumentText PARAMS((any* aDocID,long versionNumber,
				       any* documentText));
void freeWAISDocumentText PARAMS((WAISDocumentText* docText));
char* writeWAISDocumentText PARAMS((WAISDocumentText* docText,char* buffer,long* len));
char* readWAISDocumentText PARAMS((WAISDocumentText** docText,char* buffer));

WAISDocumentHeadlines* makeWAISDocumentHeadlines PARAMS((
	any* aDocID,long versionNumber,char* source,char* date,char* headline,
	char* originCity));
void freeWAISDocumentHeadlines PARAMS((WAISDocumentHeadlines* docHeadline));
char* writeWAISDocumentHeadlines PARAMS((WAISDocumentHeadlines* docHeadline,char* buffer,long* len));
char* readWAISDocumentHeadlines PARAMS((WAISDocumentHeadlines** docHeadline,char* buffer));

WAISDocumentCodes* makeWAISDocumentCodes PARAMS((
	any* aDocID,long versionNumber,char* stockCodes,char* companyCodes,
	char* industryCodes));
void freeWAISDocumentCodes PARAMS((WAISDocumentCodes* docCodes));
char* writeWAISDocumentCodes PARAMS((WAISDocumentCodes* docCodes,char* buffer,long* len));
char* readWAISDocumentCodes PARAMS((WAISDocumentCodes** docCodes,char* buffer));

any* makeWAISTextQuery PARAMS((DocObj** docs));
DocObj** readWAISTextQuery PARAMS((any* terms));

void CSTFreeWAISInitResponse PARAMS((WAISInitResponse* init));
void CSTFreeWAISSearch PARAMS((WAISSearch* query));
void CSTFreeDocObj PARAMS((DocObj* doc));
void CSTFreeWAISDocumentHeader PARAMS((WAISDocumentHeader* header));
void CSTFreeWAISDocumentShortHeader PARAMS((WAISDocumentShortHeader* header));
void CSTFreeWAISDocumentLongHeader PARAMS((WAISDocumentLongHeader* header));
void CSTFreeWAISSearchResponse PARAMS((WAISSearchResponse* response));
void CSTFreeWAISDocumentText PARAMS((WAISDocumentText* docText));
void CSTFreeWAISDocHeadlines PARAMS((WAISDocumentHeadlines* docHeadline));
void CSTFreeWAISDocumentCodes PARAMS((WAISDocumentCodes* docCodes));
void CSTFreeWAISTextQuery PARAMS(( any* query));

/*----------------------------------------------------------------------*/

#endif /* ndef _H_WAIS_protocol_ */


/*
**	Routines originally from WMessage.h -- FM
**
**----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.
   3.26.90
*/

/* wais-message.h
 *
 * This is the header outside of WAIS Z39.50 messages.  The header will be
 * printable ascii, so as to be transportable.  This header will precede each
 * Z39.50 APDU, or zero-length message if it is an ACK or NACK.  Be sure to
 * change hdr_vers current value if you change the structure of the header.
 *
 * The characters in the header are case insensitive so that the systems from
 * the past that only handle one case can at least read the header.
 *
 * 7.5.90 HWM - added constants
 * 7/5/90 brewster added funtion prototypes and comments
 * 11/30/90 HWM - went to version 2 (inits and typed retrieval)
 */

#ifndef WMESSAGE_H
#define WMESSAGE_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <HTVMS_WaisUI.h>

typedef struct wais_header {
        char    msg_len[10];    /* length in bytes of following message */
        char    msg_type;       /* type of message: 'z'=Z39.50 APDU,
                                   'a'=ACK, 'n'=NACK */
        char    hdr_vers;       /* version of this header, currently = '2' */
        char    server[10];     /* name or address of server */
        char    compression;    /* <sp>=no compression, 'u'=unix compress */
        char    encoding;       /* <sp>=no encoding, 'h'=hexize,
				   'u'=uuencode */
        char    msg_checksum;   /* XOR of every byte of message */
        } WAISMessage;

#define HEADER_LENGTH	25	/* number of bytes needed to write a
				   wais-header (not sizeof(wais_header)) */

#define HEADER_VERSION	(long)'2'

/* message type */
#define Z3950			'z'
#define ACK			'a'
#define	NAK			'n'

/* compression */
#define NO_COMPRESSION		' '
#define UNIX_COMPRESSION	'u'

/* encoding */
#define NO_ENCODING		' '
#define HEX_ENCODING		'h'  /* Swartz 4/3 encoding */
#define IBM_HEXCODING		'i'  /* same as h but uses characters acceptable for IBM mainframes */
#define UUENCODE		'u'


void readWAISPacketHeader PARAMS((char* msgBuffer,WAISMessage *header_struct));
long getWAISPacketLength PARAMS((WAISMessage* header));
void writeWAISPacketHeader PARAMS((char* header,long dataLen,long type,
				char* server,long compression,
				long encoding,long version));

#endif /* ndef WMESSAGE_H */
