/*							HTVMS_WAISUI.c
**
**	Adaptation for Lynx by F.Macrides (macrides@sci.wfeb.edu)
**
**	30-May-1994 FM	Initial version.
**
**----------------------------------------------------------------------*/

/*
**	Routines originally from UI.c -- FM
**
**----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE:
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   Brewster@think.com
*/

/*
 * this is a simple ui toolkit for building other ui's on top.
 * -brewster
 *
 * top level functions:
 *   generate_search_apdu
 *   generate_retrieval_apdu
 *   interpret_message
 *
 */

/* to do:
 *   generate multiple queries for long documents.
 *     this will crash if the file being retrieved is larger than 100k.
 *   do log_write()
 *
 */

#include <HTUtils.h>
#include <HTVMS_WaisUI.h>
#include <HTVMS_WaisProt.h>
#include <HTTCP.h>

#undef MAXINT	/* we don't need it here, and tcp.h may conflict */
#include <math.h>

#include <LYexit.h>
#include <LYLeaks.h>

void
log_write(char *s GCC_UNUSED)
{
    return;
}

/*----------------------------------------------------------------------*/

/* returns a pointer in the buffer of the first free byte.
   if it overflows, then NULL is returned
 */
char *
generate_search_apdu(
char* buff,     /* buffer to hold the apdu */
long *buff_len,    /* length of the buffer changed to reflect new data written */
char *seed_words,    /* string of the seed words */
char *database_name,
DocObj** docobjs,
long maxDocsRetrieved)
{
  /* local variables */

  SearchAPDU *search3;
  char  *end_ptr;
  static char *database_names[2] = {"", 0};
  any refID;
  WAISSearch *query;
  refID.size = 1;
  refID.bytes = "3";

  database_names[0] = database_name;
  query = makeWAISSearch(seed_words,
                         docobjs, /* DocObjsPtr */
                         0,
                         1,     /* DateFactor */
                         0,     /* BeginDateRange */
                         0,     /* EndDateRange */
                         maxDocsRetrieved
                         );

  search3 = makeSearchAPDU(30,
			   5000, /* should be large */
			   30,
                           1,	/* replace indicator */
                           "",	/* result set name */
                           database_names, /* database name */
                           QT_RelevanceFeedbackQuery, /* query_type */
                           0,   /* element name */
                           NULL, /* reference ID */
                           query);

  end_ptr = writeSearchAPDU(search3, buff, buff_len);

  CSTFreeWAISSearch(query);
  freeSearchAPDU(search3);
  return(end_ptr);
}

/*----------------------------------------------------------------------*/

/* returns a pointer into the buffer of the next free byte.
   if it overflowed, then NULL is returned
 */

char *
generate_retrieval_apdu(
char *buff,
long *buff_len,    /* length of the buffer changed to reflect new data written */
any *docID,
long chunk_type,
long start,
long end,
char *type,
char *database_name)
{
  SearchAPDU *search;
  char  *end_ptr;

  static char *database_names[2];
  static char *element_names[3];
  any refID;

  DocObj *DocObjs[2];
  any *query;			/* changed from char* by brewster */

  if(NULL == type)
    type = s_strdup("TEXT");

  database_names[0] = database_name;
  database_names[1] = NULL;

  element_names[0] = " ";
  element_names[1] = ES_DocumentText;
  element_names[2] = NULL;

  refID.size = 1;
  refID.bytes = "3";

  switch(chunk_type){
  case CT_line:
    DocObjs[0] = makeDocObjUsingLines(docID, type, start, end);
    break;
  case CT_byte:
    DocObjs[0] = makeDocObjUsingBytes(docID, type, start, end);
    break;
  }
  DocObjs[1] = NULL;

  query = makeWAISTextQuery(DocObjs);
  search = makeSearchAPDU( 10, 16, 15,
			  1,	/* replace indicator */
			  "FOO", /* result set name */
			  database_names, /* database name */
			  QT_TextRetrievalQuery, /* query_type */
			  element_names, /* element name */
			  &refID, /* reference ID */
			  query);
  end_ptr = writeSearchAPDU(search, buff, buff_len);
  CSTFreeWAISTextQuery(query);
  freeSearchAPDU(search);
  return(end_ptr);
}

/*----------------------------------------------------------------------*/

/* this is a safe version of unix 'read' it does all the checking
 * and looping necessary
 * to those trying to modify the transport code to use non-UNIX streams:
 *  This is the function to modify!
 */
PRIVATE long read_from_stream(int d, char *buf, long nbytes)
{
  long didRead;
  long toRead = nbytes;
  long totalRead = 0;		/* paranoia */

  while (toRead > 0){
    didRead = NETREAD (d, buf, (int)toRead);
    if(didRead == HT_INTERRUPTED)
      return(HT_INTERRUPTED);
    if(didRead == -1)		/* error*/
      return(-1);
    if(didRead == 0)		/* eof */
      return(-2);		/* maybe this should return 0? */
    toRead -= didRead;
    buf += didRead;
    totalRead += didRead;
  }
  if(totalRead != nbytes)	/* we overread for some reason */
    return(- totalRead);	/* bad news */
  return(totalRead);
}

/*----------------------------------------------------------------------*/

/* returns the length of the response, 0 if an error */

PRIVATE long
transport_message(
	long connection,
	char *request_message,
	long request_length,
	char *response_message,
	long response_buffer_length)
{
  WAISMessage header;
  long response_length;
  int rv;


  /* Write out message.  Read back header.  Figure out response length. */

  if( request_length + HEADER_LENGTH !=
      NETWRITE(connection,request_message,
   		  (int)( request_length +HEADER_LENGTH)) )
    return 0;

  /* read for the first '0' */

  while(1){
    rv = read_from_stream(connection, response_message, 1);
    if (rv == HT_INTERRUPTED)
      return HT_INTERRUPTED;
    if (rv < 0)
      return 0;
    if('0' == response_message[0])
      break;
  }

  rv = read_from_stream(connection, response_message + 1, HEADER_LENGTH -1);
  if (rv == HT_INTERRUPTED)
    return HT_INTERRUPTED;
  if (rv < 0)
    return 0;

  readWAISPacketHeader(response_message, &header);
  {
    char length_array[11];
    strncpy(length_array, header.msg_len, 10);
    length_array[10] = '\0';
    response_length = atol(length_array);
    /*
      if(verbose){
      printf("WAIS header: '%s' length_array: '%s'\n",
      response_message, length_array);
      }
      */
    if(response_length > response_buffer_length){
      /* we got a message that is too long, therefore empty the message out,
	 and return 0 */
      long i;
      for(i = 0; i < response_length; i++){
	rv = read_from_stream(connection,
			      response_message + HEADER_LENGTH,
			      1);
	if (rv == HT_INTERRUPTED)
	  return HT_INTERRUPTED;
	if (rv < 0)
	  return 0;
      }
      return(0);
    }
  }
  rv = read_from_stream(connection,
			response_message + HEADER_LENGTH,
			response_length);
  if (rv == HT_INTERRUPTED)
    return HT_INTERRUPTED;
  if (rv < 0)
    return 0;
  return(response_length);
}

/*----------------------------------------------------------------------*/

/* returns the number of bytes written.  0 if an error */
long
interpret_message(
	char *request_message,
	long request_length, /* length of the buffer */
	char *response_message,
	long response_buffer_length,
	long connection,
	boolean verbose GCC_UNUSED)
{
  long response_length;

  /* ?
  if(verbose){
    printf ("sending");
    if(hostname_internal && strlen(hostname_internal) > 0)
      printf(" to host %s", hostname_internal);
    if(service_name && strlen(service_name) > 0)
      printf(" for service %s", service_name);
    printf("\n");
    twais_dsply_rsp_apdu(request_message + HEADER_LENGTH,
			 request_length);
  }

  */

  writeWAISPacketHeader(request_message,
			request_length,
			(long)'z',	/* Z39.50 */
			"wais      ", /* server name */
			(long)NO_COMPRESSION,	/* no compression */
			(long)NO_ENCODING,(long)HEADER_VERSION);
  if(connection != 0) {
    response_length = transport_message(connection, request_message,
					request_length,
					response_message,
					response_buffer_length);
    if (response_length == HT_INTERRUPTED)
      return(HT_INTERRUPTED);
  }
  else
      return(0);

  return(response_length);
}

/*----------------------------------------------------------------------*/

/* modifies the string to exclude all seeker codes. sets length to
   the new length. */
PRIVATE char *delete_seeker_codes(char *string, long *length)
{
  long original_count; /* index into the original string */
  long new_count = 0; /* index into the collapsed string */
  for(original_count = 0; original_count < *length; original_count++){
    if(27 == string[original_count]){
      /* then we have an escape code */
      /* if the next letter is '(' or ')', then ignore two letters */
      if('(' == string[original_count + 1] ||
    ')' == string[original_count + 1])
     original_count += 1;    /* it is a term marker */
      else original_count += 4; /* it is a paragraph marker */
    }
    else string[new_count++] = string[original_count];
  }
  *length = new_count;
  return(string);
}

/*----------------------------------------------------------------------*/

#if defined(VMS) && defined(__GNUC__)			/* 10-AUG-1995 [pr] */
/*
  Workaround for an obscure bug in gcc's 2.6.[123] and 2.7.0 vax/vms port;
  sometimes global variables will end up not being defined properly,
  causing first gas to assume they're routines, then the linker to complain
  about unresolved symbols, and finally the program to reference the wrong
  objects (provoking ACCVIO).  It's triggered by the specific ordering of
  variable usage in the source code, hence rarely appears.  This bug is
  fixed in gcc 2.7.1, and was not present in 2.6.0 and earlier.

   Make a reference to VAXCRTL's _ctype_[], and also one to this dummy
   variable itself to prevent any "defined but not used" warning.
 */
static __const void *__const ctype_dummy[] = { &_ctype_, &ctype_dummy };
#endif /* VMS && __GNUC__ */

/* returns a pointer to a string with good stuff */
char *trim_junk(char *headline)
{
  long length = strlen(headline) + 1; /* include the trailing null */
  size_t i;
  headline = delete_seeker_codes(headline, &length);
  /* delete leading spaces */
  for(i=0; i < strlen(headline); i++){
    if(isprint(headline[i])){
      break;
    }
  }
  headline = headline + i;
  /* delete trailing stuff */
  for(i=strlen(headline) - 1 ; i > 0; i--){
    if(isprint(headline[i])){
      break;
    }
    headline[i] = '\0';
  }
  return(headline);
}

/*----------------------------------------------------------------------*/


/*
**	Routines originally from ZProt.c -- FM
**
**----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE:`
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   3.26.90	Harry Morris, morris@think.com
   3.30.90  Harry Morris - Changed any->bits to any->bytes
   4.11.90  HWM - generalized conditional includes (see c-dialect.h)
*/

#define RESERVE_SPACE_FOR_HEADER(spaceLeft)		\
	*spaceLeft -= HEADER_LEN;

#define RELEASE_HEADER_SPACE(spaceLeft)			\
	if (*spaceLeft > 0)				\
	  *spaceLeft += HEADER_LEN;

/*----------------------------------------------------------------------*/

InitResponseAPDU*
makeInitResponseAPDU(
boolean result,
boolean search,
boolean present,
boolean deleteIt,
boolean accessControl,
boolean resourceControl,
long prefSize,
long maxMsgSize,
char* auth,
char* id,
char* name,
char* version,
any* refID,
void* userInfo)
/* build an initResponse APDU with user specified information */
{
  InitResponseAPDU* init = (InitResponseAPDU*)s_malloc((size_t)sizeof(InitResponseAPDU));

  init->PDUType = initResponseAPDU;
  init->Result = result;
  init->willSearch = search;
  init->willPresent = present;
  init->willDelete = deleteIt;
  init->supportAccessControl = accessControl;
  init->supportResourceControl = resourceControl;
  init->PreferredMessageSize = prefSize;
  init->MaximumRecordSize = maxMsgSize;
  init->IDAuthentication = s_strdup(auth);
  init->ImplementationID = s_strdup(id);
  init->ImplementationName = s_strdup(name);
  init->ImplementationVersion = s_strdup(version);
  init->ReferenceID = duplicateAny(refID);
  init->UserInformationField = userInfo; /* not copied! */

  return(init);
}

/*----------------------------------------------------------------------*/

void
freeInitResponseAPDU(InitResponseAPDU* init)
/* free an initAPDU */
{
  s_free(init->IDAuthentication);
  s_free(init->ImplementationID);
  s_free(init->ImplementationName);
  s_free(init->ImplementationVersion);
  freeAny(init->ReferenceID);
  s_free(init);
}

/*----------------------------------------------------------------------*/

char*
writeInitResponseAPDU(InitResponseAPDU* init, char* buffer, long* len)
/* write the initResponse to a buffer, adding system information */
{
  char* buf = buffer + HEADER_LEN; /* leave room for the header-length-indicator */
  long size;
  bit_map* optionsBM = NULL;

  RESERVE_SPACE_FOR_HEADER(len);

  buf = writePDUType(init->PDUType,buf,len);
  buf = writeBoolean(init->Result,buf,len);
  buf = writeProtocolVersion(buf,len);

  optionsBM = makeBitMap((unsigned long)5,init->willSearch,init->willPresent,
                         init->willDelete,init->supportAccessControl,
                         init->supportResourceControl);
  buf = writeBitMap(optionsBM,DT_Options,buf,len);
  freeBitMap(optionsBM);

  buf = writeNum(init->PreferredMessageSize,DT_PreferredMessageSize,buf,len);
  buf = writeNum(init->MaximumRecordSize,DT_MaximumRecordSize,buf,len);
  buf = writeString(init->IDAuthentication,DT_IDAuthentication,buf,len);
  buf = writeString(init->ImplementationID,DT_ImplementationID,buf,len);
  buf = writeString(init->ImplementationName,DT_ImplementationName,buf,len);
  buf = writeString(init->ImplementationVersion,DT_ImplementationVersion,buf,len);
  buf = writeAny(init->ReferenceID,DT_ReferenceID,buf,len);

  /* go back and write the header-length-indicator */
  RELEASE_HEADER_SPACE(len);
  size = buf - buffer - HEADER_LEN;
  writeBinaryInteger(size,HEADER_LEN,buffer,len);

  if (init->UserInformationField != NULL)
    buf = writeInitResponseInfo(init,buf,len);

  return(buf);
}

/*----------------------------------------------------------------------*/

char*
readInitResponseAPDU(InitResponseAPDU** init, char* buffer)
{
  char* buf = buffer;
  boolean search,present,delete,accessControl,resourceControl;
  long prefSize,maxMsgSize;
  char *auth,*id,*name,*version;
  long size;
  pdu_type pduType;
  bit_map* versionBM = NULL;
  bit_map* optionsBM = NULL;
  boolean result;
  any *refID = NULL;
  void* userInfo = NULL;

  auth = id = name = version = NULL;
  refID = NULL;

  /* read required part */
  buf = readBinaryInteger(&size,HEADER_LEN,buf);
  buf = readPDUType(&pduType,buf);
  buf = readBoolean(&result,buf);
  buf = readBitMap(&versionBM,buf);
  buf = readBitMap(&optionsBM,buf);
  buf = readNum(&prefSize,buf);
  buf = readNum(&maxMsgSize,buf);

  /* decode optionsBM */
  search = bitAtPos(0,optionsBM);
  present = bitAtPos(1,optionsBM);
  delete = bitAtPos(2,optionsBM);
  accessControl = bitAtPos(3,optionsBM);
  resourceControl = bitAtPos(4,optionsBM);

  /* read optional part */
  while (buf < (buffer + size + HEADER_LEN))
    { data_tag tag = peekTag(buf);
      switch (tag)
	{ case DT_IDAuthentication:
	    buf = readString(&auth,buf);
	    break;
	  case DT_ImplementationID:
	    buf = readString(&id,buf);
	    break;
	  case DT_ImplementationName:
	    buf = readString(&name,buf);
	    break;
	  case DT_ImplementationVersion:
	    buf = readString(&version,buf);
	    break;
	  case DT_ReferenceID:
	    buf = readAny(&refID,buf);
	    break;
	  default:
	    freeBitMap(versionBM);
	    freeBitMap(optionsBM);
	    s_free(auth);
	    s_free(id);
	    s_free(name);
	    s_free(version);
	    freeAny(refID);
	    REPORT_READ_ERROR(buf);
	    break;
	  }
    }

  buf = readInitResponseInfo(&userInfo,buf);
  if (buf == NULL)
    { freeBitMap(versionBM);
      freeBitMap(optionsBM);
      s_free(auth);
      s_free(id);
      s_free(name);
      s_free(version);
      freeAny(refID);
    }
  RETURN_ON_NULL(buf);

  /* construct the basic init object */
  *init = makeInitResponseAPDU(result,
			       search,present,delete,accessControl,resourceControl,
			       prefSize,maxMsgSize,auth,id,name,version,refID,userInfo);

  freeBitMap(versionBM);
  freeBitMap(optionsBM);
  s_free(auth);
  s_free(id);
  s_free(name);
  s_free(version);
  freeAny(refID);

  return(buf);
}

/*----------------------------------------------------------------------*/

InitResponseAPDU*
replyToInitAPDU(InitAPDU* init, boolean result, void* userInfo)
/* respond to an init message in the default way - echoing back
   the init info
 */
{
  InitResponseAPDU* initResp;
  initResp = makeInitResponseAPDU(result,
				  init->willSearch,init->willPresent,init->willDelete,
				  init->supportAccessControl,init->supportResourceControl,
				  init->PreferredMessageSize,init->MaximumRecordSize,
				  init->IDAuthentication,defaultImplementationID(),defaultImplementationName(),
				  defaultImplementationVersion(),
				  init->ReferenceID,userInfo);
  return(initResp);
}

/*----------------------------------------------------------------------*/

SearchAPDU*
makeSearchAPDU(
long small,
long large,
long medium,
boolean replace,
char* name,
char** databases,
char* type,
char** elements,
any* refID,
void* queryInfo)
{
  char* ptr = NULL;
  long i;
  SearchAPDU* query = (SearchAPDU*)s_malloc((size_t)sizeof(SearchAPDU));
  query->PDUType = searchAPDU;
  query->SmallSetUpperBound = small;
  query->LargeSetLowerBound = large;
  query->MediumSetPresentNumber = medium;
  query->ReplaceIndicator = replace;
  query->ResultSetName = s_strdup(name);
  query->DatabaseNames = NULL;
  if (databases != NULL)
    { for (i = 0, ptr = databases[i]; ptr != NULL; ptr = databases[++i])
	{ if (query->DatabaseNames == NULL)
	    query->DatabaseNames = (char**)s_malloc((size_t)(sizeof(char*) * 2));
        else
          query->DatabaseNames = (char**)s_realloc((char*)query->DatabaseNames,
						   (size_t)(sizeof(char*) * (i + 2)));
	    query->DatabaseNames[i] = s_strdup(ptr);
	    query->DatabaseNames[i+1] = NULL;
	  }
      }
  query->QueryType = s_strdup(type);
  query->ElementSetNames = NULL;
  if (elements != NULL)
    { for (i = 0, ptr = elements[i]; ptr != NULL; ptr = elements[++i])
	{ if (query->ElementSetNames == NULL)
	    query->ElementSetNames = (char**)s_malloc((size_t)(sizeof(char*) * 2));
        else
          query->ElementSetNames = (char**)s_realloc((char*)query->ElementSetNames,
						     (size_t)(sizeof(char*) * (i + 2)));
	    query->ElementSetNames[i] = s_strdup(ptr);
	    query->ElementSetNames[i+1] = NULL;
	  }
      }
  query->ReferenceID = duplicateAny(refID);
  query->Query = queryInfo;	/* not copied! */
  return(query);
}

/*----------------------------------------------------------------------*/

void
freeSearchAPDU(SearchAPDU* query)
{
  s_free(query->ResultSetName);
  s_free(query->QueryType);
  doList((void**)query->DatabaseNames,fs_free); /* can't use the macro here ! */
  s_free(query->DatabaseNames);
  doList((void**)query->ElementSetNames,fs_free); /* can't use the macro here ! */
  s_free(query->ElementSetNames);
  freeAny(query->ReferenceID);
  s_free(query);
}

/*----------------------------------------------------------------------*/

#define DB_DELIMITER 	"\037" 	/* hex 1F occurs between each database name */
#define ES_DELIMITER_1 	"\037" 	/* separates database name from element name */
#define ES_DELIMITER_2 	"\036" 	/* hex 1E separates <db,es> groups from one another */

char*
writeSearchAPDU(SearchAPDU* query, char* buffer, long* len)
{
  char* buf = buffer + HEADER_LEN; /* leave room for the header-length-indicator */
  long size,i;
  char* ptr = NULL;
  char* scratch = NULL;

  RESERVE_SPACE_FOR_HEADER(len);

  buf = writePDUType(query->PDUType,buf,len);
  buf = writeBinaryInteger(query->SmallSetUpperBound,(size_t)3,buf,len);
  buf = writeBinaryInteger(query->LargeSetLowerBound,(size_t)3,buf,len);
  buf = writeBinaryInteger(query->MediumSetPresentNumber,(size_t)3,buf,len);
  buf = writeBoolean(query->ReplaceIndicator,buf,len);
  buf = writeString(query->ResultSetName,DT_ResultSetName,buf,len);
  /* write database names */
  if (query->DatabaseNames != NULL)
    { for (i = 0,scratch = NULL, ptr = query->DatabaseNames[i]; ptr != NULL; ptr = query->DatabaseNames[++i])
	{ if (scratch == NULL)
	    scratch = s_strdup(ptr);
        else
	  { size_t newScratchSize = (size_t)(strlen(scratch) + strlen(ptr) + 2);
	    scratch = (char*)s_realloc(scratch,newScratchSize);
	    s_strncat(scratch,DB_DELIMITER,2,newScratchSize);
	    s_strncat(scratch,ptr,strlen(ptr) + 1,newScratchSize);
	  }
	  }
	buf = writeString(scratch,DT_DatabaseNames,buf,len);
	s_free(scratch);
      }
  buf = writeString(query->QueryType,DT_QueryType,buf,len);
  /* write element set names */
  if (query->ElementSetNames != NULL)
    { for (i = 0,scratch = NULL, ptr = query->ElementSetNames[i]; ptr != NULL; ptr = query->ElementSetNames[++i])
	{ if (scratch == NULL)
	    { if (query->ElementSetNames[i+1] == NULL) /* there is a single element set name */
		{ scratch = (char*)s_malloc((size_t)strlen(ptr) + 2);
		  strncpy(scratch,ES_DELIMITER_1,2);
		  s_strncat(scratch,ptr,strlen(ptr) + 1,strlen(ptr) + 2);
		}
	    else		/* this is the first of a series of element set names */
	      { size_t newScratchSize = (size_t)(strlen(ptr) + strlen(query->ElementSetNames[i + 1]) + 2);
		scratch = s_strdup(ptr); /* the database name */
		ptr = query->ElementSetNames[++i]; /* the element set name */
		scratch = (char*)s_realloc(scratch,newScratchSize);
		s_strncat(scratch,ES_DELIMITER_1,2,newScratchSize);
		s_strncat(scratch,ptr,strlen(ptr) + 1,newScratchSize);
	      }
	      }
        else
	  { char* esPtr = query->ElementSetNames[++i]; /* the element set name */
	    size_t newScratchSize = (size_t)(strlen(scratch) + strlen(ptr) + strlen(esPtr) + 3);
	    scratch = (char*)s_realloc(scratch,newScratchSize);
	    s_strncat(scratch,ES_DELIMITER_2,2,newScratchSize);
	    s_strncat(scratch,ptr,strlen(ptr) + 1,newScratchSize);
	    s_strncat(scratch,ES_DELIMITER_1,2,newScratchSize);
	    s_strncat(scratch,esPtr,strlen(esPtr) + 1,newScratchSize);
	  }
	  }
	buf = writeString(scratch,DT_ElementSetNames,buf,len);
	s_free(scratch);
      }
  buf = writeAny(query->ReferenceID,DT_ReferenceID,buf,len);

  /* go back and write the header-length-indicator */
  RELEASE_HEADER_SPACE(len);
  size = buf - buffer - HEADER_LEN;
  writeBinaryInteger(size,HEADER_LEN,buffer,len);

  if (query->Query != NULL)
    buf = writeSearchInfo(query,buf,len);

  return(buf);
}

/*----------------------------------------------------------------------*/

SearchResponseAPDU*
makeSearchResponseAPDU(
long result,
long count,
long recordsReturned,
long nextPos,
long resultStatus,
long presentStatus,
any* refID,
void* records)
{
  SearchResponseAPDU* query = (SearchResponseAPDU*)s_malloc((size_t)sizeof(SearchResponseAPDU));
  query->PDUType = searchResponseAPDU;
  query->SearchStatus = result;
  query->ResultCount = count;
  query->NumberOfRecordsReturned = recordsReturned;
  query->NextResultSetPosition = nextPos;
  query->ResultSetStatus = resultStatus;
  query->PresentStatus = presentStatus;
  query->ReferenceID = duplicateAny(refID);
  query->DatabaseDiagnosticRecords = records;
  return(query);
}

/*----------------------------------------------------------------------*/

void
freeSearchResponseAPDU(SearchResponseAPDU* queryResponse)
{
  freeAny(queryResponse->ReferenceID);
  s_free(queryResponse);
}

/*----------------------------------------------------------------------*/

char*
writeSearchResponseAPDU(SearchResponseAPDU* queryResponse, char* buffer, long* len)
{
  char* buf = buffer + HEADER_LEN; /* leave room for the header-length-indicator */
  long size;

  RESERVE_SPACE_FOR_HEADER(len);

  buf = writePDUType(queryResponse->PDUType,buf,len);
  buf = writeBinaryInteger(queryResponse->SearchStatus,(size_t)1,buf,len);
  buf = writeBinaryInteger(queryResponse->ResultCount,(size_t)3,buf,len);
  buf = writeBinaryInteger(queryResponse->NumberOfRecordsReturned,(size_t)3,buf,len);
  buf = writeBinaryInteger(queryResponse->NextResultSetPosition,(size_t)3,buf,len);
  buf = writeNum(queryResponse->ResultSetStatus,DT_ResultSetStatus,buf,len);
  buf = writeNum(queryResponse->PresentStatus,DT_PresentStatus,buf,len);
  buf = writeAny(queryResponse->ReferenceID,DT_ReferenceID,buf,len);

  /* go back and write the header-length-indicator */
  RELEASE_HEADER_SPACE(len);
  size = buf - buffer - HEADER_LEN;
  writeBinaryInteger(size,HEADER_LEN,buffer,len);

  if (queryResponse->DatabaseDiagnosticRecords != NULL)
    buf = writeSearchResponseInfo(queryResponse,buf,len);

  return(buf);
}

/*----------------------------------------------------------------------*/

char*
readSearchResponseAPDU(SearchResponseAPDU** queryResponse, char* buffer)
{
  char* buf = buffer;
  long size;
  pdu_type pduType;
  long result,count,recordsReturned,nextPos;
  long resultStatus,presentStatus;
  any *refID = NULL;
  void* userInfo = NULL;

  /* read required part */
  buf = readBinaryInteger(&size,HEADER_LEN,buf);
  buf = readPDUType(&pduType,buf);
  buf = readBinaryInteger(&result,(size_t)1,buf);
  buf = readBinaryInteger(&count,(size_t)3,buf);
  buf = readBinaryInteger(&recordsReturned,(size_t)3,buf);
  buf = readBinaryInteger(&nextPos,(size_t)3,buf);

  resultStatus = presentStatus = UNUSED;
  refID = NULL;

  /* read optional part */
  while (buf < (buffer + size + HEADER_LEN))
    { data_tag tag = peekTag(buf);
      switch (tag)
	{ case DT_ResultSetStatus:
	    buf = readNum(&resultStatus,buf);
	    break;
	  case DT_PresentStatus:
	    buf = readNum(&presentStatus,buf);
	    break;
	  case DT_ReferenceID:
	    buf = readAny(&refID,buf);
	    break;
	  default:
	    freeAny(refID);
	    REPORT_READ_ERROR(buf);
	    break;
	  }
    }

  buf = readSearchResponseInfo(&userInfo,buf);
  if (buf == NULL)
    freeAny(refID);
  RETURN_ON_NULL(buf);

  /* construct the search object */
  *queryResponse = makeSearchResponseAPDU(result,count,recordsReturned,nextPos,
					  (long)resultStatus,(long)presentStatus,refID,userInfo);

  freeAny(refID);

  return(buf);
}


/*
**	Routines originally from ZUtil.c -- FM
**
**----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE:
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   3.26.90	Harry Morris, morris@think.com
   3.30.90  Harry Morris - Changed any->bits to any->bytes
   4.11.90  HWM - fixed include file names, changed
   				- writeCompressedIntegerWithPadding() to
                  writeCompressedIntWithPadding()
                - generalized conditional includes (see c-dialect.h)
   3.7.91   Jonny Goldman.  Replaced "short" in makeBitMap with "int" line 632.
*/

char* readErrorPosition = NULL; /* pos where buf stoped making sense */

/*----------------------------------------------------------------------*/
/* A note on error handling
   read - these are low level routines, they do not check the type tags
   which (sometimes) preceed the data (this is done by the higher
   level functions which call these functions).  There is no
   attempt made to check that the reading does not exceed the read
   buffer.  Such cases should be very rare and usually will be
   caught by the calling functions. (note - it is unlikely that
   a series of low level reads will go far off the edge without
   triggering a type error.  However, it is possible for a single
   bad read in an array function (eg. readAny) to attempt to read a
   large ammount, possibly causing a segmentation violation or out
   of memory condition.
 */
/*----------------------------------------------------------------------*/

diagnosticRecord*
makeDiag(boolean surrogate, char* code, char* addInfo)
{
  diagnosticRecord* diag =
    (diagnosticRecord*)s_malloc((size_t)sizeof(diagnosticRecord));

  diag->SURROGATE = surrogate;
  memcpy(diag->DIAG,code,DIAGNOSTIC_CODE_SIZE);
  diag->ADDINFO = s_strdup(addInfo);

  return(diag);
}

/*----------------------------------------------------------------------*/

void
freeDiag(diagnosticRecord* diag)
{
  if (diag != NULL)
    { if (diag->ADDINFO != NULL)
	s_free(diag->ADDINFO);
	s_free(diag);
      }
}

/*----------------------------------------------------------------------*/

#define END_OF_RECORD	0x1D

char*
writeDiag(diagnosticRecord* diag, char* buffer, long* len)
/* diagnostics (as per Appendix D) have a very weird format - this changes
   in SR-1
 */
{
  char* buf = buffer;
  long  length;

  if (diag == NULL)		/* handle unspecified optional args */
    return(buf);

  buf = writeTag(DT_DatabaseDiagnosticRecords,buf,len);
  CHECK_FOR_SPACE_LEFT(0,len);

  length = 3;
  if (diag->ADDINFO != NULL)
    length += strlen(diag->ADDINFO);

  if (length >= 0xFFFF )	/* make sure the length is reasonable */
    { length = 0xFFFF - 1;
      diag->ADDINFO[0xFFFF - 3 - 1] = '\0';
    }

  buf = writeBinaryInteger(length,2,buf,len);

  CHECK_FOR_SPACE_LEFT(1,len);
  buf[0] = diag->DIAG[0];
  buf++;

  CHECK_FOR_SPACE_LEFT(1,len);
  buf[0] = diag->DIAG[1];
  buf++;

  if (length > 3)
    { CHECK_FOR_SPACE_LEFT(3,len);
      memcpy(buf,diag->ADDINFO,(size_t)length - 3);
      buf += length - 3;
    }

  CHECK_FOR_SPACE_LEFT(1,len);
  buf[0] = diag->SURROGATE;
  buf++;

  CHECK_FOR_SPACE_LEFT(1,len);
  buf[0] = END_OF_RECORD;
  buf++;

  return(buf);
}

/*----------------------------------------------------------------------*/

char*
readDiag(diagnosticRecord** diag, char* buffer)
{
  char* buf = buffer;
  diagnosticRecord* d
    = (diagnosticRecord*)s_malloc((size_t)sizeof(diagnosticRecord));
  data_tag tag;
  long len;

  buf = readTag(&tag,buf);

  buf = readBinaryInteger(&len,2,buf);

  d->DIAG[0] = buf[0];
  d->DIAG[1] = buf[1];
  d->DIAG[2] = '\0';

  if (len > 3)
    { d->ADDINFO = (char*)s_malloc((size_t)(len - 3 + 1));
      memcpy(d->ADDINFO,(char*)(buf + 2),(size_t)(len - 3));
      d->ADDINFO[len - 3] = '\0';
    }
  else
    d->ADDINFO = NULL;

  d->SURROGATE = buf[len - 1];

  *diag = d;

  return(buf + len + 1);
}

/*----------------------------------------------------------------------*/

#define continueBit	0x80
#define dataMask	0x7F
#define dataBits	7

char*
writeCompressedInteger(unsigned long num, char* buf, long* len)
/* write a binary integer in the format described on p. 40.
   this might be sped up
*/
{
  char byte;
  unsigned long i;
  unsigned long size;

  size = writtenCompressedIntSize(num);
  CHECK_FOR_SPACE_LEFT(size,len);

  for (i = size - 1; i != 0; i--)
    { byte = num & dataMask;
      if (i != (size-1))	/* turn on continue bit */
	byte = (char)(byte | continueBit);
      buf[i] = byte;
      num = num >> dataBits;	/* don't and here */
    }

  return(buf + size);
}

/*----------------------------------------------------------------------*/

char*
readCompressedInteger(unsigned long *num, char* buf)
/* read a binary integer in the format described on p. 40.
   this might be sped up
*/
{
  long i = 0;
  unsigned char byte;

  *num = 0;

  do
    { byte = buf[i++];
      *num = *num << dataBits;
      *num += (byte & dataMask);
    }
  while (byte & continueBit);

  return(buf + i);
}

/*----------------------------------------------------------------------*/

#define pad	128 /* high bit is set */

char*
writeCompressedIntWithPadding(
unsigned long num,
unsigned long size,
char* buffer,
long* len)
/* Like writeCompressedInteger, except writes padding (128) to make
   sure that size bytes are used.  This can be read correctly by
   readCompressedInteger()
*/
{
  char* buf = buffer;
  unsigned long needed,padding;
  long i;

  CHECK_FOR_SPACE_LEFT(size,len);

  needed = writtenCompressedIntSize(num);
  padding = size - needed;
  i = padding - 1;

  for (i = padding - 1;i >= 0;i--)
    { buf[i] = pad;
    }

  buf = writeCompressedInteger(num,buf + padding,len);

  return(buf);
}

/*----------------------------------------------------------------------*/

unsigned long
writtenCompressedIntSize(unsigned long num)
/* return the number of bytes needed to represnet the value num in
   compressed format.  curently limited to 4 bytes
 */
{
  if (num < CompressedInt1Byte)
    return(1);
  else if (num < CompressedInt2Byte)
    return(2);
  else if (num < CompressedInt3Byte)
    return(3);
  else
    return(4);
}

/*----------------------------------------------------------------------*/

char*
writeTag(data_tag tag, char* buf, long* len)
/* write out a data tag */
{
  return(writeCompressedInteger(tag,buf,len));
}

/*----------------------------------------------------------------------*/

char*
readTag(data_tag* tag, char* buf)
/* read a data tag */
{
  return(readCompressedInteger(tag,buf));
}

/*----------------------------------------------------------------------*/

unsigned long
writtenTagSize(data_tag tag)
{
  return(writtenCompressedIntSize(tag));
}

/*----------------------------------------------------------------------*/

data_tag
peekTag(char* buf)
/* read a data tag without advancing the buffer */
{
  data_tag tag;
  readTag(&tag,buf);
  return(tag);
}

/*----------------------------------------------------------------------*/

any*
makeAny(unsigned long size, char* data)
{
  any* a = (any*)s_malloc((size_t)sizeof(any));
  a->size = size;
  a->bytes = data;
  return(a);
}

/*----------------------------------------------------------------------*/

void
freeAny(any* a)
/* destroy an any and its associated data.  Assumes a->bytes was
   allocated using the s_malloc family of libraries
 */
{
  if (a != NULL)
    { if (a->bytes != NULL)
	s_free(a->bytes);
      s_free(a);
    }
}

/*----------------------------------------------------------------------*/

any*
duplicateAny(any* a)
{
  any* copy = NULL;

  if (a == NULL)
    return(NULL);

  copy = (any*)s_malloc((size_t)sizeof(any));
  copy->size = a->size;
  if (a->bytes == NULL)
    copy->bytes = NULL;
  else
    { copy->bytes = (char*)s_malloc((size_t)copy->size);
      memcpy(copy->bytes,a->bytes,(size_t)copy->size);
    }
  return(copy);
}

/*----------------------------------------------------------------------*/

char*
writeAny(any* a, data_tag tag, char* buffer, long* len)
/* write an any + tag and size info */
{
  char* buf = buffer;

  if (a == NULL)		/* handle unspecified optional args */
    return(buf);

  /* write the tags */
  buf = writeTag(tag,buf,len);
  buf = writeCompressedInteger(a->size,buf,len);

  /* write the bytes */
  CHECK_FOR_SPACE_LEFT(a->size,len);
  memcpy(buf,a->bytes,(size_t)a->size);

  return(buf+a->size);
}

/*----------------------------------------------------------------------*/


char *readAny(any** anAny, char* buffer)
/* read an any + tag and size info */
{
  char *buf;
  any* a;
  data_tag tag;



a=(any*)s_malloc((size_t)sizeof(any));

  buf=buffer;

  buf = readTag(&tag,buf);

  buf = readCompressedInteger(&a->size,buf);

  /* now simply copy the bytes */
  a->bytes = (char*)s_malloc((size_t)a->size);
  memcpy(a->bytes,buf,(size_t)a->size);
  *anAny = a;

  return(buf+a->size);
}

/*----------------------------------------------------------------------*/

unsigned long
writtenAnySize(data_tag tag, any* a)
{
  unsigned long size;

  if (a == NULL)
    return(0);

  size = writtenTagSize(tag);
  size += writtenCompressedIntSize(a->size);
  size += a->size;
  return(size);
}

/*----------------------------------------------------------------------*/

any*
stringToAny(char* s)
{
  any* a = NULL;

  if (s == NULL)
    return(NULL);

  a = (any*)s_malloc((size_t)sizeof(any));
  a->size = strlen(s);
  a->bytes = (char*)s_malloc((size_t)a->size);
  memcpy(a->bytes,s,(size_t)a->size);
  return(a);
}

/*----------------------------------------------------------------------*/

char*
anyToString(any* a)
{
  char* s = NULL;

  if (a == NULL)
    return(NULL);

  s = s_malloc((size_t)(a->size + 1));
  memcpy(s,a->bytes,(size_t)a->size);
  s[a->size] = '\0';
  return(s);
}

/*----------------------------------------------------------------------*/

char*
writeString(char* s, data_tag tag, char* buffer, long* len)
/* Write a C style string.  The terminating null is not written.
   This function is not part of the Z39.50 spec.  It is provided
   for the convienience of those wishing to pass C strings in
   the place of an any.
 */
{
  char* buf = buffer;
  any* data = NULL;
  if (s == NULL)
    return(buffer);		/* handle unused optional item before making an any */
  data = (any*)s_malloc((size_t)sizeof(any));
  data->size = strlen(s);
  data->bytes = s;		/* save a copy here by not using stringToAny() */
  buf = writeAny(data,tag,buf,len);
  s_free(data);			/* don't use freeAny() since it will free s too */
  return(buf);
}

/*----------------------------------------------------------------------*/

char*
readString(char** s, char* buffer)
/* Read an any and convert it into a C style string.
   This function is not part of the Z39.50 spec.  It is provided
   for the convienience of those wishing to pass C strings in
   the place of an any.
 */
{
  any* data = NULL;
  char* buf = readAny(&data,buffer);
  *s = anyToString(data);
  freeAny(data);
  return(buf);
}

/*----------------------------------------------------------------------*/

unsigned long
writtenStringSize(data_tag tag, char* s)
{
  unsigned long size;

  if (s == NULL)
   return(0);

  size = writtenTagSize(tag);
  size += writtenCompressedIntSize(size);
  size += strlen(s);
  return(size);
}

/*----------------------------------------------------------------------*/

any*
longToAny(long num)
/* a convienience function */
{
  char s[40];

  sprintf(s,"%ld",num);

  return(stringToAny(s));
}

/*----------------------------------------------------------------------*/

long
anyToLong(any* a)
/* a convienience function */
{
  long num;
  char* str = NULL;
  str = anyToString(a);
  sscanf(str,"%ld",&num);	/* could check the result and return
				   an error */
  s_free(str);
  return(num);
}

/*----------------------------------------------------------------------*/

#define bitsPerByte	8

bit_map*
makeBitMap(unsigned long numBits, ...)
/* construct and return a bitmap with numBits elements */
{
  va_list ap;
  unsigned long i,j;
  bit_map* bm = NULL;

  LYva_start(ap,numBits);

  bm = (bit_map*)s_malloc((size_t)sizeof(bit_map));
  bm->size = (unsigned long)(ceil((double)numBits / bitsPerByte));
  bm->bytes = (char*)s_malloc((size_t)bm->size);

  /* fill up the bits */
  for (i = 0; i < bm->size; i++) /* iterate over bytes */
    { char byte = 0;
      for (j = 0; j < bitsPerByte; j++) /* iterate over bits */
	{ if ((i * bitsPerByte + j) < numBits)
	    { boolean bit = false;
	      bit = (boolean)va_arg(ap,boolean);
	      if (bit)
	        { byte = byte | (1 << (bitsPerByte - j - 1));
	        }
	    }
	  }
      bm->bytes[i] = byte;
    }

  va_end(ap);
  return(bm);
}


/*----------------------------------------------------------------------*/

void
freeBitMap(bit_map* bm)
/* destroy a bit map created by makeBitMap() */
{
  s_free(bm->bytes);
  s_free(bm);
}

/*----------------------------------------------------------------------*/

/* use this routine to interpret a bit map.  pos specifies the bit
   number.  bit 0 is the Leftmost bit of the first byte.
   Could do bounds checking.
 */

boolean
bitAtPos(unsigned long pos, bit_map* bm)
{
  if (pos > bm->size*bitsPerByte)
    return false;
  else
    return((bm->bytes[(pos / bitsPerByte)] &
	    (0x80>>(pos % bitsPerByte))) ?
	   true : false);
}

/*----------------------------------------------------------------------*/

char*
writeBitMap(bit_map* bm, data_tag tag, char* buffer, long* len)
/* write a bitmap + type and size info */
{
  return(writeAny((any*)bm,tag,buffer,len));
}

/*----------------------------------------------------------------------*/

char*
readBitMap(bit_map** bm, char* buffer)
/* read a bitmap + type and size info */
{
    char *c;
    c = readAny((any**)bm,buffer);
    return(c);
}

/*----------------------------------------------------------------------*/

char*
writeByte(unsigned long byte, char* buf, long* len)
{
  CHECK_FOR_SPACE_LEFT(1,len);
  buf[0] = byte & 0xFF; /* we really only want the first byte */
  return(buf + 1);
}

/*----------------------------------------------------------------------*/

char*
readByte(unsigned char* byte, char* buf)
{
  *byte = buf[0];
  return(buf + 1);
}

/*----------------------------------------------------------------------*/

char*
writeBoolean(boolean flag, char* buf, long* len)
{
  return(writeByte(flag,buf,len));
}

/*----------------------------------------------------------------------*/

char*
readBoolean(boolean* flag, char* buffer)
{
  unsigned char byte;
  char* buf = readByte(&byte,buffer);
  *flag = (byte == true) ? true : false;
  return(buf);
}

/*----------------------------------------------------------------------*/

char*
writePDUType(pdu_type pduType, char* buf, long* len)
/* PDUType is a single byte */
{
  return(writeBinaryInteger((long)pduType,(unsigned long)1,buf,len));
}

/*----------------------------------------------------------------------*/

char*
readPDUType(pdu_type* pduType, char* buf)
/* PDUType is a single byte */
{
  return(readBinaryInteger((long*)pduType,(unsigned long)1,buf));
}

/*----------------------------------------------------------------------*/

pdu_type
peekPDUType(char* buf)
/* read the next pdu without advancing the buffer, Note that this
   function is to be used on a buffer that is known to contain an
   APDU.  The pdu_type is written HEADER_LEN bytes into the buffer
 */
{
  pdu_type pdu;
  readPDUType(&pdu,buf + HEADER_LEN);
  return(pdu);
}

/*----------------------------------------------------------------------*/

#define BINARY_INTEGER_BYTES	sizeof(long) /* the number of bytes used by
						a "binary integer" */
char*
writeBinaryInteger(long num, unsigned long size, char* buf, long* len)
/* write out first size bytes of num - no type info
  XXX should this take unsigned longs instead ???  */
{
  long i;
  char byte;

  if (size < 1 || size > BINARY_INTEGER_BYTES)
    return(NULL);		/* error */

  CHECK_FOR_SPACE_LEFT(size,len);

  for (i = size - 1; i >= 0; i--)
    { byte = (char)(num & 255);
      buf[i] = byte;
      num = num >> bitsPerByte; /* don't and here */
    }

  return(buf + size);
}

/*----------------------------------------------------------------------*/

char*
readBinaryInteger(long* num, unsigned long size, char* buf)
/* read in first size bytes of num - no type info
  XXX this should take unsigned longs instead !!! */
{
  unsigned long i;
  unsigned char byte;

  if (size < 1 || size > BINARY_INTEGER_BYTES)
    return(buf);		/* error */
  *num = 0;

  for (i = 0; i < size; i++)
    { byte = buf[i];
      *num = *num << bitsPerByte;
      *num += byte;
    }

  return(buf + size);
}

/*----------------------------------------------------------------------*/

unsigned long
writtenCompressedBinIntSize(long num)
/* return the number of bytes needed to represent the value num.
   currently limited to max of 4 bytes
   Only compresses for positive nums - negatives get whole 4 bytes
 */
{
  if (num < 0L)
    return(4);
  else if (num < 256L)		/* 2**8 */
    return(1);
  else if (num < 65536L)	/* 2**16 */
    return(2);
  else if (num < 16777216L)	/* 2**24 */
    return(3);
  else
    return(4);
}

/*----------------------------------------------------------------------*/

char*
writeNum(long num, data_tag tag, char* buffer, long* len)
/* write a binary integer + size and tag info */
{
  char* buf = buffer;
  long size = writtenCompressedBinIntSize(num);

  if (num == UNUSED)
    return(buffer);

  buf = writeTag(tag,buf,len);
  buf = writeCompressedInteger(size,buf,len);
  buf = writeBinaryInteger(num,(unsigned long)size,buf,len);
  return(buf);
}

/*----------------------------------------------------------------------*/

char*
readNum(long* num, char* buffer)
/* read a binary integer + size and tag info */
{
  char* buf = buffer;
  data_tag tag;
  unsigned long size;
  unsigned long val;

  buf = readTag(&tag,buf);
  buf = readCompressedInteger(&val,buf);
  size = (unsigned long)val;
  buf = readBinaryInteger(num,size,buf);
  return(buf);
}

/*----------------------------------------------------------------------*/

unsigned long
writtenNumSize(data_tag tag, long num)
{
  long dataSize = writtenCompressedBinIntSize(num);
  long size;

  size = writtenTagSize(tag); /* space for the tag */
  size += writtenCompressedIntSize(dataSize); /* space for the size */
  size += dataSize; /* space for the data */

  return(size);
}

/*----------------------------------------------------------------------*/

typedef void (voidfunc)(void *);

void
doList(void** list, voidfunc *func)
/* call func on each element of the NULL terminated list of pointers */
{
  register long i;
  register void* ptr = NULL;
  if (list == NULL)
    return;
  for (i = 0,ptr = list[i]; ptr != NULL; ptr = list[++i])
    (*func)(ptr);
}

/*----------------------------------------------------------------------*/

char*
writeProtocolVersion(char* buf, long* len)
/* write a bitmap describing the protocols available */
{
  static bit_map* version = NULL;

  if (version == NULL)
   { version = makeBitMap((unsigned long)1,true); /* version 1! */
   }

  return(writeBitMap(version,DT_ProtocolVersion,buf,len));
}

/*----------------------------------------------------------------------*/

char*
defaultImplementationID(void)
{
  static char	ImplementationID[] = "TMC";
  return(ImplementationID);
}

/*----------------------------------------------------------------------*/

char*
defaultImplementationName(void)
{
  static char ImplementationName[] = "Thinking Machines Corporation Z39.50";
  return(ImplementationName);
}

/*----------------------------------------------------------------------*/

char*
defaultImplementationVersion(void)
{
  static char	ImplementationVersion[] = "2.0A";
  return(ImplementationVersion);
}

/*----------------------------------------------------------------------*/


/*
**	Routines originally from ZType1.c -- FM
**
**----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE:
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   3.26.90	Harry Morris, morris@think.com
   4.11.90  HWM - generalized conditional includes (see c-dialect.h)
*/
/*----------------------------------------------------------------------*/

query_term*
makeAttributeTerm(
char* use,
char* relation,
char* position,
char* structure,
char* truncation,
char* completeness,
any* term)
{
  query_term* qt = (query_term*)s_malloc((size_t)sizeof(query_term));

  qt->TermType = TT_Attribute;

  /* copy in the attributes */
  strncpy(qt->Use,use,ATTRIBUTE_SIZE);
  strncpy(qt->Relation,relation,ATTRIBUTE_SIZE);
  strncpy(qt->Position,position,ATTRIBUTE_SIZE);
  strncpy(qt->Structure,structure,ATTRIBUTE_SIZE);
  strncpy(qt->Truncation,truncation,ATTRIBUTE_SIZE);
  strncpy(qt->Completeness,completeness,ATTRIBUTE_SIZE);

  qt->Term = duplicateAny(term);

  qt->ResultSetID = NULL;

  return(qt);
}

/*----------------------------------------------------------------------*/

query_term*
makeResultSetTerm(any* resultSet)
{
  query_term* qt = (query_term*)s_malloc((size_t)sizeof(query_term));

  qt->TermType = TT_ResultSetID;

  qt->ResultSetID = duplicateAny(resultSet);

  qt->Term = NULL;

  return(qt);
}

/*----------------------------------------------------------------------*/

query_term*
makeOperatorTerm(char* operatorCode)
{
  query_term* qt = (query_term*)s_malloc((size_t)sizeof(query_term));

  qt->TermType = TT_Operator;

  strncpy(qt->Operator,operatorCode,OPERATOR_SIZE);

  qt->Term = NULL;
  qt->ResultSetID = NULL;

  return(qt);
}

/*----------------------------------------------------------------------*/

void
freeTerm(void* param)
{
  query_term* qt = (query_term*)param;
  switch (qt->TermType)
    { case TT_Attribute:
	freeAny(qt->Term);
	break;
      case TT_ResultSetID:
	freeAny(qt->ResultSetID);
	break;
      case TT_Operator:
	/* do nothing */
	break;
      default:
	panic("Implementation error: Unknown term type %ld",
	      qt->TermType);
	break;
      }
  s_free(qt);
}

/*----------------------------------------------------------------------*/

#define ATTRIBUTE_LIST_SIZE	ATTRIBUTE_SIZE * 6
#define AT_DELIMITER	" "

char*
writeQueryTerm(query_term* qt, char* buffer, long* len)
{
  char* buf = buffer;
  char attributes[ATTRIBUTE_LIST_SIZE];

  switch (qt->TermType)
    { case TT_Attribute:
	strncpy(attributes,qt->Use,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,AT_DELIMITER,sizeof(AT_DELIMITER) + 1,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,qt->Relation,ATTRIBUTE_SIZE,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,AT_DELIMITER,sizeof(AT_DELIMITER) + 1,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,qt->Position,ATTRIBUTE_SIZE,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,AT_DELIMITER,sizeof(AT_DELIMITER) + 1,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,qt->Structure,ATTRIBUTE_SIZE,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,AT_DELIMITER,sizeof(AT_DELIMITER) + 1,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,qt->Truncation,ATTRIBUTE_SIZE,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,AT_DELIMITER,sizeof(AT_DELIMITER) + 1,ATTRIBUTE_LIST_SIZE);
	s_strncat(attributes,qt->Completeness,ATTRIBUTE_SIZE,ATTRIBUTE_LIST_SIZE);
	buf = writeString(attributes,DT_AttributeList,buf,len);
	buf = writeAny(qt->Term,DT_Term,buf,len);
	break;
      case TT_ResultSetID:
	buf = writeAny(qt->ResultSetID,DT_ResultSetID,buf,len);
	break;
      case TT_Operator:
	buf = writeString(qt->Operator,DT_Operator,buf,len);
	break;
      default:
	panic("Implementation error: Unknown term type %ld",
	      qt->TermType);
	break;
      }

  return(buf);
}

/*----------------------------------------------------------------------*/

char*
readQueryTerm(query_term** qt, char* buffer)
{
  char* buf = buffer;
  char  *attributeList = NULL;
  char* operator = NULL;
  any* 	term;
  char* use = NULL;
  char* relation = NULL;
  char* position = NULL;
  char* structure = NULL;
  char* truncation = NULL;
  char* completeness;
  any*	resultSetID = NULL;
  data_tag tag;


  tag = peekTag(buffer);

  switch(tag)
    { case DT_AttributeList:
	buf = readString(&attributeList,buf);
	buf = readAny(&term,buf);
	use = strtok(attributeList,AT_DELIMITER);
	relation = strtok(NULL,AT_DELIMITER);
	position = strtok(NULL,AT_DELIMITER);
	structure = strtok(NULL,AT_DELIMITER);
	truncation = strtok(NULL,AT_DELIMITER);
	completeness = strtok(NULL,AT_DELIMITER);
	*qt = makeAttributeTerm(use,relation,position,structure,
				truncation,completeness,term);
	s_free(attributeList);
	freeAny(term);
	break;
      case DT_ResultSetID:
	buf = readAny(&resultSetID,buf);
	*qt = makeResultSetTerm(resultSetID);
	freeAny(resultSetID);
	break;
      case DT_Operator:
	buf = readString(&operator,buf);
	*qt = makeOperatorTerm(operator);
	s_free(operator);
	break;
      default:
	REPORT_READ_ERROR(buf);
	break;
      }

  return(buf);
}

/*----------------------------------------------------------------------*/

static unsigned long getQueryTermSize PARAMS((query_term* qt));

static unsigned long
getQueryTermSize(query_term* qt)
/* figure out how many bytes it will take to write this query */
{
  unsigned long size = 0;
  static char attributes[] = "11 22 33 44 55 66"; /* we just need this to
						     calculate its written
						     size */

  switch (qt->TermType)
    { case TT_Attribute:
	size = writtenStringSize(DT_AttributeList,attributes);
	size += writtenAnySize(DT_Term,qt->Term);
	break;
      case TT_ResultSetID:
	size = writtenAnySize(DT_ResultSetID,qt->ResultSetID);
	break;
      case TT_Operator:
	size = writtenStringSize(DT_Operator,qt->Operator);
	break;
      default:
	panic("Implementation error: Unknown term type %ld",
	      qt->TermType);
	break;
      }

  return(size);
}

/*----------------------------------------------------------------------*/

/* A query is simply a null terminated list of query terms.  For
   transmission, a query is written into an any which is sent as
   the user information field. */

any*
writeQuery(query_term** terms)
{
  any* info = NULL;
  char* writePos = NULL;
  char* data = NULL;
  unsigned long size = 0;
  long remaining = 0;
  long i;
  query_term* qt = NULL;

  if (terms == NULL)
    return(NULL);

  /* calculate the size of write buffer */
  for (i = 0,qt = terms[i]; qt != NULL; qt = terms[++i])
    size += getQueryTermSize(qt);

  data = (char*)s_malloc((size_t)size);

  /* write the terms */
  writePos = data;
  remaining = size;
  for (i = 0,qt = terms[i]; qt != NULL; qt = terms[++i])
    writePos = writeQueryTerm(qt,writePos,&remaining);

  info = makeAny(size,data);

  return(info);
}

/*----------------------------------------------------------------------*/

query_term**
readQuery(any *info)
{
  char* readPos = info->bytes;
  query_term** terms = NULL;
  query_term* qt = NULL;
  long numTerms = 0L;
char tmp[100];

sprintf(tmp,"readquery: bytes: %ld",info->size);
log_write(tmp);

  while (readPos < info->bytes + info->size)
    { readPos = readQueryTerm(&qt,readPos);

      if (terms == NULL)
	{ terms = (query_term**)s_malloc((size_t)(sizeof(query_term*)*2));
	}
      else
	{ terms =
	    (query_term**)s_realloc((char*)terms,
				    (size_t)(sizeof(query_term*)*(numTerms+2)));
	  }
if(qt==NULL)
	log_write("qt = null");
      terms[numTerms++] = qt;
      terms[numTerms] = NULL;
    }

  return(terms);
}

/*----------------------------------------------------------------------*/


/*
**	Routines originally from panic.c -- FM
**
**----------------------------------------------------------------------*/
/* WIDE AREA INFORMATION SERVER SOFTWARE:
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   Morris@think.com
*/

/* panic is an error system interface.  On the Mac, it will pop
 * up a little window to explain the problem.
 * On a unix box, it will print out the error and call perror()
 */

/*----------------------------------------------------------------------*/

static void exitAction PARAMS((long error));

static void
exitAction(long error GCC_UNUSED)
{
  long i;
  for (i = 0; i < 100000; i++)
    ;
  exit(0);
}

/*----------------------------------------------------------------------*/

#define PANIC_HEADER "Fatal Error:  "

void
panic(char *format, ...)
{
  va_list ap;			/* the variable arguments */

  fprintf(stderr,PANIC_HEADER);
  LYva_start(ap, format);	/* init ap */
  vfprintf(stderr,format,ap);	/* print the contents */
  va_end(ap);			/* free ap */
  fflush(stderr);

  exitAction(0);
}

/*----------------------------------------------------------------------*/


/*
**	Routines originally from cutil.c -- FM
**
**----------------------------------------------------------------------*/
/* Wide AREA INFORMATION SERVER SOFTWARE
   No guarantees or restrictions.  See the readme file for the full standard
   disclaimer.

   3.26.90	Harry Morris, morris@think.com
   4.11.90  HWM - generalized conditional includes (see c-dialect.h)
*/

/*----------------------------------------------------------------------*/

void
fs_checkPtr(void* ptr)
/* If the ptr is NULL, give an error */
{
  if (ptr == NULL)
    panic("checkPtr found a NULL pointer");
}

/*----------------------------------------------------------------------*/

void*
fs_malloc(size_t size)
/* does safety checks and optional accounting */
{
  register void* ptr = NULL;

  ptr = (void*)calloc((size_t)size,(size_t)1);
  s_checkPtr(ptr);

  return(ptr);
}

/*----------------------------------------------------------------------*/

void*
fs_realloc(void* ptr, size_t size)
/* does safety checks and optional accounting
   note - we don't know how big ptr's memory is, so we can't ensure
   that any new memory allocated is NULLed!
 */
{
  register void* nptr = NULL;

  if (ptr == NULL)		/* this is really a malloc */
    return(s_malloc(size));

  nptr = (void*)realloc(ptr,size);
  s_checkPtr(ptr);

  return(nptr);
}

/*----------------------------------------------------------------------*/

void
fs_free(void* ptr)
/* does safety checks and optional accounting */
{
  if (ptr != NULL)		/* some non-ansi compilers/os's cant handle freeing null */
    {				/* if we knew the size of this block of memory, we could clear it - oh well */
      free(ptr);
      ptr = NULL;
    }
}

/*----------------------------------------------------------------------*/

char*
s_strdup(char* s)

/* return a copy of s.  This is identical to the standard library routine
   strdup(), except that it is safe.  If s == NULL or malloc fails,
   appropriate action is taken.
 */
{
  unsigned long len;
  char* copy = NULL;

  if (s == NULL)		/* saftey check to postpone stupid errors */
    return(NULL);

  len = strlen(s);		/* length of string - terminator */
  copy = (char*)s_malloc((size_t)(sizeof(char)*(len + 1)));
  strncpy(copy,s,len + 1);
  return(copy);
}

/*----------------------------------------------------------------------*/

char*
fs_strncat(char* dst, char* src, size_t maxToAdd, size_t maxTotal)

/* like strncat, except the fourth argument limits the maximum total
   length of the resulting string
 */
{
  size_t dstSize = strlen(dst);
  size_t srcSize = strlen(src);

  if (dstSize + srcSize < maxTotal) /* use regular old strncat */
    return(strncat(dst,src,maxToAdd));
  else
    { size_t truncateTo = maxTotal - dstSize - 1;
      char   saveChar = src[truncateTo];
      char*  result = NULL;
      src[truncateTo] = '\0';
      result = strncat(dst,src,maxToAdd);
      src[truncateTo] = saveChar;
      return(result);
    }
}

/*----------------------------------------------------------------------*/

char char_downcase(unsigned long long_ch)
{
  unsigned char ch = long_ch & 0xFF; /* just want one byte */
  /* when ansi is the way of the world, this can be tolower */
  return (((ch >= 'A') && (ch <= 'Z')) ? (ch + 'a' -'A') : ch);
}

char *string_downcase(char *word)
{
  long i = 0;
  while(word[i] != '\0'){
    word[i] = char_downcase((unsigned long)word[i]);
    i++;
  }
  return(word);
}

/*----------------------------------------------------------------------*/

