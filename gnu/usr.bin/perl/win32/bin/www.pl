##
## Jeffrey Friedl (jfriedl@omron.co.jp)
## Copyri.... ah hell, just take it.
##
## This is "www.pl".
## Include (require) to use, execute ("perl www.pl") to print a man page.
## Requires my 'network.pl' library.
package www;
$version = "951219.9";

##
## 951219.9
## -- oops, stopped sending garbage Authorization line when no
##    authorization was requested.
##
## 951114.8
## -- added support for HEAD, If-Modified-Since
##
## 951017.7
## -- Change to allow a POST'ed HTTP text to have newlines in it.
##    Added 'NewURL to the open_http_connection %info. Idea courtesy
##    of Bryan Schmersal (http://www.transarc.com/~bryans/Home.html).
##
##
## 950921.6
## -- added more robust HTTP error reporting
##    (due to steven_campbell@uk.ibm.com)
##
## 950911.5
## -- added Authorization support
##

##
## HTTP return status codes.
##
%http_return_code =
    (200,"OK",
     201,"Created",
     202,"Accepted",
     203,"Partial Information",
     204,"No Response",
     301,"Moved",
     302,"Found",
     303,"Method",
     304,"Not modified",
     400,"Bad request",
     401,"Unauthorized",
     402,"Payment required",
     403,"Forbidden",
     404,"Not found",
     500,"Internal error",
     501,"Not implemented",
     502,"Service temporarily overloaded",
     503,"Gateway timeout");

##
## If executed directly as a program, print as a man page.
##
if (length($0) >= 6 && substr($0, -6) eq 'www.pl')
{
   seek(DATA, 0, 0) || die "$0: can't reset internal pointer.\n";
   print "www.pl version $version\n", '=' x 60, "\n";
   while (<DATA>) {
	next unless /^##>/../^##</;   ## select lines to print
	s/^##[<> ]?//;                ## clean up
	print;
   }
   exit(0);
}

##
## History:
##   version 950425.4
##      added require for "network.pl"
##
##   version 950425.3
##      re-did from "Www.pl" which was a POS.
## 
##
## BLURB:
##   A group of routines for dealing with URLs, HTTP sessions, proxies, etc.
##   Requires my 'network.pl' package. The library file can be executed
##   directly to produce a man page.

##>
## A motley group of routines for dealing with URLs, HTTP sessions, proxies,
## etc. Requires my 'network.pl' package.
##
## Latest version, as well as other stuff (including network.pl) available
## at http://www.wg.omron.co.jp/~jfriedl/perl/
##
## Simpleton complete program to dump a URL given on the command-line:
##
##    require 'network.pl';                             ## required for www.pl
##    require 'www.pl';                                 ## main routines
##    $URL = shift;                                     ## get URL
##    ($status, $memo) = &www'open_http_url(*IN, $URL); ## connect
##    die "$memo\n" if $status ne 'ok';                 ## report any error
##    print while <IN>;                                 ## dump contents
##
## There are various options available for open_http_url.
## For example, adding 'quiet' to the call, i.e.       vvvvvvv-----added
##    ($status, $memo) = &www'open_http_url(*IN, $URL, 'quiet');
## suppresses the normal informational messages such as "waiting for data...".
##
## The options, as well as the various other public routines in the package,
## are discussed below.
##
##<

##
## Default port for the protocols whose URL we'll at least try to recognize.
##
%default_port = ('http', 80,
		 'ftp',  21,
		 'gopher', 70,
		 'telnet', 23,
		 'wais', 210,
		 );

##
## A "URL" to "ftp.blah.com" without a protocol specified is probably
## best reached via ftp. If the hostname begins with a protocol name, it's
## easy. But something like "www." maps to "http", so that mapping is below:
##
%name2protocol = (
	'www',	 'http',
	'wwwcgi','http',
);

$last_message_length = 0;
$useragent = "www.pl/$version";

##
##>
##############################################################################
## routine: open_http_url
##
## Used as
##  ($status, $memo, %info) = &www'open_http_url(*FILEHANDLE, $URL, options..)
##
## Given an unused filehandle, a URL, and a list of options, opens a socket
## to the URL and returns with the filehandle ready to read the data of the
## URL. The HTTP header, as well as other information, is returned in %info.
##
## OPTIONS are from among:
##
##   "post"
##	If PATH appears to be a query (i.e. has a ? in it), contact
##	via a POST rather than a GET.
##
##   "nofollow" 
##	Normally, if the initial contact indicates that the URL has moved
##	to a different location, the new location is automatically contacted.
##	"nofollow" inhibits this.
##
##   "noproxy"
##	Normally, a proxy will be used if 'http_proxy' is defined in the
##	environment. This option inhibits the use of a proxy.
##
##   "retry"
##	If a host's address can't be found, it may well be because the
##	nslookup just didn't return in time and that retrying the lookup
##	after a few seconds will succeed. If this option is given, will
##	wait five seconds and try again. May be given multiple times to
##	retry multiple times.
##
##   "quiet"
##	Informational messages will be suppressed.
##
##   "debug"
##	Additional messages will be printed.
##
##   "head"
##      Requests only the file header to be sent
##
##
##
##
## The return array is ($STATUS, $MEMO, %INFO).
##
##    STATUS is 'ok', 'error', 'status', or 'follow'
##
##	If 'error', the MEMO will indicate why (URL was not http, can't
##	connect, etc.). INFO is probably empty, but may have some data.
##	See below.
##
##	If 'status', the connnection was made but the reply was not a normal
##	"OK" successful reply (i.e. "Not found", etc.). MEMO is a note.
##	INFO is filled as noted below. Filehandle is ready to read (unless
##	$info{'BODY'} is filled -- see below), but probably most useful
##	to treat this as an 'error' response.
##
##	If 'follow', MEMO is the new URL (for when 'nofollow' was used to
##	turn off automatic following) and INFO is filled as described
##	below.  Unless you wish to give special treatment to these types of
##	responses, you can just treat 'follow' responses like 'ok'
##	responses.
##
##	If 'ok', the connection went well and the filehandle is ready to
##      read.
##
##   INFO contains data as described at the read_http_header() function (in
##   short, the HTTP response header) and additional informational fields.
##   In addition, the following fields are filled in which describe the raw
##   connection made or attempted:
##
## 	PROTOCOL, HOST, PORT, PATH
##
##   Note that if a proxy is being used, these will describe the proxy.
##   The field TARGET will describe the host or host:port ultimately being
##   contacted. When no proxy is being used, this will be the same info as
##   in the raw connection fields above. However, if a proxy is being used,
##   it will refer to the final target.
##
##   In some cases, the additional entry $info{'BODY'} exists as well. If
##   the result-code indicates an error, the body of the message may be
##   parsed for internal reasons (i.e. to support 'repeat'), and if so, it
##   will be saved in $info{'BODY}.
##
##   If the URL has moved, $info{'NewURL'} will exist and contain the new
##   URL.  This will be true even if the 'nofollow' option is specified.
##   
##<
##
sub open_http_url
{
    local(*HTTP, $URL, @options) = @_;
    return &open_http_connection(*HTTP, $URL, undef, undef, undef, @options);
}


##
##>
##############################################################################
## routine: read_http_header
##
## Given a filehandle to a just-opened HTTP socket connection (such as one
## created via &network'connect_to which has had the HTTP request sent),
## reads the HTTP header and and returns the parsed info.
##
##   ($replycode, %info) = &read_http_header(*FILEHANDLE);
##
## $replycode will be the HTTP reply code as described below, or
## zero on header-read error.
## 
## %info contains two types of fields:
##
##    Upper-case fields are informational from the function.
##    Lower-case fields are the header field/value pairs.
##
##  Upper-case fields:
##
##     $info{'STATUS'} will be the first line read (HTTP status line)
##
##     $info{'CODE'} will be the numeric HTTP reply code from that line.
##       This is also returned as $replycode.
##
##     $info{'TYPE'} is the text from the status line that follows CODE.
##
##     $info{'HEADER'} will be the raw text of the header (sans status line),
##       newlines and all.
##
##     $info{'UNKNOWN'}, if defined, will be any header lines not in the
##       field/value format used to fill the lower-case fields of %info.
##
##  Lower-case fields are reply-dependent, but in general are described
##  in http://info.cern.ch/hypertext/WWW/Protocols/HTTP/Object_Headers.html
##
##  A header line such as
##      Content-type: Text/Plain
##  will appear as $info{'content-type'} = 'Text/Plain';
##
##  (*) Note that while the field names are are lower-cased, the field
##      values are left as-is.
##
##
## When $replycode is zero, there are two possibilities:
##    $info{'TYPE'} is 'empty'
##        No response was received from the filehandle before it was closed.
##        No other %info fields present.
##    $info{'TYPE'} is 'unknown'
##        First line of the response doesn't seem to be proper HTTP.
##        $info{'STATUS'} holds that line. No other %info fields present.
##
## The $replycode, when not zero, is as described at
##        http://info.cern.ch/hypertext/WWW/Protocols/HTTP/HTRESP.html
##
## Some of the codes:
##
##   success 2xx
##    ok 200
##    created 201
##    accepted 202
##    partial information 203
##    no response 204
##   redirection 3xx
##    moved 301
##    found 302
##    method 303
##    not modified 304
##   error 4xx, 5xx
##    bad request 400
##    unauthorized 401
##    paymentrequired 402
##    forbidden 403
##    not found 404
##    internal error 500
##    not implemented 501
##    service temporarily overloaded 502
##    gateway timeout 503
##
##<
##
sub read_http_header
{
    local(*HTTP) = @_;
    local(%info, $_);

    ##
    ## The first line of the response will be the status (OK, error, etc.)
    ##
    unless (defined($info{'STATUS'} = <HTTP>)) {
	$info{'TYPE'} = "empty";
        return (0, %info);
    }
    chop $info{'STATUS'};

    ##
    ## Check the status line. If it doesn't match and we don't know the
    ## format, we'll just let it pass and hope for the best.
    ##
    unless ($info{'STATUS'} =~ m/^HTTP\S+\s+(\d\d\d)\s+(.*\S)/i) {
	$info{'TYPE'} = 'unknown';
        return (0, %info);
    }

    $info{'CODE'} = $1;
    $info{'TYPE'} = $2;
    $info{'HEADER'} = '';

    ## read the rest of the header.
    while (<HTTP>) {
	last if m/^\s*$/;
	$info{'HEADER'} .= $_; ## save whole text of header.

	if (m/^([^\n:]+):[ \t]*(.*\S)/) {
	    local($field, $value) = ("\L$1", $2);
	    if (defined $info{$field}) {
		$info{$field} .= "\n" . $value;
	    } else {
		$info{$field} = $value;
	    }
	} elsif (defined $info{'UNKNOWN'}) {
	    $info{'UNKNOWN'} .= $_;
	} else {
	    $info{'UNKNOWN'} = $_;
	}
    }

    return ($info{'CODE'}, %info);
}

##
##>
##
##############################################################################
## routine: grok_URL(URL, noproxy, defaultprotocol)
##
## Given a URL, returns access information. Deals with
##	http, wais, gopher, ftp, and telnet
## URLs.
##
## Information returned is
##     (PROTOCOL, HOST, PORT, PATH, TARGET, USER, PASSWORD)
##
## If noproxy is not given (or false) and there is a proxy defined
## for the given protocol (via the "*_proxy" environmental variable),
## the returned access information will be for the proxy and will
## reference the given URL. In this case, 'TARGET' will be the
## HOST:PORT of the original URL (PORT elided if it's the default port).
##
## Access information returned:
##   PROTOCOL: "http", "ftp", etc. (guaranteed to be lowercase).
##   HOST: hostname or address as given.
##   PORT: port to access
##   PATH: path of resource on HOST:PORT.
##   TARGET: (see above)
##   USER and PASSWORD: for 'ftp' and 'telnet' URLs, if supplied by the
##      URL these will be defined, undefined otherwise.
##
## If no protocol is defined via the URL, the defaultprotocol will be used
## if given. Otherwise, the URL's address will be checked for a leading
## protocol name (as with a leading "www.") and if found will be used.
## Otherwise, the protocol defaults to http.
##
## Fills in the appropriate default port for the protocol if need be.
##
## A proxy is defined by a per-protocol environmental variable such
## as http_proxy. For example, you might have
##    setenv http_proxy http://firewall:8080/
##    setenv ftp_proxy $http_proxy
## to set it up.
##
## A URL seems to be officially described at
##    http://www.w3.org/hypertext/WWW/Addressing/URL/5_BNF.html
## although that document is a joke of errors.
##
##<
##
sub grok_URL
{
    local($_, $noproxy, $defaultprotocol) = @_;
    $noproxy = defined($noproxy) && $noproxy;

    ## Items to be filled in and returned.
    local($protocol, $address, $port, $path, $target, $user, $password);

    return undef unless m%^(([a-zA-Z]+)://|/*)([^/]+)(/.*)?$%;

    ##
    ## Due to a bug in some versions of perl5, $2 might not be empty
    ## even if $1 is. Therefore, we must check $1 for a : to see if the
    ## protocol stuff matched or not. If not, the protocol is undefined.
    ##
    ($protocol, $address, $path) = ((index($1,":") >= 0 ? $2 : undef), $3, $4);

    if (!defined $protocol)
    {
	##
        ## Choose a default protocol if none given. If address begins with
	## a protocol name (one that we know via %name2protocol or
	## %default_port), choose it. Otherwise, choose http.
	##
	if (defined $defaultprotocol)	{
	    $protocol = $defaultprotocol;
	}
	else
	{
	    $address =~ m/^[a-zA-Z]+/;
	    if (defined($name2protocol{"\L$&"})) {
		$protocol = $name2protocol{"\L$&"};
	    } else {
		$protocol = defined($default_port{"\L$&"}) ? $& : 'http';
	    }
        }
    }
    $protocol =~ tr/A-Z/a-z/; ## ensure lower-case.

    ##
    ## Http support here probably not kosher, but fits in nice for basic
    ## authorization.
    ##
    if ($protocol eq 'ftp' || $protocol eq 'telnet' || $protocol eq 'http')
    {
        ## Glean a username and password from address, if there.
        ## There if address starts with USER[:PASSWORD]@
	if ($address =~ s/^(([^\@:]+)(:([^@]+))?\@)//) {
	    ($user, $password) = ($2, $4);
	}
    }

    ##
    ## address left is (HOSTNAME|HOSTNUM)[:PORTNUM]
    ##
    if ($address =~ s/:(\d+)$//) {
       $port = $1;
    } else {
       $port = $default_port{$protocol};
    }

    ## default path is '/';
    $path = '/' if !defined $path;

    ##
    ## If there's a proxy and we're to proxy this request, do so.
    ##
    local($proxy) = $ENV{$protocol."_proxy"};
    if (!$noproxy && defined($proxy) && !&no_proxy($protocol,$address))
    {
	local($dummy);
	local($old_pass, $old_user);

	##
	## Since we're going through a proxy, we want to send the
	## proxy the entire URL that we want. However, when we're
	## doing Authenticated HTTP, we need to take out the user:password
	## that webget has encoded in the URL (this is a bit sleazy on
	## the part of webget, but the alternative is to have flags, and
	## having them part of the URL like with FTP, etc., seems a bit
	## cleaner to me in the context of how webget is used).
	##
	## So, if we're doing this slezy thing, we need to construct
	## the new URL from the compnents we have now (leaving out password
	## and user), decode the proxy URL, then return the info for
	## that host, a "filename" of the entire URL we really want, and
	## the user/password from the original URL.
	##
	## For all other things, we can just take the original URL,
	## ensure it has a protocol on it, and pass it as the "filename"
	## we want to the proxy host. The difference between reconstructing
	## the URL (as for HTTP Authentication) and just ensuring the
	## protocol is there is, except for the user/password stuff,
	## nothing. In theory, at least.
	##
        if ($protocol eq 'http' && (defined($password) || defined($user)))
	{
	    $path = "http://$address$path";
	    $old_pass = $password;
	    $old_user = $user;
	} else {
	    ## Re-get original URL and ensure protocol// actually there.
	    ## This will become our new path.
	    ($path = $_) =~ s,^($protocol:)?/*,$protocol://,i;
        }

	## note what the target will be
	$target = ($port==$default_port{$protocol})?$address:"$address:$port";

	## get proxy info, discarding
        ($protocol, $address, $port, $dummy, $dummy, $user, $password)
	    = &grok_URL($proxy, 1);
        $password = $old_pass if defined $old_pass;
        $user     = $old_user if defined $old_user;
    }
    ($protocol, $address, $port, $path, $target, $user, $password);
}



##
## &no_proxy($protocol, $host)
##
## Returns true if the specified host is identified in the no_proxy
## environmental variable, or identify the proxy server itself.
##
sub no_proxy
{
    local($protocol, $targethost) = @_;
    local(@dests, $dest, $host, @hosts, $aliases);
    local($proxy) = $ENV{$protocol."_proxy"};
    return 0 if !defined $proxy;
    $targethost =~ tr/A-Z/a-z/; ## ensure all lowercase;

    @dests = ($proxy);
    push(@dests,split(/\s*,\s*/,$ENV{'no_proxy'})) if defined $ENV{'no_proxy'};

    foreach $dest (@dests)
    {
	## just get the hostname
	$host = (&grok_URL($dest, 1), 'http')[1];

	if (!defined $host) {
	    warn "can't grok [$dest] from no_proxy env.var.\n";
	    next;
	}
	@hosts = ($host); ## throw in original name just to make sure
	($host, $aliases) = (gethostbyname($host))[0, 1];

	if (defined $aliases) {
	    push(@hosts, ($host, split(/\s+/, $aliases)));
	} else {
	    push(@hosts, $host);
	}
	foreach $host (@hosts) {
	    next if !defined $host;
	    return 1 if "\L$host" eq $targethost;
	}
    }
    return 0;
}

sub ensure_proper_network_library
{
   require 'network.pl' if !defined $network'version;
   warn "WARNING:\n". __FILE__ .
        qq/ needs a newer version of "network.pl"\n/ if
     !defined($network'version) || $network'version < "950311.5";
}



##
##>
##############################################################################
## open_http_connection(*FILEHANDLE, HOST, PORT, PATH, TARGET, OPTIONS...)
##
## Opens an HTTP connection to HOST:PORT and requests PATH.
## TARGET is used only for informational messages to the user.
##
## If PORT and PATH are undefined, HOST is taken as an http URL and TARGET
## is filled in as needed.
##
## Otherwise, it's the same as open_http_url (including return value, etc.).
##<
##
sub open_http_connection
{
    local(*HTTP, $host, $port, $path, $target, @options) = @_;
    local($post_text, @error, %seen);
    local(%info);

    &ensure_proper_network_library;

    ## options allowed:
    local($post, $retry, $authorization,  $nofollow, $noproxy,
	  $head, $debug, $ifmodifiedsince, $quiet,              ) = (0) x 10;
    ## parse options:
    foreach $opt (@options)
    {
	next unless defined($opt) && $opt ne '';
	local($var, $val);
	if ($opt =~ m/^(\w+)=(.*)/) {
	    ($var, $val) = ($1, $2);
	} else {
	    $var = $opt;
	    $val = 1;
	}
	$var =~ tr/A-Z/a-z/; ## ensure variable is lowercase.
	local(@error);

	eval "if (defined \$$var) { \$$var = \$val; } else { \@error = 
              ('error', 'bad open_http_connection option [$opt]'); }";
        return ('error', "open_http_connection eval: $@") if $@;
	return @error if defined @error;
    }
    $quiet = 0 if $debug;  ## debug overrides quiet
   
    local($protocol, $error, $code, $URL, %info, $tmp, $aite);

    ##
    ## if both PORT and PATH are undefined, treat HOST as a URL.
    ##
    unless (defined($port) && defined($path))
    {
        ($protocol,$host,$port,$path,$target)=&grok_URL($host,$noproxy,'http');
	if ($protocol ne "http") {
	    return ('error',"open_http_connection doesn't grok [$protocol]");
	}
	unless (defined($host)) {
	    return ('error', "can't grok [$URL]");
	}
    }

    return ('error', "no port in URL [$URL]") unless defined $port;
    return ('error', "no path in URL [$URL]") unless defined $path;

    RETRY: while(1)
    {
	## we'll want $URL around for error messages and such.
	if ($port == $default_port{'http'}) {
	    $URL = "http://$host";
	} else {
	    $URL = "http://$host:$default_port{'http'}";
	}
        $URL .= ord($path) eq ord('/') ? $path : "/$path";

	$aite = defined($target) ? "$target via $host" : $host;

	&message($debug, "connecting to $aite ...") unless $quiet;

	##
        ## note some info that might be of use to the caller.
	##
        local(%preinfo) = (
	    'PROTOCOL', 'http',
	    'HOST', $host,
	    'PORT', $port,
	    'PATH', $path,
        );
	if (defined $target) {
	    $preinfo{'TARGET'} = $target;
	} elsif ($default_port{'http'} == $port) {
	    $preinfo{'TARGET'} = $host;
	} else {
	    $preinfo{'TARGET'} = "$host:$port";
	}

	## connect to the site
	$error = &network'connect_to(*HTTP, $host, $port);
	if (defined $error) {
	    return('error', "can't connect to $aite: $error", %preinfo);
	}

	## If we're asked to POST and it looks like a POST, note post text.
	if ($post && $path =~ m/\?/) {
	    $post_text = $'; ## everything after the '?'
	    $path = $`;      ## everything before the '?'
        }

	## send the POST or GET request
	$tmp = $head ? 'HEAD' : (defined $post_text ? 'POST' : 'GET');

	&message($debug, "sending request to $aite ...") if !$quiet;
	print HTTP $tmp, " $path HTTP/1.0\n";

	## send the If-Modified-Since field if needed.
	if ($ifmodifiedsince) {
	    print HTTP "If-Modified-Since: $ifmodifiedsince\n";
	}

	## oh, let's sputter a few platitudes.....
	print HTTP "Accept: */*\n";
	print HTTP "User-Agent: $useragent\n" if defined $useragent;

        ## If doing Authorization, do so now.
        if ($authorization) {
	    print HTTP "Authorization: Basic ",
	        &htuu_encode($authorization), "\n";
	}

	## If it's a post, send it.
	if (defined $post_text)
	{
	    print HTTP "Content-type: application/x-www-form-urlencoded\n";
	    print HTTP "Content-length: ", length $post_text, "\n\n";
	    print HTTP $post_text, "\n";
	}
	print HTTP "\n";
	&message($debug, "waiting for data from $aite ...") unless $quiet;

	## we can now read the response (header, then body) via HTTP.
	binmode(HTTP); ## just in case.

	($code, %info) = &read_http_header(*HTTP);
	&message(1, "header returns code $code ($info{'TYPE'})") if $debug;

	## fill in info from %preinfo
	local($val, $key);
	while (($val, $key) = each %preinfo) {
	    $info{$val} = $key;
	}

	if ($code == 0)
	{
	    return('error',"empty response for $URL")
		if $info{'TYPE'} eq 'empty';
	    return('error', "non-HTTP response for $URL", %info)
		if $info{'TYPE'} eq 'unknown';
	    return('error', "unknown zero-code for $URL", %info);
	}

	if ($code == 302) ## 302 is magic for "Found"
	{
	    if (!defined $info{'location'}) {
		return('error', "No location info for Found URL $URL", %info);
	    }
	    local($newURL) = $info{'location'};

	    ## Remove :80 from hostname, if there. Looks ugly.
	    $newURL =~ s,^(http:/+[^/:]+):80/,$1/,i;
	    $info{"NewURL"} = $newURL;

	    ## if we're not following links or if it's not to HTTP, return.
	    return('follow', $newURL, %info) if
		$nofollow || $newURL!~m/^http:/i;

	    ## note that we've seen this current URL.
	    $seen{$host, $port, $path} = 1;

	    &message(1, qq/[note: now moved to "$newURL"]/) unless $quiet;


	    ## get the new one and return an error if it's been seen.
	    ($protocol, $host, $port, $path, $target) =
		&www'grok_URL($newURL, $noproxy);
	    &message(1, "[$protocol][$host][$port][$path]") if $debug;

	    if (defined $seen{$host, $port, $path})
	    {
		return('error', "circular reference among:\n    ".
		       join("\n    ", sort grep(/^http/i, keys %seen)), %seen);
	    }
	    next RETRY;
	}
	elsif ($code == 500) ## 500 is magic for "internal error"
	{
	    ##
	    ## A proxy will often return this with text saying "can't find
	    ## host" when in reality it's just because the nslookup returned
	    ## null at the time. Such a thing should be retied again after a
	    ## few seconds.
	    ##
	    if ($retry)
	    {
		local($_) = $info{'BODY'} = join('', <HTTP>);
		if (/Can't locate remote host:\s*(\S+)/i) {
		    local($times) = ($retry == 1) ?
			"once more" : "up to $retry more times";
		    &message(0, "can't locate $1, will try $times ...")
			unless $quiet;
		    sleep(5);
		    $retry--;
		    next RETRY;
		}
	    }
	}

	if ($code != 200)  ## 200 is magic for "OK";
	{  
	    ## I'll deal with these as I see them.....
	    &clear_message;
	    if ($info{'TYPE'} eq '')
	    {
		if (defined $http_return_code{$code}) {
		    $info{'TYPE'} = $http_return_code{$code};
		} else {
		    $info{'TYPE'} = "(unknown status code $code)";
		}
	    }
	    return ('status', $info{'TYPE'}, %info);
	}

        &clear_message;
	return ('ok', 'ok', %info);
    }
}


##
## Hyper Text UUencode. Somewhat different from regular uuencode.
##
## Logic taken from Mosaic for X code by Mark Riordan and Ari Luotonen.
##
sub htuu_encode
{
    local(@in) = unpack("C*", $_[0]);
    local(@out);

    push(@in, 0, 0); ## in case we need to round off an odd byte or two
    while (@in >= 3) {
	##
        ## From the next three input bytes,
	## construct four encoded output bytes.
	##
	push(@out, $in[0] >> 2);
	push(@out, (($in[0] << 4) & 060) | (($in[1] >> 4) & 017));
        push(@out, (($in[1] << 2) & 074) | (($in[2] >> 6) & 003));
        push(@out,   $in[2]       & 077);
	splice(@in, 0, 3); ## remove these three
    }

    ##
    ## @out elements are now indices to the string below. Convert to
    ## the appropriate actual text.
    ##
    foreach $new (@out) {
	$new = substr(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
          $new, 1);
    }

    if (@in == 2) {
	## the two left over are the two extra nulls, so we encoded the proper
        ## amount as-is.
    } elsif (@in == 1) {
	## We encoded one extra null too many. Undo it.
	$out[$#out] = '=';
    } else {
        ## We must have encoded two nulls... Undo both.
	$out[$#out   ] = '=';
	$out[$#out -1] = '=';
    }

    join('', @out);
}

##
## This message stuff really shouldn't be here, but in some seperate library.
## Sorry.
##
## Called as &message(SAVE, TEXT ....), it shoves the text to the screen.
## If SAVE is true, bumps the text out as a printed line. Otherwise,
## will shove out without a newline so that the next message overwrites it,
## or it is clearded via &clear_message().
##
sub message
{
    local($nl) = shift;
    die "oops $nl." unless $nl =~ m/^\d+$/;
    local($text) = join('', @_);
    local($NL) = $nl ? "\n" : "\r";
    $thislength = length($text);
    if ($thislength >= $last_message_length) {
	print STDERR $text, $NL;
    } else {
	print STDERR $text, ' 'x ($last_message_length-$thislength), $NL;
    }	
    $last_message_length = $nl ? 0 : $thislength;
}

sub clear_message
{
    if ($last_message_length) {
	print STDERR ' ' x $last_message_length, "\r";
	$last_message_length = 0;
    }
}

1;
__END__
