#!/usr/local/bin/perl -w

use Test::More tests => 33;

END { ok $loaded; }
use CGI ( ':standard', '-no_debug', '*h3', 'start_table' );
$loaded = 1;
ok 1;

BEGIN {
    $| = 1;
    if ( $] > 5.006 ) {

        # no utf8
        require utf8;    # we contain Latin-1
        utf8->unimport;
    }
}

######################### End of black magic.

my $CRLF = "\015\012";
if ( $^O eq 'VMS' ) {
    $CRLF = "\n";        # via web server carriage is inserted automatically
}
if ( ord("\t") != 9 ) {    # EBCDIC?
    $CRLF = "\r\n";
}

# util
sub test {
    local ($^W) = 0;
    my ( undef, $true, $msg ) = @_;
    ok $true => $msg;
}

# all the automatic tags
is h1(), '<h1 />', "single tag";

is h1('fred'), '<h1>fred</h1>', "open/close tag";

is h1( 'fred', 'agnes', 'maura' ), '<h1>fred agnes maura</h1>',
  "open/close tag multiple";

is h1( { -align => 'CENTER' }, 'fred' ), '<h1 align="CENTER">fred</h1>',
  "open/close tag with attribute";

is h1( { -align => undef }, 'fred' ), '<h1 align>fred</h1>',
  "open/close tag with orphan attribute";

is h1( { -align => 'CENTER' }, [ 'fred', 'agnes' ] ),
  '<h1 align="CENTER">fred</h1> <h1 align="CENTER">agnes</h1>',
  "distributive tag with attribute";

{
    local $" = '-';

    is h1( 'fred', 'agnes', 'maura' ), '<h1>fred-agnes-maura</h1>',
      "open/close tag \$\" interpolation";

}

is header(), "Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}",
  "header()";

is header( -type => 'image/gif' ), "Content-Type: image/gif${CRLF}${CRLF}",
  "header()";

is header( -type => 'image/gif', -status => '500 Sucks' ),
  "Status: 500 Sucks${CRLF}Content-Type: image/gif${CRLF}${CRLF}", "header()";

like header( -nph => 1 ),
  qr!HTTP/1.0 200 OK${CRLF}Server: cmdline${CRLF}Date:.+${CRLF}Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}!,
  "header()";

is start_html(), <<END, "start_html()";
<!DOCTYPE html
	PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
	 "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en-US" xml:lang="en-US">
<head>
<title>Untitled Document</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1" />
</head>
<body>
END

is start_html( -Title => 'The world of foo' ), <<END, "start_html()";
<!DOCTYPE html
	PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
	 "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en-US" xml:lang="en-US">
<head>
<title>The world of foo</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1" />
</head>
<body>
END

for my $v (qw/ 2.0 3.2 4.0 4.01 /) {
    local $CGI::XHTML = 1;
    is
      start_html( -dtd => "-//IETF//DTD HTML $v//FR", -lang => 'fr' ),
      <<"END", 'start_html()';
<!DOCTYPE html
	PUBLIC "-//IETF//DTD HTML $v//FR">
<html lang="fr"><head><title>Untitled Document</title>
</head>
<body>
END
}

is
  start_html( -dtd => "-//IETF//DTD HTML 9.99//FR", -lang => 'fr' ),
  <<"END", 'start_html()';
<!DOCTYPE html
	PUBLIC "-//IETF//DTD HTML 9.99//FR">
<html xmlns="http://www.w3.org/1999/xhtml" lang="fr" xml:lang="fr">
<head>
<title>Untitled Document</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1" />
</head>
<body>
END

my $cookie =
  cookie( -name => 'fred', -value => [ 'chocolate', 'chip' ], -path => '/' );

is $cookie, 'fred=chocolate&chip; path=/', "cookie()";

my $h = header( -Cookie => $cookie );

like $h,
  qr!^Set-Cookie: fred=chocolate&chip\; path=/${CRLF}Date:.*${CRLF}Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}!s,
  "header(-cookie)";

is start_h3, '<h3>';

is end_h3, '</h3>';

is start_table( { -border => undef } ), '<table border>';
is h1( escapeHTML("this is <not> \x8bright\x9b") ),
  '<h1>this is &lt;not&gt; &#8249;right&#8250;</h1>';

charset('utf-8');

is h1( escapeHTML("this is <not> \x8bright\x9b") ),
  ord("\t") == 9
  ? '<h1>this is &lt;not&gt; ‹right›</h1>'
  : '<h1>this is &lt;not&gt; »rightº</h1>';

is i( p('hello there') ), '<i><p>hello there</p></i>';

my $q = CGI->new;
is $q->h1('hi'), '<h1>hi</h1>';

$q->autoEscape(1);

is $q->p( { title => "hello world&egrave;" }, 'hello &aacute;' ),
  '<p title="hello world&amp;egrave;">hello &aacute;</p>';

$q->autoEscape(0);

is $q->p( { title => "hello world&egrave;" }, 'hello &aacute;' ),
  '<p title="hello world&egrave;">hello &aacute;</p>';

is p( { title => "hello world&egrave;" }, 'hello &aacute;' ),
  '<p title="hello world&amp;egrave;">hello &aacute;</p>';

is header( -type => 'image/gif', -charset => 'UTF-8' ),
  "Content-Type: image/gif; charset=UTF-8${CRLF}${CRLF}", "header()";
