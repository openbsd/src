/*							HTVMS_WAISUI.h
**
**	Adaptation for Lynx by F.Macrides (macrides@sci.wfeb.edu)
**
**	31-May-1994 FM	Initial version.
*/

#ifndef HTVMSWAIS_H
#define HTVMSWAIS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

void	log_write PARAMS((char *));

/*
**	Routines originally from Panic.h -- FM
**
**----------------------------------------------------------------------*/

void	panic (char* format,...);

/*----------------------------------------------------------------------*/


/*
**	Routines originally from CUtil.h -- FM
**
**----------------------------------------------------------------------*/

/* types and constants */

#ifndef boolean
#define boolean unsigned long
#endif /* boolean */

#ifndef true
#define true	(boolean)1L
#endif /* true */

#ifndef false
#define false	(boolean)0L   /* used to be (!true), but broke
				 some compilers */
#endif /* false */

#ifndef TRUE
#define TRUE	true
#endif /* TRUE */

#ifndef FALSE
#define FALSE	false
#endif /* FALSE */

/*----------------------------------------------------------------------*/
/* functions */

/* enhanced memory handling functions - don't call them directly, use the
   macros below */
void	fs_checkPtr PARAMS((void* ptr));
void*	fs_malloc PARAMS((size_t size));
void*	fs_realloc PARAMS((void* ptr,size_t size));
void	fs_free PARAMS((void* ptr));
char*	fs_strncat PARAMS((char* dst,char* src,size_t maxToAdd,size_t maxTotal));

/* macros for memory functions.  call these in your program.  */
#define s_checkPtr(ptr)		fs_checkPtr(ptr)
#define s_malloc(size)		fs_malloc(size)
#define s_realloc(ptr,size)	fs_realloc((ptr),(size))
#define s_free(ptr)		{ fs_free((char*)ptr); ptr = NULL; }
#define s_strncat(dst,src,maxToAdd,maxTotal)	fs_strncat((dst),(src),(maxToAdd),(maxTotal))

char*	s_strdup PARAMS((char* s));

#define IS_DELIMITER	1
#define	NOT_DELIMITER	!IS_DELIMITER

char char_downcase PARAMS((unsigned long ch));
char *string_downcase PARAMS((char* word));

/*----------------------------------------------------------------------*/



/*
**	Routines originally from ZUtil.c -- FM
**
**----------------------------------------------------------------------*/

/* Data types / constants */

/* bytes to leave for the header size info */
#define HEADER_LEN	(size_t)2

typedef long pdu_type;

#define	initAPDU			(pdu_type)20
#define	initResponseAPDU		(pdu_type)21
#define	searchAPDU			(pdu_type)22
#define	searchResponseAPDU		(pdu_type)23
#define	presentAPDU			(pdu_type)24
#define	presentResponseAPDU		(pdu_type)25
#define	deteteAPDU			(pdu_type)26
#define	deleteResponseAPDU		(pdu_type)27
#define	accessControlAPDU		(pdu_type)28
#define	accessControlResponseAPDU	(pdu_type)29
#define	resourceControlAPDU		(pdu_type)30
#define	resourceControlResponseAPDU	(pdu_type)31

typedef struct any {	/* an any is a non-ascii string of characters */
	unsigned long	size;
	char*			bytes;
	} any;

typedef any	bit_map;	/* a bit_map is a group of packed bits */

typedef unsigned long data_tag;

#define DT_PDUType			(data_tag)1
#define	DT_ReferenceID			(data_tag)2
#define	DT_ProtocolVersion		(data_tag)3
#define	DT_Options			(data_tag)4
#define	DT_PreferredMessageSize		(data_tag)5
#define	DT_MaximumRecordSize		(data_tag)6
#define	DT_IDAuthentication		(data_tag)7
#define	DT_ImplementationID		(data_tag)8
#define	DT_ImplementationName		(data_tag)9
#define	DT_ImplementationVersion	(data_tag)10
#define	DT_UserInformationField		(data_tag)11
#define	DT_Result			(data_tag)12
#define	DT_SmallSetUpperBound		(data_tag)13
#define	DT_LargeSetLowerBound		(data_tag)14
#define	DT_MediumSetPresentNumber	(data_tag)15
#define	DT_ReplaceIndicator		(data_tag)16
#define	DT_ResultSetName		(data_tag)17
#define	DT_DatabaseNames		(data_tag)18
#define DT_ElementSetNames		(data_tag)19
#define	DT_QueryType			(data_tag)20
#define	DT_Query			(data_tag)21
#define	DT_SearchStatus			(data_tag)22
#define	DT_ResultCount			(data_tag)23
#define	DT_NumberOfRecordsReturned	(data_tag)24
#define	DT_NextResultSetPosition	(data_tag)25
#define	DT_ResultSetStatus		(data_tag)26
#define	DT_PresentStatus		(data_tag)27
#define	DT_DatabaseDiagnosticRecords	(data_tag)28
#define	DT_NumberOfRecordsRequested	(data_tag)29
#define	DT_ResultSetStartPosition	(data_tag)30
#define	DT_ResultSetID			(data_tag)31
#define	DT_DeleteOperation		(data_tag)32
#define	DT_DeleteStatus			(data_tag)33
#define	DT_NumberNotDeleted		(data_tag)34
#define	DT_BulkStatuses			(data_tag)35
#define	DT_DeleteMSG			(data_tag)36
#define	DT_SecurityChallenge		(data_tag)37
#define	DT_SecurityChallengeResponse	(data_tag)38
#define	DT_SuspendedFlag		(data_tag)39
#define	DT_ResourceReport		(data_tag)40
#define	DT_PartialResultsAvailable	(data_tag)41
#define	DT_ContinueFlag			(data_tag)42
#define	DT_ResultSetWanted		(data_tag)43

#define UNUSED	-1

/* number of bytes required to represent the following sizes in compressed
   integer format
 */
#define CompressedInt1Byte	128		/* 2 ^ 7 */
#define CompressedInt2Byte	16384		/* 2 ^ 14 */
#define CompressedInt3Byte	2097152		/* 2 ^ 21 */
/* others may follow ... */

/* types of query */
#define QT_0	"0"	/* query whose non-standard format has been agreed upon
			   client and server */
/* values for InitAPDU option element */
#define	WILL_USE		TRUE
#define WILL_NOT_USE		FALSE
#define WILL_SUPPORT		TRUE
#define WILL_NOT_SUPPORT	FALSE

/* values for InitResponseAPDU result element */
#define ACCEPT	TRUE
#define REJECT	FALSE

/* values for SearchResponseAPDU search status element */
#define	SUCCESS	0 /* intuitive huh? */
#define FAILURE	1

/* values for SearchResponseAPDU result set status element */
#define	SUBSET	1
#define INTERIM	2
#define NONE	3

/* values for SearchResponseAPDU present status element */
/* SUCCESS already defined */
#define PARTIAL_1	1
#define PARTIAL_2	2
#define PARTIAL_3	3
#define PARTIAL_4	4
#define PS_NONE		5 /* can't use NONE since it was used by result
			     set status */

#define DIAGNOSTIC_CODE_SIZE	(size_t)3

typedef struct diagnosticRecord
 { boolean	SURROGATE;
   char		DIAG[DIAGNOSTIC_CODE_SIZE];
   char*	ADDINFO;
 } diagnosticRecord;

#define D_PermanentSystemError	       "S1"
#define D_TemporarySystemError         "S2"
#define D_UnsupportedSearch            "S3"
#define D_TermsOnlyStopWords           "S5"
#define D_TooManyArgumentWords         "S6"
#define D_TooManyBooleanOperators      "S7"
#define D_TooManyTruncatedWords        "S8"
#define D_TooMany IncompleteSubfields  "S9"
#define D_TruncatedWordsTooShort       "SA"
#define D_InvalidFormatForRecordNumber "SB"
#define D_TooManyCharactersInSearch    "SC"
#define D_TooManyRecordsRetrieved      "SD"
#define D_PresentRequestOutOfRange     "SF"
#define D_SystemErrorInPresentRecords  "SG"
#define D_RecordNotAuthorizedToBeSent  "SH"
#define D_RecordExceedsPrefMessageSize "SI"
#define D_RecordExceedsMaxRecordSize   "SJ"
#define D_ResultSetNotSuppAsSearchTerm "SK"
#define D_OnlyOneRsltSetAsSrchTermSupp "SL"
#define D_OnlyANDingOfASnglRsltSetSupp "SM"
#define D_RsltSetExistsNoReplace       "SN"
#define D_ResultSetNamingNotSupported  "SO"
#define D_CombinationDatabasesNotSupp  "SP"
#define D_ElementSetNamesNotSupported  "SQ"
#define D_ElementSetNameNotValid       "SR"
#define D_OnlyASingleElmntSetNameSupp  "SS"
#define D_ResultSetDeletedByTarget     "ST"
#define D_ResultSetIsInUse             "SU"
#define D_DatabasesIsLocked            "SV"
#define D_TerminatedByNoContinueResp   "SW"
#define D_ResultSetDoesNotExist        "SX"
#define D_ResExNoResultsAvailable      "SY"
#define D_ResExUnpredictableResults    "SZ"
#define D_ResExValidSubsetOfResults    "T1"
#define D_AccessControlFailure         "T2"
#define D_SecurityNotIssuedReqTerm     "T3"
#define D_SecurityNotBeIssuedRecNotInc "T4"

/*----------------------------------------------------------------------*/

/* for internal error handling */

extern char* readErrorPosition;		/* pos where buf stoped making sense */

/* the following are macros so that they can return OUT of the function
   which calls them
 */

#define RETURN_ON_NULL(var)					\
	if (var == NULL)					\
	  return(NULL); /* jump out of caller */

#define REPORT_READ_ERROR(pos)					\
	{ readErrorPosition = (pos);				\
	  return(NULL); /* jump out of caller */		\
    }

#define CHECK_FOR_SPACE_LEFT(spaceNeeded,spaceLeft)		\
	{ if (*spaceLeft >= spaceNeeded)			\
	    (*spaceLeft) -= spaceNeeded;			\
	  else							\
	   { *spaceLeft = 0;					\
	     return(NULL); /* jump out of the caller */		\
	   }							\
	}

/*----------------------------------------------------------------------*/

diagnosticRecord* makeDiag PARAMS((boolean surrogate,char* code,char* addInfo));
void freeDiag PARAMS((diagnosticRecord* diag));
char* writeDiag PARAMS((diagnosticRecord* diag,char* buffer,long* len));
char* readDiag PARAMS((diagnosticRecord** diag,char* buffer));

char* writeCompressedInteger PARAMS((unsigned long num,char* buf,long* len));
char* readCompressedInteger PARAMS((unsigned long *num,char* buf));
char* writeCompressedIntWithPadding PARAMS((unsigned long num,unsigned long size,
					 char* buffer,long* len));
unsigned long writtenCompressedIntSize PARAMS((unsigned long num));

char* writeTag PARAMS((data_tag tag,char* buf,long* len));
char* readTag PARAMS((data_tag* tag,char* buf));
data_tag peekTag PARAMS((char* buf));
unsigned long writtenTagSize PARAMS((data_tag tag));

any* makeAny PARAMS((unsigned long size,char* data));
void freeAny PARAMS((any* a));
any* duplicateAny PARAMS((any* a));
char* writeAny PARAMS((any* a,data_tag tag,char* buffer,long* len));
char* readAny PARAMS((any** anAny,char* buffer));
unsigned long writtenAnySize PARAMS((data_tag tag,any* a));

any* stringToAny PARAMS((char* s));
char* anyToString PARAMS((any* a));
unsigned long writtenStringSize PARAMS((data_tag tag,char* s));

any* longToAny PARAMS((long Num));
long anyToLong PARAMS((any* a));

char* writeString PARAMS((char* s,data_tag tag,char* buffer,long* len));
char* readString PARAMS((char** s,char* buffer));

bit_map* makeBitMap (unsigned long numBits,...);

void freeBitMap PARAMS((bit_map* bm));
boolean bitAtPos PARAMS((unsigned long pos,bit_map* bm));
char* writeBitMap PARAMS((bit_map* bm,data_tag tag,char* buffer,long* len));
char* readBitMap PARAMS((bit_map** bm,char* buffer));

char* writeByte PARAMS((unsigned long byte,char* buf,long* len));
char* readByte PARAMS((unsigned char* byte,char* buf));

char* writeBoolean PARAMS((boolean flag,char* buf,long* len));
char* readBoolean PARAMS((boolean* flag,char* buf));

char* writePDUType PARAMS((pdu_type pduType,char* buf,long* len));
char* readPDUType PARAMS((pdu_type* pduType,char* buf));
pdu_type peekPDUType PARAMS((char* buf));

char* writeBinaryInteger PARAMS((long num,unsigned long size,
			      char* buf,long* len));
char* readBinaryInteger PARAMS((long* num,unsigned long size,char* buf));
unsigned long writtenCompressedBinIntSize PARAMS((long num));

char* writeNum PARAMS((long num,data_tag tag,char* buffer,long* len));
char* readNum PARAMS((long* num,char* buffer));
unsigned long  writtenNumSize PARAMS((data_tag tag,long num));

void doList PARAMS((void** list,void (*func)(void *)));

char* writeProtocolVersion PARAMS((char* buf,long* len));
char* defaultImplementationID PARAMS((void));
char* defaultImplementationName PARAMS((void));
char* defaultImplementationVersion PARAMS((void));

/*----------------------------------------------------------------------*/


/*
**	Routines originally from ZType1.c -- FM
**
**----------------------------------------------------------------------*/

/* This file implements the type 1 query defined in appendices B & C
   of the SR 1 spec.
 */

/*----------------------------------------------------------------------*/
/* types and constants */

/* new data tags */
#define	DT_AttributeList	(data_tag)44
#define DT_Term			(data_tag)45
#define DT_Operator		(data_tag)46

#define QT_BooleanQuery	"1"		/* standard boolean query */

/* general attribute code - use in place of any attribute */
#define IGNORE	"ig"

/* use value codes */
#define	UV_ISBN	"ub"
#define	CORPORATE_NAME	"uc"
#define	ISSN	"us"
#define	PERSONAL_NAME	"up"
#define	SUBJECT	"uj"
#define	TITLE	"ut"
#define	GEOGRAPHIC_NAME	"ug"
#define	CODEN	"ud"
#define	SUBJECT_SUBDIVISION	"ue"
#define	SERIES_TITLE	"uf"
#define	MICROFORM_GENERATION	"uh"
#define	PLACE_OF_PUBLICATION	"ui"
#define	NUC_CODE	"uk"
#define	LANGUAGE	"ul"
#define	COMBINATION_OF_USE_VALUES	"um"
#define	SYSTEM_CONTROL_NUMBER	"un"
#define	DATE	"uo"
#define	LC_CONTROL_NUMBER	"ur"
#define	MUSIC_PUBLISHERS_NUMBER	"uu"
#define	GOVERNMENT_DOCUMENTS_NUMBER	"uv"
#define	SUBJECT_CLASSIFICATION	"uw"
#define	RECORD_TYPE	"uy"

/* relation value codes */
#define	EQUAL	"re"
#define	GREATER_THAN	"rg"
#define	GREATER_THAN_OR_EQUAL	"ro"
#define	LESS_THAN	"rl"
#define	LESS_THAN_OR_EQUAL	"rp"
#define	NOT_EQUAL	"rn"

/* position value codes */
#define	FIRST_IN_FIELD	"pf"
#define	FIRST_IN_SUBFIELD	"ps"
#define	FIRST_IN_A_SUBFIELD	"pa"
#define	FIRST_IN_NOT_A_SUBFIELD	"pt"
#define	ANY_POSITION_IN_FIELD	"py"

/* structure value codes */
#define	PHRASE	"sp"
#define	WORD	"sw"
#define	KEY	"sk"
#define	WORD_LIST	"sl"

/* truncation value codes */
#define	NO_TRUNCATION	"tn"
#define	RIGHT_TRUNCATION	"tr"
#define	PROC_NUM_INCLUDED_IN_SEARCH_ARG	"ti"

/* completeness value codes */
#define	INCOMPLETE_SUBFIELD	"ci"
#define	COMPLETE_SUBFIELD	"cs"
#define	COMPLETEFIELD	"cf"

/* operator codes */
#define AND	"a"
#define OR	"o"
#define AND_NOT	"n"

/* term types */
#define TT_Attribute		1
#define	TT_ResultSetID		2
#define	TT_Operator			3

#define ATTRIBUTE_SIZE		3
#define OPERATOR_SIZE		2

typedef struct query_term {
  /* type */
  long	TermType;
  /* for term */
  char	Use[ATTRIBUTE_SIZE];
  char	Relation[ATTRIBUTE_SIZE];
  char	Position[ATTRIBUTE_SIZE];
  char	Structure[ATTRIBUTE_SIZE];
  char	Truncation[ATTRIBUTE_SIZE];
  char	Completeness[ATTRIBUTE_SIZE];
  any*	Term;
  /* for result set */
  any*	ResultSetID;
  /* for operator */
  char	Operator[OPERATOR_SIZE];
} query_term;

/*----------------------------------------------------------------------*/
/* functions */

query_term* makeAttributeTerm PARAMS((
        char* use,char* relation,char* position,char* structure,
	char* truncation,char* completeness,any* term));
query_term* makeResultSetTerm PARAMS((any* resultSet));
query_term* makeOperatorTerm PARAMS((char* operatorCode));
void freeTerm PARAMS((void* qt));
char* writeQueryTerm PARAMS((query_term* qt,char* buffer,long* len));
char* readQueryTerm PARAMS((query_term** qt,char* buffer));
any* writeQuery PARAMS((query_term** terms));
query_term** readQuery PARAMS((any* info));

/*----------------------------------------------------------------------*/


/*
**	Routines originally from UI.c -- FM
**
**----------------------------------------------------------------------*/

char *
generate_retrieval_apdu PARAMS((char *buff,
			     long *buff_len,
			     any *docID,
			     long chunk_type,
			     long start_line, long end_line,
			     char *type,
			     char *database_name));


long
interpret_message PARAMS((char *request_message,
		       long request_length,
		       char *response_message,
		       long response_buffer_length, /* length of the buffer (modified)*/
		       long connection,
		       boolean verbose));

char *trim_junk PARAMS((char *headline));



/*
**	Routines originally from ZProt.c -- FM
**
**----------------------------------------------------------------------*/

/* APDU types */

typedef struct InitAPDU {
	pdu_type		PDUType;
	boolean			willSearch,willPresent,willDelete;
	boolean			supportAccessControl,supportResourceControl;
	long			PreferredMessageSize;
	long			MaximumRecordSize;
	char*			IDAuthentication;
	char*			ImplementationID;
	char*			ImplementationName;
	char*			ImplementationVersion;
	any*			ReferenceID;
	void*			UserInformationField;
	} InitAPDU;

typedef struct InitResponseAPDU {
	pdu_type		PDUType;
	boolean			Result;
	boolean			willSearch,willPresent,willDelete;
	boolean			supportAccessControl,supportResourceControl;
	long			PreferredMessageSize;
	long			MaximumRecordSize;
	char*			IDAuthentication;
	char*			ImplementationID;
	char*			ImplementationName;
	char*			ImplementationVersion;
	any*			ReferenceID;
	void*			UserInformationField;
	} InitResponseAPDU;

typedef struct SearchAPDU {
	pdu_type		PDUType;
	long			SmallSetUpperBound;
	long			LargeSetLowerBound;
	long			MediumSetPresentNumber;
	boolean			ReplaceIndicator;
	char*			ResultSetName;
	char**			DatabaseNames;
	char*			QueryType;
	char**			ElementSetNames;
	any*			ReferenceID;
	void*			Query;
	} SearchAPDU;

typedef struct SearchResponseAPDU {
	pdu_type		PDUType;
	long			SearchStatus;
	long			ResultCount;
	long			NumberOfRecordsReturned;
	long			NextResultSetPosition;
	long			ResultSetStatus;
	long			PresentStatus;
	any*			ReferenceID;
	void*			DatabaseDiagnosticRecords;
	} SearchResponseAPDU;

typedef struct PresentAPDU {
	pdu_type		PDUType;
	long			NumberOfRecordsRequested;
	long			ResultSetStartPosition;
	char*			ResultSetID;
	char*			ElementSetNames;
	any*			ReferenceID;
	void*			PresentInfo;
	} PresentAPDU;

typedef struct PresentResponseAPDU {
	pdu_type		PDUType;
	boolean			PresentStatus;
	long			NumberOfRecordsReturned;
	long			NextResultSetPosition;
	any*			ReferenceID;
	void*			DatabaseDiagnosticRecords;
	} PresentResponseAPDU;

/*----------------------------------------------------------------------*/
/* Functions */

InitAPDU* makeInitAPDU PARAMS((boolean search,boolean present,boolean deleteIt,
			    boolean accessControl,boolean resourceControl,
			    long prefMsgSize,long maxMsgSize,
			    char* auth,char* id,char* name, char* version,
			    any* refID,void* userInfo));
void freeInitAPDU PARAMS((InitAPDU* init));
char* writeInitAPDU PARAMS((InitAPDU* init,char* buffer,long* len));
char* readInitAPDU PARAMS((InitAPDU** init,char* buffer));

InitResponseAPDU* makeInitResponseAPDU PARAMS((boolean result,
					    boolean search,boolean present,boolean deleteIt,
					    boolean accessControl,boolean resourceControl,
					    long prefMsgSize,long maxMsgSize,
					    char* auth,char* id,char* name, char* version,
					    any* refID,void* userInfo));
void freeInitResponseAPDU PARAMS((InitResponseAPDU* init));
char* writeInitResponseAPDU PARAMS((InitResponseAPDU* init,char* buffer,long* len));
char* readInitResponseAPDU PARAMS((InitResponseAPDU** init,char* buffer));
InitResponseAPDU* replyToInitAPDU PARAMS((InitAPDU* init,boolean result,void* userInfo));

SearchAPDU* makeSearchAPDU PARAMS((long small,long large, long medium,
				boolean replace,char* name,char** databases,
				char* type,char** elements,any* refID,void* queryInfo));
void freeSearchAPDU PARAMS((SearchAPDU* query));
char* writeSearchAPDU PARAMS((SearchAPDU* query,char* buffer,long* len));
char* readSearchAPDU PARAMS((SearchAPDU** query,char* buffer));

SearchResponseAPDU* makeSearchResponseAPDU PARAMS((long result,long count,
						long recordsReturned,long nextPos,
						long resultStatus,long presentStatus,
						any* refID,void* records));
void freeSearchResponseAPDU PARAMS((SearchResponseAPDU* queryResponse));
char* writeSearchResponseAPDU PARAMS((SearchResponseAPDU* queryResponse,char* buffer,long* len));
char* readSearchResponseAPDU PARAMS((SearchResponseAPDU** queryResponse,char* buffer));

PresentAPDU* makePresentAPDU PARAMS((long recsReq, long startPos,
				  char* resultID,any* refID,void* info));
void freePresentAPDU PARAMS((PresentAPDU* present));
char* writePresentAPDU PARAMS((PresentAPDU* present,char* buffer,long* len));
char* readPresentAPDU PARAMS((PresentAPDU** present,char* buffer));

PresentResponseAPDU* makePresentResponseAPDU PARAMS((boolean status,long recsRet,
						  long nextPos,any* refID,
						  void* records));
void freePresentResponseAPDU PARAMS((PresentResponseAPDU* present));
char* writePresentResponseAPDU PARAMS((PresentResponseAPDU* present,char* buffer,long* len));
char* readPresentResponseAPDU PARAMS((PresentResponseAPDU** present,char* buffer));

/*----------------------------------------------------------------------*/
/* user extension hooks: */

extern char* writeInitInfo PARAMS((InitAPDU* init,char* buffer,long* len));
extern char* readInitInfo PARAMS((void** info,char* buffer));

extern char* writeInitResponseInfo PARAMS((InitResponseAPDU* init,char* buffer,long* len));
extern char* readInitResponseInfo PARAMS((void** info,char* buffer));

extern char* writeSearchInfo PARAMS((SearchAPDU* query,char* buffer,long* len));
extern char* readSearchInfo PARAMS((void** info,char* buffer));

extern char* writeSearchResponseInfo PARAMS((SearchResponseAPDU* query,char* buffer,long* len));
extern char* readSearchResponseInfo PARAMS((void** info,char* buffer));

extern char* writePresentInfo PARAMS((PresentAPDU* present,char* buffer,long* len));
extern char* readPresentInfo PARAMS((void** info,char* buffer));

extern char* writePresentResponseInfo PARAMS((PresentResponseAPDU* present,char* buffer,long* len));
extern char* readPresentResponseInfo PARAMS((void** info,char* buffer));


#endif /* HTVMSWAIS_H */
