#!/usr/local/bin/perl -w

#-
#!/usr/local/bin/perl -w
$version = "951121.18";
$comments = 'jfriedl@omron.co.jp';

##
## This is "webget"
##
## Jeffrey Friedl (jfriedl@omron.co.jp), July 1994.
## Copyright 19.... ah hell, just take it.
## Should work with either perl4 or perl5
##
## BLURB:
## Given a URL on the command line (HTTP and FTP supported at the moment),
## webget fetches the named object (HTML text, images, audio, whatever the
## object happens to be). Will automatically use a proxy if one is defined
## in the environment, follow "this URL has moved" responses, and retry
## "can't find host" responses from a proxy in case host lookup was slow).
## Supports users & passwords (FTP), Basic Authorization (HTTP), update-if-
## modified (HTTP), and much more. Works with perl4 or perl5.

##
## More-detailed instructions in the comment block below the history list.
##

##
## To-do:
##   Add gopher support.
##   Fix up how error messages are passed among this and the libraries.
##   

##   951219.19
##	Lost ftp connections now die with a bit more grace.
##
##   951121.18
##	Add -nnab.
##      Brought the "usage" string in line with reality.
##
##   951114.17
##      Added -head.
##	Added -update/-refresh/-IfNewerThan. If any URL was not pulled
##	because it was not out of date, an exit value of 2 is returned.
##
##   951031.16
##	Added -timeout. Cleaned up (a bit) the exit value. Now exits
##	with 1 if all URLs had some error (timeout exits immediately with
##	code 3, though. This is subject to change). Exits with 0 if any
##	URL was brought over safely.
##
##   951017.15
##     Neat -pf, -postfile idea from Lorrie Cranor
##     (http://www.ccrc.wustl.edu/~lorracks/)
##
##   950912.14
##     Sigh, fixed a typo.
##
##   950911.13
##     Added Basic Authorization support for http. See "PASSWORDS AND STUFF"
##     in the documentation.
##
##   950911.12
##     Implemented a most-excellent suggestion by Anthony D'Atri
##     (aad@nwnet.net), to be able to automatically grab to a local file of
##     the same name as the URL. See the '-nab' flag.
##
##   950706.11
##     Quelled small -w warning (thanks: Lars Rasmussen <gnort@daimi.aau.dk>)
##
##   950630.10
##     Steve Campbell to the rescue again. FTP now works when supplied
##     with a userid & password (eg ftp://user:pass@foo.bar.com/index.txt).
##
##   950623.9
##     Incorporated changes from Steve Campbell (steven_campbell@uk.ibm.com)
##     so that the ftp will work when no password is required of a user.
##
##   950530.8
##     Minor changes:
##     Eliminate read-size warning message when size unknown.
##     Pseudo-debug/warning messages at the end of debug_read now go to
##     stderr. Some better error handling when trying to contact systems
##     that aren't really set up for ftp. Fixed a bug concerning FTP access
##     to a root directory. Added proxy documentation at head of file.
##
##   950426.6,7
##     Complete Overhaul:
##     Renamed from httpget. Added ftp support (very sketchy at the moment).
##     Redid to work with new 'www.pl' library; chucked 'Www.pl' library.
##     More or less new and/or improved in many ways, but probably introduced
##     a few bugs along the way.
##
##   941227.5
##     Added follow stuff (with -nofollow, etc.)
##     Added -updateme. Cool!
##     Some general tidying up.
##
##   941107.4
##     Allowed for ^M ending a header line... PCs give those kind of headers.
##
##   940820.3
##     First sorta'clean net release.
##
##

##
##>
##
## Fetch http and/or ftp URL(s) given on the command line and spit to
## STDOUT.
##
## Options include:
##  -V, -version
##	Print version information; exit.
##
##  -p, -post
##	If the URL looks like a reply to a form (i.e. has a '?' in it),
##	the request is POST'ed instead of GET'ed.
##
##  -head
##	Gets the header only (for HTTP). This might include such useful
##	things as 'Last-modified' and 'Content-length' fields
##	(a lack of a 'Last-modified' might be a good indication that it's
##	a CGI).
##
##      The "-head" option implies "-nostrip", but does *not* imply,
##      for example "-nofollow".
##
##
##  -pf, -postfile
##	The item after the '?' is taken as a local filename, and the contents
##	are POST'ed as with -post
##
##  -nab, -f, -file
##      Rather than spit the URL(s) to standard output, unconditionally
##      dump to a file (or files) whose name is that as used in the URL,
##      sans path. I like '-nab', but supply '-file' as well since that's
##      what was originally suggested. Also see '-update' below for the
## 	only-if-changed version.
##
##  -nnab
##      Like -nab, but in addtion to dumping to a file, dump to stdout as well.
##      Sort of like the 'tee' command.
##
##  -update, -refresh
##	Do the same thing as -nab, etc., but does not bother pulling the
##	URL if it older than the localfile. Only applies to HTTP.
##	Uses the HTTP "If-Modified-Since" field. If the URL was not modified
##	(and hence not changed), the return value is '2'.
##
##  -IfNewerThan FILE
##  -int FILE
##	Only pulls URLs if they are newer than the date the local FILE was
##	last written.
##
##  -q, -quiet
##	Suppresses all non-essential informational messages.
##
##  -nf, -nofollow
##	Normally, a "this URL has moved" HTTP response is automatically
##	followed. Not done with -nofollow.
##
##  -nr, -noretry
##	Normally, an HTTP proxy response of "can't find host" is retried
##	up to three times, to give the remote hostname lookup time to
##	come back with an answer. This suppresses the retries. This is the
##	same as '-retry 0'.
##
##  -r#, -retry#, -r #, -retry #
##	Sets the number of times to retry. Default 3.
##
##  -ns, -nostrip
##	For HTTP items (including other items going through an HTTP proxy),
##	the HTTP response header is printed rather than stripped as default.
##
##  -np, -noproxy
##	A proxy is not used, even if defined for the protocol.
##
##  -h, -help
##	Show a usage message and exit.
##
##  -d, -debug
##	Show some debugging messages.
##
##  -updateme
## 	The special and rather cool flag "-updateme" will see if webget has
## 	been updated since you got your version, and prepare a local
## 	version of the new version for you to use. Keep updated! (although
## 	you can always ask to be put on the ping list to be notified when
## 	there's a new version -- see the author's perl web page).
##
##  -timeout TIMESPAN
##  -to TIMESPAN
##	Time out if a connection can not be made within the specified time
##      period. TIMESPAN is normally in seconds, although a 'm' or 'h' may
##	be appended to indicate minutes and hours. "-to 1.5m" would timeout
##	after 90 seconds.
##	
##	(At least for now), a timeout causes immediate program death (with
##	exit value 3).  For some reason, the alarm doesn't always cause a
##	waiting read or connect to abort, so I just die immediately.. /-:
##
##	I might consider adding an "entire fetch" timeout, if someone
##	wants it.
##
## PASSWORDS AND SUCH
##
##  You can use webget to do FTP fetches from non-Anonymous systems and
##  accounts. Just put the required username and password into the URL,
##  as with
##	webget 'ftp:/user:password@ftp.somesite.com/pub/pix/babe.gif
##                   ^^^^^^^^^^^^^
##  Note the user:password is separated from the hostname by a '@'.
##
##  You can use the same kind of thing with HTTP, and if so it will provide
##  what's know as Basic Authorization. This is >weak< authorization.  It
##  also provides >zero< security -- I wouldn't be sending any credit-card
##  numbers this way (unless you send them 'round my way :-). It seems to
##  be used most by providers of free stuff where they want to make some
##  attempt to limit access to "known users".
##
## PROXY STUFF
##
##  If you need to go through a gateway to get out to the whole internet,
##  you can use a proxy if one's been set up on the gateway. This is done
##  by setting the "http_proxy" environmental variable to point to the
##  proxy server. Other variables are used for other target protocols....
##  "gopher_proxy", "ftp_proxy", "wais_proxy", etc.
##
##  For example, I have the following in my ".login" file (for use with csh):
##
##       setenv http_proxy http://local.gateway.machine:8080/
##
##  This is to indicate that any http URL should go to local.gateway.machine
##  (port 8080) via HTTP.  Additionally, I have
##
##       setenv gopher_proxy "$http_proxy"
##       setenv wais_proxy   "$http_proxy"
##       setenv ftp_proxy    "$http_proxy"
##
##  This means that any gopher, wais, or ftp URL should also go to the
##  same place, also via HTTP. This allows webget to get, for example,
##  GOPHER URLs even though it doesn't support GOPHER itself. It uses HTTP
##  to talk to the proxy, which then uses GOPHER to talk to the destination.
##
##  Finally, if there are sites inside your gateway that you would like to
##  connect to, you can list them in the "no_proxy" variable. This will allow
##  you to connect to them directly and skip going through the proxy:
##
##       setenv no_proxy     "www.this,www.that,www.other"
##
##  I (jfriedl@omron.co.jp) have little personal experience with proxies
##  except what I deal with here at Omron, so if this is not representative
##  of your situation, please let me know.
##
## RETURN VALUE
##  The value returned to the system by webget is rather screwed up because
##  I didn't think about dealing with it until things were already
##  complicated. Since there can be more than one URL on the command line,
##  it's hard to decide what to return when one times out, another is fetched,
##  another doesn't need to be fetched, and a fourth isn't found.
##
##  So, here's the current status:
##   
##	Upon any timeout (via the -timeout arg), webget immediately
##	returns 3. End of story. Otherwise....
##
##	If any URL was fetched with a date limit (i.e. via
##	'-update/-refresh/-IfNewerThan' and was found to not have changed,
##	2 is returned. Otherwise....
##
##	If any URL was successfully fetched, 0 is returned. Otherwise...
##
##	If there were any errors, 1 is returned. Otherwise...
##
##	Must have been an info-only or do-nothing instance. 0 is returned.
##
##  Phew. Hopefully useful to someone.
##<
##

## Where latest version should be.
$WEB_normal  = 'http://www.wg.omron.co.jp/~jfriedl/perl/webget';
$WEB_inlined = 'http://www.wg.omron.co.jp/~jfriedl/perl/inlined/webget';


require 'network.pl'; ## inline if possible (directive to a tool of mine)
require 'www.pl';     ## inline if possible (directive to a tool of mine)
$inlined=0;           ## this might be changed by a the inline thing.

##
## Exit values. All screwed up.
##
$EXIT_ok          = 0;
$EXIT_error       = 1;
$EXIT_notmodified = 2;
$EXIT_timeout     = 3;

##
##

warn qq/WARNING:\n$0: need a newer version of "network.pl"\n/ if
  !defined($network'version) || $network'version < "950311.5";
warn qq/WARNING:\n$0: need a newer version of "www.pl"\n/ if
  !defined($www'version) || $www'version < "951114.8";

$WEB = $inlined ? $WEB_inlined : $WEB_normal;

$debug = 0;
$strip = 1;           ## default is to strip
$quiet = 0;           ## also normally off.
$follow = 1;          ## normally, we follow "Found (302)" links
$retry = 3;           ## normally, retry proxy hostname lookups up to 3 times.
$nab = 0;             ## If true, grab to a local file of the same name.
$refresh = 0;	      ## If true, use 'If-Modified-Since' with -nab get.
$postfile = 0;	      ## If true, filename is given after the '?'
$defaultdelta2print = 2048;
$TimeoutSpan = 0;     ## seconds after which we should time out.

while (@ARGV && $ARGV[0] =~ m/^-/)
{
    $arg = shift(@ARGV);

    $nab = 1,                           next if $arg =~ m/^-f(ile)?$/;
    $nab = 1,                           next if $arg =~ m/^-nab$/;
    $nab = 2,                           next if $arg =~ m/^-nnab$/;
    $post = 1,				next if $arg =~ m/^-p(ost)?$/i;
    $post = $postfile = 1,		next if $arg =~ m/^-p(ost)?f(ile)?$/i;
    $quiet=1, 				next if $arg =~ m/^-q(uiet)?$/;
    $follow = 0, 			next if $arg =~ m/^-no?f(ollow)?$/;
    $strip = 0,				next if $arg =~ m/^-no?s(trip)?$/;
    $debug=1, 				next if $arg =~ m/^-d(ebug)?$/;
    $noproxy=1,				next if $arg =~ m/^-no?p(roxy)?$/;
    $retry=0,				next if $arg =~ m/^-no?r(etry)?$/;
    $retry=$2,				next if $arg =~ m/^-r(etry)?(\d+)$/;
    &updateme				     if $arg eq '-updateme';
    $strip = 0, $head = 1,              next if $arg =~ m/^-head(er)?/;
    $nab = $refresh = 1,                next if $arg =~ m/^-(refresh|update)/;

    &usage($EXIT_ok) if $arg =~ m/^-h(elp)?$/;
    &show_version, exit($EXIT_ok) if $arg eq '-version' || $arg eq '-V';

    if ($arg =~ m/^-t(ime)?o(ut)?$/i) {
	local($num) = shift(@ARGV);
        &usage($EXIT_error, "expecting timespan argument to $arg\n") unless
		$num =~ m/^\d+(\d*)?[hms]?$/;
	&timeout_arg($num);
	next;
    }
    
    if ($arg =~ m/^-if?n(ewer)?t(han)?$/i) {
	$reference_file = shift(@ARGV);
        &usage($EXIT_error, "expecting filename arg to $arg")
	   if !defined $reference_file;
        if (!-f $reference_file) {
	   warn qq/$0: ${arg}'s "$reference_file" not found.\n/;
	   exit($EXIT_error);
	}
	next;
    }

    if ($arg eq '-r' || $arg eq '-retry') {
	local($num) = shift(@ARGV);
	&usage($EXIT_error, "expecting numerical arg to $arg\n") unless
	   defined($num) && $num =~ m/^\d+$/;
	$retry = $num;
	next;
    }
    &usage($EXIT_error, qq/$0: unknown option "$arg"\n/);
}

if ($head && $post) {
    warn "$0: combining -head and -post makes no sense, ignoring -post.\n";
    $post = 0;
    undef $postfile;
}

if ($refresh && defined($reference_file)) {
    warn "$0: combining -update and -IfNewerThan make no sense, ignoring -IfNewerThan.\n";
    undef $reference_file;
}

if (@ARGV == 0) {
   warn "$0: nothing to do. Use -help for info.\n";
   exit($EXIT_ok);
}


##
## Now run through the remaining arguments (mostly URLs) and do a quick
## check to see if they look well-formed. We won't *do* anything -- just
## want to catch quick errors before really starting the work.
##
@tmp = @ARGV;
$errors = 0;
while (@tmp) {
    $arg = shift(@tmp);
    if ($arg =~ m/^-t(ime)?o(ut)?$/) {
	local($num) = shift(@tmp);
	if ($num !~ m/^\d+(\d*)?[hms]?$/) {
	    &warn("expecting timespan argument to $arg\n");
	    $errors++;
	}		
    } else {
        local($protocol) = &www'grok_URL($arg, $noproxy);

        if (!defined $protocol) {
	    warn qq/can't grok "$arg"/;
	    $errors++;
	} elsif (!$quiet && ($protocol eq 'ftp')) {
	    warn qq/warning: -head ignored for ftp URLs\n/   if $head;
	    warn qq/warning: -refresh ignored for ftp URLs\n/if $refresh;
	    warn qq/warning: -IfNewerThan ignored for ftp URLs\n/if defined($reference_file);

        }
    }
}

exit($EXIT_error) if $errors;


$SuccessfulCount = 0;
$NotModifiedCount = 0;

##
## Now do the real thing.
##
while (@ARGV) {
    $arg = shift(@ARGV);
    if ($arg =~ m/^-t(ime)?o(ut)?$/) {
	&timeout_arg(shift(@ARGV));
    } else {
	&fetch_url($arg);
    }
}

if ($NotModifiedCount) {
    exit($EXIT_notmodified);
} elsif ($SuccessfulCount) {
    exit($EXIT_ok);
} else {
    exit($EXIT_error);
}

###########################################################################
###########################################################################

sub timeout_arg
{
    ($TimeoutSpan) = @_;
			    $TimeoutSpan =~ s/s//;  
    $TimeoutSpan *=   60 if $TimeoutSpan =~ m/m/;
    $TimeoutSpan *= 3600 if $TimeoutSpan =~ m/h/;

}

##
## As a byproduct, returns the basename of $0.
##
sub show_version
{
    local($base) = $0;
    $base =~ s,.*/,,;
    print STDERR "This is $base version $version\n";
    $base;
}

##
## &usage(exitval, message);
##
## Prints a usage message to STDERR.
## If MESSAGE is defined, prints that first.
## If exitval is defined, exits with that value. Otherwise, returns.
##
sub usage
{
    local($exit, $message) = @_;

    print STDERR $message if defined $message;
    local($base) = &show_version;
    print STDERR <<INLINE_LITERAL_TEXT;
usage: $0 [options] URL ...
  Fetches and displays the named URL(s). Supports http and ftp.
  (if no protocol is given, a leading "http://" is normally used).

Options are from among:
  -V, -version    Print version information; exit.
  -p, -post       If URL looks like a form reply, does POST instead of GET.
  -pf, -postfile  Like -post, but takes everything after ? to be a filename.
  -q, -quiet      All non-essential informational messages are suppressed.
  -nf, -nofollow  Don't follow "this document has moved" replies.
  -nr, -noretry   Doesn't retry a failed hostname lookup (same as -retry 0)
  -r #, -retry #  Sets failed-hostname-lookup-retry to # (default $retry)
  -np, -noproxy   Uses no proxy, even if one defined for the protocol.
  -ns, -nostrip   The HTTP header, normally elided, is printed.
  -head           gets item header only (implies -ns)
  -nab, -file     Dumps output to file whose name taken from URL, minus path
  -nnab           Like -nab, but *also* dumps to stdout.
  -update         HTTP only. Like -nab, but only if the page has been modified.
  -h, -help       Prints this message.
  -IfNewerThan F  HTTP only. Only brings page if it is newer than named file.
  -timeout T      Fail if a connection can't be made in the specified time.

  -updateme       Pull the latest version of $base from
		    $WEB
                  and reports if it is newer than your current version.

Comments to $comments.
INLINE_LITERAL_TEXT

    exit($exit) if defined $exit;
}

##
## Pull the latest version of this program to a local file.
## Clip the first couple lines from this executing file so that we
## preserve the local invocation style.
##
sub updateme
{
    ##
    ## Open a temp file to hold the new version,
    ## redirecting STDOUT to it.
    ##
    open(STDOUT, '>'.($tempFile="/tmp/webget.new"))     ||
    open(STDOUT, '>'.($tempFile="/usr/tmp/webget.new")) ||
    open(STDOUT, '>'.($tempFile="/webget.new"))         ||
    open(STDOUT, '>'.($tempFile="webget.new"))          ||
	die "$0: can't open a temp file.\n";

    ##
    ## See if we can figure out how we were called.
    ## The seek will rewind not to the start of the data, but to the
    ## start of the whole program script.
    ## 
    ## Keep the first line if it begins with #!, and the next two if they
    ## look like the trick mentioned in the perl man page for getting
    ## around the lack of #!-support.
    ##
    if (seek(DATA, 0, 0)) { ## 
	$_ = <DATA>; if (m/^#!/) { print STDOUT;
	    $_ = <DATA>; if (m/^\s*eval/) { print STDOUT;
		$_ = <DATA>; if (m/^\s*if/) { print STDOUT; }
	    }
	}
	print STDOUT "\n#-\n";
    }

    ## Go get the latest one...
    local(@options);
    push(@options, 'head') if $head;
    push(@options, 'nofollow') unless $follow;
    push(@options, ('retry') x $retry) if $retry;
    push(@options, 'quiet') if $quiet;
    push(@options, 'debug') if $debug;
    local($status, $memo, %info) = &www'open_http_url(*IN, $WEB, @options);
    die "fetching $WEB:\n   $memo\n" unless $status eq 'ok';

    $size = $info{'content-length'};
    while (<IN>)
    {
	$size -= length;
	print STDOUT;
	if (!defined $fetched_version && m/version\s*=\s*"([^"]+)"/) {
	    $fetched_version = $1;
	    &general_read(*IN, $size);
	    last;
	}
    }
    
    $fetched_version = "<unknown>" unless defined $fetched_version;

    ##
    ## Try to update the mode of the temp file with the mode of this file.
    ## Don't worry if it fails.
    ##
    chmod($mode, $tempFile) if $mode = (stat($0))[2];

    $as_well = '';
    if ($fetched_version eq $version)
    {
	print STDERR "You already have the most-recent version ($version).\n",
		     qq/FWIW, the newly fetched one has been left in "$tempFile".\n/;
    }
    elsif ($fetched_version <= $version)
    {
	print STDERR
	    "Mmm, your current version seems newer (?!):\n",
	    qq/  your version: "$version"\n/,
	    qq/  new version:  "$fetched_version"\n/,
	    qq/FWIW, fetched one left in "$tempFile".\n/;
    }
    else
    {
	print STDERR
	    "Indeed, your current version was old:\n",
	    qq/  your version: "$version"\n/,
	    qq/  new version:  "$fetched_version"\n/,
	    qq/The file "$tempFile" is ready to replace the old one.\n/;
	print STDERR qq/Just do:\n  % mv $tempFile $0\n/ if -f $0;
	$as_well = ' as well';
    }
    print STDERR "Note that the libraries it uses may (or may not) need updating$as_well.\n"
	unless $inlined;
    exit($EXIT_ok);
}

##
## Given a list of URLs, fetch'em.
## Parses the URL and calls the routine for the appropriate protocol
##
sub fetch_url
{
    local(@todo) = @_;
    local(%circref, %hold_circref);

    URL_LOOP: while (@todo)
    {
	$URL = shift(@todo);
	%hold_circref = %circref; undef %circref;

	local($protocol, @args) = &www'grok_URL($URL, $noproxy);

	if (!defined $protocol) {
	    &www'message(1, qq/can't grok "$URL"/);
	    next URL_LOOP;
	}

	## call protocol-specific handler
	$func = "fetch_via_" . $protocol;
	$error = &$func(@args, $TimeoutSpan);
	if (defined $error) {
    	    &www'message(1, "$URL: $error");
	} else {
	    $SuccessfulCount++;
        }
    } 
}

sub filedate
{
   local($filename) = @_;
   local($filetime) = (stat($filename))[9];
   return 0 if !defined $filetime;
   local($sec, $min, $hour, $mday, $mon, $year, $wday) = gmtime($filetime);
   return 0 if !defined $wday;
   sprintf(qq/"%s, %02d-%s-%02d %02d:%02d:%02d GMT"/,
	("Sunday", "Monday", "Tuesdsy", "Wednesday",
         "Thursday", "Friday", "Saturday")[$wday],
	$mday,
	("Jan", "Feb", "Mar", "Apr", "May", "Jun",
         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec")[$mon],
	$year,
	$hour,
	$min,
	$sec);
}

sub local_filename
{
    local($filename) = @_;
    $filename =~ s,/+$,,;        ## remove any trailing slashes
    $filename =~ s,.*/,,;        ## remove any leading path
    if ($filename eq '') {
	## empty -- pick a random name
	$filename = "file0000";
	## look for a free random name.
	$filename++ while -f $filename;
    }
    $filename;
}

sub set_output_file
{
    local($filename) = @_;
    if (!open(OUT, ">$filename")) {
	&www'message(1, "$0: can't open [$filename] for output");
    } else {
	open(SAVEOUT, ">>&STDOUT") || die "$!";;
	open(STDOUT, ">>&OUT");
    }
}

sub close_output_file
{
    local($filename) = @_;
    unless ($quiet)
    {
	local($note) = qq/"$filename" written/;
	if (defined $error) {
	    $note .= " (possibly corrupt due to error above)";
	}
	&www'message(1, "$note.");
    }
    close(STDOUT);
    open(STDOUT, ">&SAVEOUT");
}

sub http_alarm
{
    &www'message(1, "ERROR: $AlarmNote.");
    exit($EXIT_timeout);  ## the alarm doesn't seem to cause a waiting syscall to break?
#   $HaveAlarm = 1;
}

##
## Given the host, port, and path, and (for info only) real target,
## fetch via HTTP.
##
## If there is a user and/or password, use that for Basic Authorization.
##
## If $timeout is nonzero, time out after that many seconds.
##
sub fetch_via_http
{
    local($host, $port, $path, $target, $user, $password, $timeout) = @_;
    local(@options);
    local($local_filename);

    ##
    ## If we're posting, but -postfile was given, we need to interpret
    ## the item in $path after '?' as a filename, and replace it with
    ## the contents of the file.
    ##
    if ($postfile && $path =~ s/\?([\d\D]*)//) {
	local($filename) = $1;
	return("can't open [$filename] to POST") if !open(IN, "<$filename");
	local($/) = ''; ## want to suck up the whole file.
	$path .= '?' . <IN>;
	close(IN);
    }

    $local_filename = &local_filename($path)
	if $refresh || $nab || defined($reference_file);
    $refresh = &filedate($local_filename) if $refresh;
    $refresh = &filedate($reference_file) if defined($reference_file);

    push(@options, 'head') if $head;
    push(@options, 'post') if $post;
    push(@options, 'nofollow') unless $follow;
    push(@options, ('retry') x 3);
    push(@options, 'quiet') if $quiet;
    push(@options, 'debug') if $debug;
    push(@options, "ifmodifiedsince=$refresh") if $refresh;

    if (defined $password || defined $user) {
	local($auth) = join(':', ($user || ''), ($password || ''));
	push(@options, "authorization=$auth");
    }

    local($old_alarm);
    if ($timeout) {
	$old_alarm = $SIG{'ALRM'} || 'DEFAULT';
	$SIG{'ALRM'} = "main'http_alarm";
#	$HaveAlarm = 0;
	$AlarmNote = "host $host";
	$AlarmNote .= ":$port" if $port != $www'default_port{'http'};
	$AlarmNote .= " timed out after $timeout second";
	$AlarmNote .= 's' if $timeout > 1;
	alarm($timeout);
    }
    local($result, $memo, %info) =
	&www'open_http_connection(*HTTP, $host,$port,$path,$target,@options);

    if ($timeout) {
	alarm(0);
	$SIG{'ALRM'} = $old_alarm;
    }

#    if ($HaveAlarm) {
#	close(HTTP);
#	$error = "timeout after $timeout second";
#	$error .= "s" if $timeout > 1;
#	return $error;
#    }

    if ($follow && ($result eq 'follow')) {
	%circref = %hold_circref;
	$circref{$memo} = 1;
	unshift(@todo, $memo);
	return undef;
    }


    return $memo if $result eq 'error';
    if (!$quiet && $result eq 'status' && ! -t STDOUT) {
	#&www'message(1, "Warning: $memo");
	$error = "Warning: $memo";
    }

    if ($info{'CODE'} == 304) { ## 304 is magic for "Not Modified"
	close(HTTP);
        &www'message(1, "$URL: Not Modified") unless $quiet;
	$NotModifiedCount++;
	return undef; ## no error
    }


    &set_output_file($local_filename) if $nab;

    unless($strip) {
        print         $info{'STATUS'}, "\n", $info{'HEADER'}, "\n";

        print SAVEOUT $info{'STATUS'}, "\n", $info{'HEADER'}, "\n" if $nab==2;
    }

    if (defined $info{'BODY'}) {
        print         $info{'BODY'};
	print SAVEOUT $info{'BODY'} if $nab==2;
    }

    if (!$head) {
	&general_read(*HTTP, $info{'content-length'});
    }
    close(HTTP);
    &close_output_file($local_filename) if $nab;

    $error; ## will be 'undef' if no error;
}

sub fetch_via_ftp
{
    local($host, $port, $path, $target, $user, $password, $timeout) = @_;
    local($local_filename) = &local_filename($path);
    local($ftp_debug) = $debug;
    local(@password) = ($password);
    $path =~ s,^/,,;  ## remove a leading / from the path.
    $path = '.' if $path eq ''; ## make sure we have something

    if (!defined $user) {
	$user = 'anonymous';
	$password = $ENV{'USER'} || 'WWWuser';
	@password = ($password.'@'. &network'addr_to_ascii(&network'my_addr),
		     $password.'@');
    } elsif (!defined $password) {
	@password = ("");
    }

    local($_last_ftp_reply, $_passive_host, $_passive_port);
    local($size);

    sub _ftp_get_reply
    {
	local($text) = scalar(<FTP_CONTROL>);
	die "lost connection to $host\n" if !defined $text;
	local($_, $tmp);
	print STDERR "READ: $text" if $ftp_debug;
	die "internal error: expected reply code in response from ".
	    "ftp server [$text]" unless $text =~ s/^(\d+)([- ])//;
	local($code) = $1;
	if ($2 eq '-') {
	    while (<FTP_CONTROL>) {
		($tmp = $_) =~ s/^\d+[- ]//;
		$text .= $tmp;
		last if m/^$code /;
	    }
	}
	$text =~ s/^\d+ ?/<foo>/g;
        ($code, $text);
    }

    sub _ftp_expect
    {
	local($code, $text) = &_ftp_get_reply;
	$_last_ftp_reply = $text;
	foreach $expect (@_) {
	    return ($code, $text) if $code == $expect;
	}
	die "internal error: expected return code ".
	    join('|',@_).", got [$text]";
    }

    sub _ftp_send
    {
	print STDERR "SEND: ", @_ if $ftp_debug;
	print FTP_CONTROL @_;
    }

    sub _ftp_do_passive
    {
	local(@commands) = @_;

	&_ftp_send("PASV\r\n");
	local($code) = &_ftp_expect(227, 125);

	if ($code == 227)
	{
	    die "internal error: can't grok passive reply [$_last_ftp_reply]"
		unless $_last_ftp_reply =~ m/\(([\d,]+)\)/;
	    local($a,$b,$c,$d, $p1, $p2) = split(/,/, $1);
	    ($_passive_host, $_passive_port) =
		("$a.$b.$c.$d", $p1*256 + $p2);
	}

	foreach(@commands) {
	    &_ftp_send($_);
	}

	local($error)=
	     &network'connect_to(*PASSIVE, $_passive_host, $_passive_port);
	die "internal error: passive ftp connect [$error]" if $error;
    }

    ## make the connection to the host
    &www'message($debug, "connecting to $host...") unless $quiet;

    local($old_alarm);
    if ($timeout) {
	$old_alarm = $SIG{'ALRM'} || 'DEFAULT';
	$SIG{'ALRM'} = "main'http_alarm"; ## can use this for now
#	$HaveAlarm = 0;
	$AlarmNote = "host $host";
	$AlarmNote .= ":$port" if $port != $www'default_port{'ftp'};
	$AlarmNote .= " timed out after $timeout second";
	$AlarmNote .= 's' if $timeout > 1;
	alarm($timeout);
    }

    local($error) = &network'connect_to(*FTP_CONTROL, $host, $port);

    if ($timeout) {
	alarm(0);
	$SIG{'ALRM'} = $old_alarm;
    }

    return $error if $error;

    local ($code, $text) = &_ftp_get_reply(*FTP_CONTROL);
    close(FTP_CONTROL), return "internal ftp error: [$text]" unless $code==220;

    ## log in
    &www'message($debug, "logging in as $user...") unless $quiet;
    foreach $password (@password)
    {
	&_ftp_send("USER $user\r\n");
	($code, $text) = &_ftp_expect(230,331,530);
	close(FTP_CONTROL), return $text if ($code == 530);
	last if $code == 230; ## hey, already logged in, cool.

	&_ftp_send("PASS $password\r\n");
	($code, $text) = &_ftp_expect(220,230,530,550,332);
	last if $code != 550;
	last if $text =~ m/can't change directory/;
    }

    if ($code == 550)
    {
	$text =~ s/\n+$//;
	&www'message(1, "Can't log in $host: $text") unless $quiet;
	exit($EXIT_error);
    }

    if ($code == 332)
    {
	 &_ftp_send("ACCT noaccount\r\n");
	 ($code, $text) = &_ftp_expect(230, 202, 530, 500,501,503, 421)
    }
    close(FTP_CONTROL), return $text if $code >= 300;

    &_ftp_send("TYPE I\r\n");
    &_ftp_expect(200);

    unless ($quiet) {
	local($name) = $path;
	$name =~ s,.*/([^/]),$1,;
        &www'message($debug, "requesting $name...");
    }
    ## get file
    &_ftp_do_passive("RETR $path\r\n");
    ($code,$text) = &_ftp_expect(125, 150, 550, 530);
    close(FTP_CONTROL), return $text if $code == 530;

    if ($code == 550)
    {
	close(PASSIVE);
	if ($text =~ /directory/i) {
	    ## probably from "no such file or directory", so just return now.
	    close(FTP_CONTROL);
	    return $text;
	}

	## do like Mosaic and try getting a directory listing.
	&_ftp_send("CWD $path\r\n");
	($code) = &_ftp_expect(250,550);
	if ($code == 550) {
	    close(FTP_CONTROL);
	    return $text;
	}
	&_ftp_do_passive("LIST\r\n");
	&_ftp_expect(125, 150);
    }

    $size = $1 if $text =~ m/(\d+)\s+bytes/;
    binmode(PASSIVE); ## just in case.
    &www'message($debug, "waiting for data...") unless $quiet;
    &set_output_file($local_filename) if $nab;
    &general_read(*PASSIVE, $size);
    &close_output_file($local_filename) if $nab;

    close(PASSIVE);
    close(FTP_CONTROL);
    undef;
}

sub general_read
{
    local(*INPUT, $size) = @_;
    local($lastcount, $bytes) = (0,0);
    local($need_to_clear) = 0;
    local($start_time) = time;
    local($last_time, $time) = $start_time;
    ## Figure out how often to print the "bytes read" message
    local($delta2print) =
	(defined $size) ? int($size/50) : $defaultdelta2print;

    &www'message(0, "read 0 bytes") unless $quiet;

    ## so $! below is set only if a real error happens from now
    eval 'local($^W) = 0; undef $!';
				

    while (defined($_ = <INPUT>))
    {
	## shove it out.
	&www'clear_message if $need_to_clear;
	print;
	print SAVEOUT if $nab==2;

	## if we know the content-size, keep track of what we're reading.
	$bytes += length;

	last if eof || (defined $size && $bytes >= $size);

	if (!$quiet && $bytes > ($lastcount + $delta2print))
	{
	    if ($time = time, $last_time == $time) {
		$delta2print *= 1.5;
	    } else {
		$last_time = $time;
		$lastcount = $bytes;
		local($time_delta) = $time - $start_time;
		local($text);

		$delta2print /= $time_delta;
		if (defined $size) {
		    $text = sprintf("read $bytes bytes (%.0f%%)",
				    $bytes*100/$size);
		} else {
		    $text = "read $bytes bytes";
		}

		if ($time_delta > 5 || ($time_delta && $bytes > 10240))
		{
		    local($rate) = int($bytes / $time_delta);
		    if ($rate < 5000) {
			$text .= " ($rate bytes/sec)";
		    } elsif ($rate < 1024 * 10) {
			$text .= sprintf(" (%.1f k/sec)", $rate/1024);
		    } else {
			$text .= sprintf(" (%.0f k/sec)", $rate/1024);
		    }
		}
		&www'message(0, "$text...");
		$need_to_clear = -t STDOUT;
	    }
	}
    }

    if (!$quiet)
    {
	if ($size && ($size != $bytes)) {
	   &www'message("WARNING: Expected $size bytes, read $bytes bytes.\n");
	}
# 	if ($!) {
# 	    print STDERR "\$! is [$!]\n";
# 	}
# 	if ($@) {
# 	    print STDERR "\$\@ is [$@]\n";
# 	}
    }
    &www'clear_message($text) unless $quiet;
}

sub dummy {
    1 || &dummy || &fetch_via_ftp || &fetch_via_http || &http_alarm;
    1 || close(OUT);
    1 || close(SAVEOUT);
}

__END__
