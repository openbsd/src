/* $OpenBSD: smtpfilter.c,v 1.3 2001/12/07 18:45:33 mpech Exp $*/

/*
 * smtpfilter, Filter for filtering headers during forwarding them between
 * smtpfwdd and sendmail. Also logs size of message to syslog; may only
 * be invoked with one recipient.
 *
 * OriginalId: smtpfilter.c,v 1.00 1997/3/28 11:04:08 andre Exp $
 */

#define MAX_LINE          1024
#define MY_OUTSIDE_NAME   "curry.mchp.siemens.de"	/* my firewall name outside */
#define MY_INSIDE_DOMAIN  ".tld"			/* my illegal domain inside */

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sysexits.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>

typedef unsigned long int   ULONG;
typedef signed long int     LONG;
typedef unsigned short int  UWORD;
typedef signed short int    WORD;
typedef unsigned char       UBYTE;
typedef signed char         BYTE;

char* strtrim( char* const str );
WORD check( const char* const r, const char* const d );


int main( int argc, const char* const argv[] )
{
  char   tmp_line[ MAX_LINE + sizeof(MY_INSIDE_DOMAIN) ];
  char   line[ MAX_LINE ];
  FILE*  fp;
  char*  sender;
  char*  recipient;
  char*  cp;
  char*  cp2;
  size_t len = 0;
  int  	 state = 0;


  openlog( "smtpfilter", LOG_PID | LOG_NDELAY, LOG_MAIL);


  /*
   * grab arguments 
   */

  if( argc != 4 ) {
    fprintf( stderr, "usage: smtpfilter -f sender recipient\n");
    exit( EX_USAGE );
  }

  if( strcmp( argv[1], "-f" ) ) {
    fprintf( stderr, "usage: smtpfilter -f sender recipient\n");
    exit( EX_USAGE );
  }

  sender    = strtrim( strdup( argv[2] ) );
  recipient = strtrim( strdup( argv[3] ) );


  /*
   * If the recipient is internal don't filter
   */

  if( check( recipient, MY_OUTSIDE_NAME )
   || check( recipient, MY_INSIDE_DOMAIN )
   || strchr( recipient, '@' ) == NULL )

    state = 2;


  /*
   * Open the pipe to sendmail.
   */

  sprintf( line, "/usr/lib/sendmail -f \\<%s\\> \\<%s\\>", sender, recipient );
  if( (fp = popen( line, "w" )) == NULL ) {
    syslog( LOG_ERR, "Could not open pipe to sendmail (%m)" );
    exit( EX_TEMPFAIL );
  }


  /*
   * Filter message
   * state == 0: start
   * state == 1: Within Received: line
   * state == 2: don't filter anymore
   */

  while( fgets( line, sizeof(line), stdin ) != NULL ) {

    line[ MAX_LINE - 1 ] = '\0';
    len += strlen( line );			/* sum up bytes */

    if( state < 2 ) {				/* Still in header */

      if( state == 1 && isspace( *line ) )	/* Received: continuation */
        continue;
  
      state = strncmp( line, "Received:", 9 ) == 0 ? 1 : 0;
      if( state == 1 )
        continue;

      cp = &line[0];				/* find empty line */
      while( isspace( *cp ) )
        cp++;
      if( *cp == '\n' || *cp == '\0' )		/* found, end of header */
        state = 2;
     
      else if( strstr( line, MY_INSIDE_DOMAIN ) != NULL
            && strncmp( line, "From: ", 6 ) != 0 )

        if( strncmp( line, "To: ", 4 ) == 0
         || strstr( line, "Message-ID:" ) != NULL
         || strstr( line, "Message-Id:" ) != NULL
         || strstr( line, "X-Sender:" ) != NULL )

          while( (cp = cp2 = strstr( line, MY_INSIDE_DOMAIN )) != NULL ) {

            while( cp > line ) {
              char c;
              c = *(cp-1);
              if( ! isalnum( c ) && c != '.' && c != '-' )
                break; 
              cp--;
            }
      
            while( isalnum( *cp2 ) || *cp2 == '.' || *cp2 == '-' )
              cp2++;
      
            *cp = '\0';
            strcpy( tmp_line, line );
            strcat( tmp_line, MY_OUTSIDE_NAME );
            strcat( tmp_line, cp2 );

            if( strlen( tmp_line ) > MAX_LINE - 1 )
              syslog( LOG_CRIT, "line to long (possible attack ?), reads: %s", tmp_line );

            strncpy( line, tmp_line, MAX_LINE - 1 );
          }
    
        else
          syslog( LOG_ERR, "unknown line containing %s: %s\n", MY_INSIDE_DOMAIN, line );
    }

    if( fputs( line, fp ) == EOF ) {
      syslog( LOG_ERR, "write failed to pipe (%m)" );
      pclose( fp );
      exit( EX_TEMPFAIL );
    }
  }

  syslog( LOG_NOTICE, "%s %s %lu", sender, recipient, len );

  state = pclose( fp );

  if( (! WIFEXITED( state )) || WEXITSTATUS( state ) != 0 )
    state = WEXITSTATUS( state );

  if( state != 0 && state != EX_NOUSER )
    syslog( LOG_ERR, "sendmail exited with status %d\n", state );
  
  return( state );
}


char* strtrim( char* str )
{
  char*  cp;

  while( (cp = strpbrk( str, "<>" )) != NULL )
    *cp = ' ';

  while( isspace( *str ) )
    str++;

  cp = &str[ strlen( str ) - 1 ];
  while( isspace( *cp ) )
    *cp-- = '\0';

  return( str );
}


WORD check( const char* const r, const char* const d )
{
  size_t dl;

  if( strlen( r ) <  (dl = strlen( d )) )
    return 0;

  return( strcasecmp( &r[ strlen( r ) - dl ], d ) == 0 );
}

