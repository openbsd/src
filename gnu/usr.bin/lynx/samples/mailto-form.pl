#! /usr/bin/perl -w
# Some scripts for handling mailto URLs within lynx via an interactive form
# 
# Warning: this is a quick demo, to show what kinds of things are possible
# by hooking some external commands into lynx.  Use at your own risk.
# 
# Requirements:
# 
# - Perl and CGI.pm.
# - A "sendmail" command for actually sending mail (if you need some
#   other interface, change the code below in sub sendit appropriately).
# - Lynx compiled with support for lynxcgi, that means EXEC_CGI must have
#   been defined at compilation, usually done with
#     ./configure --enable-cgi-links
# - Lynx must have support for CERN-style rules as of 2.8.3, which must
#   not have been disabled at compilation (it is enabled by default).
# 
# Instructions:
# (This is for people without lynxcgi experience; if you are already
# use lynxcgi, you don't have to follow everything literally, use
# common sense for picking appropriate file locations in your situation.)
# 
# - Make a subdirectory 'lynxcgi' under you home directory, i.e.
#      mkdir ~/lynxcgi
# - Put this three script file mailto-form.pl there and make it
#   executable.  For example,
#      cp mailto-form.pl ~/lynxcgi
#      chmod a+x ~/lynxcgi/mailto-form.pl
# - Edit mailto-form.pl (THIS FILE), there are some strings that
#   that need to be changed, see ### Configurable variables ###
#   below.
# - Allow lynx to execute lynxcgi files in that directory, for example,
#   put in your lynx.cfg file:
#      TRUSTED_LYNXCGI:<tab>/home/myhomedir/lynxcgi/mailto-form.pl
#   where <tab> is a real TAB character and you have to put the real
#   location of your directory in place of "myhomedir", of course.
#   The '~' abbreviation cannot be used.
#   You could also just enable execution of all lynxcgi scripts, by
#   not having any TRUSTED_LYNXCGI options in lynx.cfg at all, but
#   that can't be recommended.
# - Tell lynx to actually use the lynxcgi scripts for mailto URLs.
#   There are two variants:
#   a) Redirect "mailto"
#   Requires patched lynx, currently not yet in the developent code.
#   Use the following two lines in the file that is configured as
#   RULESFILE in lynxcfg:
#      PermitRedirection mailto:*
#      Redirect mailto:* lynxcgi:/home/myhomedir/lynxcgi/mailto-form.pl?from=myname@myhost&to=*
#   You can also put them directly in lynx.cfg, prefixing each with
#   "RULE:".  Replace ""myhomedir", "myname", and "myhost" with your
#   correct values, of course.
#   b) Redirect "xmailto"
#   Requires defining a fake proxy before starting lynx, like
#      export xmailto_proxy=dummy  # or for csh: setenv xmailto_proxy dummy
#   Requires that you change "mailto" to "xmailto" each time you want
#   to activate a mailto link.  This can be done conveniently with
#   a few keys: 'E', ^A, 'x', Enter.
#   Use the following two lines in the file that is configured as
#   RULESFILE in lynxcfg:
#      PermitRedirection xmailto:*
#      Redirect xmailto:* lynxcgi:/home/myhomedir/lynxcgi/mailto-form.pl?from=myname@myhost&to=*
#   You can also put them directly in lynx.cfg, prefixing each with
#   "RULE:".  Replace ""myhomedir", "myname", and "myhost" with your
#   correct values, of course.
# 
# Limitations:
# 
# - Only applies to mailto URLs that appear as links or are entered at
#   a 'g'oto prompt.  Does not apply to other ways of sending mail, like
#   the 'c' (COMMENT) key, mailto as a FORM action, or mailing a file
#   from the 'P'rinting Options screen.
# - Nothing is done for charset labelling, content-transfer-encoding
#   of non-ASCII characters, and other MIME niceties.
#
# Klaus Weide 20000712

########################################################################
########## Configurable variables ######################################

$SENDMAIL = '/usr/sbin/sendmail';
#                                   The location of your sendmail binary
$SELFURL = 'lynxcgi:/home/lynxdev/lynxcgi/mailto-form.pl';
#                                   Where this script lives in URL space
$SEND_TOKEN = '/vJhOp6eQ';
#                           When found in the PATH_INFO part of the URL,
#                           this causes the script to actually send mail
#                           by calling $SENDMAIL instead of just throwing
#                           up a form.  CHANGE IT!  And don't tell anyone!
#                           Treat it like a password.
#                           Must start with '/', probably should have only
#                           alphanumeric ASCII characters.

## Also, make sure the first line of this script points
## to your PERL binary

########## Nothing else to change - I hope #############################
########################################################################

use CGI;

$|=1;

### Upcase first character
##sub ucfirst {
##    s/^./\U$1/;
##}

# If there are mutiple occurrences of the same thing, how to join them
# into one string
%joiner = (from => ', ',
	   to => ', ',
	   cc => ', ',
	   subject => '; ',
	   body => "\n\n"
	   );
sub joiner {
    my ($key) = @_;
    if ($joiner{$key}) {
	$joiner{$key};
    } else {
	" ";
    }
}

# Here we check whether this script is called for actual sending, rather
# than form generation.  If so, all the rest is handled by sub sendit, below.
$pathinfo = $ENV{'PATH_INFO'}; 
if (defined($pathinfo) && $pathinfo eq $SEND_TOKEN) {
    $q = new CGI;
    print $q->header('text/plain');
    sendit();
    exit;
}

$method = $ENV{'REQUEST_METHOD'};
$querystring = $ENV{'QUERY_STRING'};
if ($querystring) {
    if ($method && $method eq "POST" && $ENV{'CONTENT_LENGTH'}) {
	$querystring =~ s/((^|\&)to=[^?&]*)\?/$1&/;
	$q0 = new CGI;
	$q = new CGI($querystring);
	@fields = $q0->param();
	foreach $key (@fields) {
	    @vals = $q0->param($key);
#	    print "Content-type: text/html\n\n";
#	    print "Appending $key to \$q...\n";
	    $q->append($key, @vals);
#	    print "<H2>Current Values in \$q0</H2>\n";
#	    print $q0->dump;
#	    print "<H2>Current Values in \$q</H2>\n";
#	    print $q->dump;

	}

    } else {
	$querystring =~ s/((^|\&)to=[^?&]*)\?/$1&/;
	$q = new CGI($querystring);
    }
} else {
    $q = new CGI;
}

print $q->header;

$long_title = $ENV{'QUERY_STRING'};
$long_title =~ s/^from=([^&]*)\&to=//;
$long_title = "someone" unless $long_title;
$long_title = "Compose mail for $long_title";
if (length($long_title) > 72) {
    $title = substr($long_title,0,72) . "...";
} else {
    $title = $long_title;
}
$long_title =~ s/&/&amp;/g;
$long_title =~ s/</&lt;/g;
print
    $q->start_html($title), "\n",
    $q->h1($long_title), "\n",
    $q->start_form(-method=>'POST', -action => $SELFURL . $SEND_TOKEN), "\n";

print "<TABLE>\n";
@fields = $q->param();
foreach $key (@fields) {
    @vals = $q->param($key);
    if (scalar(@vals) != 1) {
	print "multiple values " . scalar(@vals) ." for $key!\n";
	$q->param($key, join (joiner($key), @vals));
    }
}
foreach $key (@fields) {
    $_ = lc($key);
    if ($_ ne $key) {
	print "noncanonical case for $key!\n";
	$val=$q->param($key);
	$q->delete($key);
	if (!$q->param($_)) {
	    $q->param($_, $val);
	} else {
	    $q->param($_, $q->param($_) . joiner($_) . "$val");
	}
    }
}
foreach $key ('from', 'to', 'cc', 'subject') {
    print $q->Tr,
    $q->td(ucfirst($key) . ":"),
    $q->td($q->textfield(-name=>$key,
			 -size=>60,
			 -default=>$q->param($key))), "\n";
    $q->delete($key);
}

# Also pass on any unrecognized header fields that were specified.
# This may not be a good idea for general use!
# At least some dangerous header fields may have to be suppressed.
@keys = $q->param();
if (scalar(@keys) > (($q->param('body')) ? 1 : 0)) {
    print "<TR><TD colspan=2><EM>Additional headers:</EM>\n";
    foreach $key ($q->param()) {
	if ($key ne 'body') {
	    print $q->Tr,
	    $q->td(ucfirst($key) . ":"),
	    $q->td($q->textfield(-name=>$key,
				 -size=>60,
				 -default=>$q->param($key))), "\n";
	}
    }
}
print "</TABLE>\n";
print $q->textarea(-name=>'body',
		   -default=>$q->param('body')), "\n";
print "<PRE>\n\n</PRE>", "\n",
    $q->submit(-value=>"Send the message"), "\n",
    $q->endform, "\n";

print "\n";
exit;

# This is for header field values.
sub sanitize_field_value {
    my($val) = @_;
    $val =~ s/\0/./g;
    $val =~ s/\r\n/\n/g;
    $val =~ s/\r/\n/g;
    $val =~ s/\n*$//g;
    $val =~ s/\n+/\n/g;
    $val =~ s/\n(\S)/\n\t$1/g;
    $val;
}

sub sendit {
    open (MAIL, "| $SENDMAIL -t -oi -v") || die ("$0: Can't run sendmail: $!\n");
    @fields = $q->param();
    foreach $key (@fields) {
	@vals = $q->param($key);
	if (scalar(@vals) != 1) {
	    print "multiple values " . scalar(@vals) ." for $key!\n";
	    $q->param($key, join (joiner($key), @vals));
	}
    }
    foreach $key (@fields) {
	if ($key ne 'body') {
	    if ($key =~ /[^A-Za-z0-9_-]/) {
		print "$0: Ignoring malformed header field named '$key'!\n";
		next;
	    }
	    print MAIL ucfirst($key) . ": " .
		sanitize_field_value($q->param($key)) . "\n"
		or die ("$0: Feeding header to sendmail failed: $!\n");
	}
    }
    print MAIL "\n"
	or die ("$0: Ending header for sendmail failed: $!\n");
    print MAIL $q->param('body'), "\n"
	or die ("$0: Feeding body to sendmail failed: $!\n");
    close(MAIL)
	or warn $! ? "Error closing pipe to sendmail: $!"
	    : ($? & 127) ? ("Sendmail killed by signal " . ($? & 127) .
			    ($? & 127) ? ", core dumped" : "")
		: "Return value " . ($? >> 8) . " from sendmail";
}
