$ ! LYNX.COM
$ ! sets up lynx as a command so that it will accept command line arguments
$ ! It is assumed that this file is located in the same place as the LYNX
$ ! Image.  If it is not then you must change the lynx symbol.
$ ! Written by Danny Mayer, Digital Equipment Corporation
$ !
$ !
$ THIS_PATH = F$PARSE(F$ENV("PROCEDURE"),,,"DEVICE") + -
	      F$PARSE(F$ENV("PROCEDURE"),,,"DIRECTORY")
$ alpha = F$GETSYI("HW_MODEL") .GT. 1023
$ !
$ CPU := VAX
$ IF alpha THEN CPU  :== AXP
$ lynx:==$'THIS_PATH'lynx_'CPU'.exe
$!
$! fill in another gateway if you wish
$!
$define "WWW_wais_GATEWAY" "http://www.w3.org:8001"
$!
$! fill in your NNTP news server here
$!
$ !define "NNTPSERVER" "news"
$ !
$ !  Set up the Proxy Information Here
$ !
$ !  no_proxy environmental variable
$ !  The no_proxy environmental variable is checked to get the list of
$ !  of hosts for which the proxy server is not consulted.
$ !  NOTE:  THE no_proxy VARIABLE MUST BE IN LOWER CASE.  On VMS systems
$ !  this is accomplished by defining a logical name in double-quotes.
$ !
$ !  The no_proxy environmental variable is a comma-separated or
$ !  space-separated list of machine or domain names, with optional
$ !  :port part.  If no :port part is present, it applies to all ports
$ !  on that domain.
$ !
$ !  Example:
$ !          define "no_proxy"  "cern.ch,some.domain:8001"
$ !
$ !
$ define "no_proxy" "yourorg.com"	! Use only for outside of yourorg
$ !
$ !  proxy server environmental variables
$ !  In Lynx, each protocol needs an environmental variable defined for
$ !  it in order for it to use a proxy server set up for that protocol.
$ !  The proxy environmental variable is of the form:
$ !  protocol_proxy where protocol is the protocol name part of the URL,
$ !  for example: http or ftp.  NOTE: the protocol server proxy variable
$ !  MUST BE IN LOWER CASE.
$ !  Example:
$ !           define "http_proxy" "http://your_proxy.yourorg:8080/"
$ !
$ Proxy_Server = "http://your_proxy.yourorg:8080/"
$ define "http_proxy" "''Proxy_Server'"
$ define "ftp_proxy" "''Proxy_Server'"
$ define "gopher_proxy" "''Proxy_Server'"
$ define "news_proxy" "''Proxy_Server'"
$ define "wais_proxy" "''Proxy_Server'"
$ !
