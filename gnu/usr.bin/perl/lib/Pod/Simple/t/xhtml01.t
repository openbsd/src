#!/usr/bin/perl -w

# t/xhtml01.t - check basic output from Pod::Simple::XHTML

BEGIN {
    chdir 't' if -d 't';
}

use strict;
use lib '../lib';
use Test::More tests => 26;

use_ok('Pod::Simple::XHTML') or exit;

my $parser = Pod::Simple::XHTML->new ();
isa_ok ($parser, 'Pod::Simple::XHTML');

my $results;

my $PERLDOC = "http://search.cpan.org/perldoc?";

initialize($parser, $results);
$parser->parse_string_document( "=head1 Poit!" );
is($results, "<h1>Poit!</h1>\n\n", "head1 level output");

initialize($parser, $results);
$parser->parse_string_document( "=head2 I think so Brain." );
is($results, "<h2>I think so Brain.</h2>\n\n", "head2 level output");

initialize($parser, $results);
$parser->parse_string_document( "=head3 I say, Brain..." );
is($results, "<h3>I say, Brain...</h3>\n\n", "head3 level output");

initialize($parser, $results);
$parser->parse_string_document( "=head4 Zort!" );
is($results, "<h4>Zort!</h4>\n\n", "head4 level output");


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

Gee, Brain, what do you want to do tonight?
EOPOD

is($results, <<'EOHTML', "simple paragraph");
<p>Gee, Brain, what do you want to do tonight?</p>

EOHTML


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

B: Now, Pinky, if by any chance you are captured during this mission,
remember you are Gunther Heindriksen from Appenzell. You moved to
Grindelwald to drive the cog train to Murren. Can you repeat that?

P: Mmmm, no, Brain, don't think I can.
EOPOD

is($results, <<'EOHTML', "multiple paragraphs");
<p>B: Now, Pinky, if by any chance you are captured during this mission, remember you are Gunther Heindriksen from Appenzell. You moved to Grindelwald to drive the cog train to Murren. Can you repeat that?</p>

<p>P: Mmmm, no, Brain, don't think I can.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=over

=item *

P: Gee, Brain, what do you want to do tonight?

=item *

B: The same thing we do every night, Pinky. Try to take over the world!

=back

EOPOD

is($results, <<'EOHTML', "simple bulleted list");
<ul>

<li>P: Gee, Brain, what do you want to do tonight?</li>

<li>B: The same thing we do every night, Pinky. Try to take over the world!</li>

</ul>

EOHTML


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=over

=item 1

P: Gee, Brain, what do you want to do tonight?

=item 2

B: The same thing we do every night, Pinky. Try to take over the world!

=back

EOPOD

is($results, <<'EOHTML', "numbered list");
<ol>

<li>1. P: Gee, Brain, what do you want to do tonight?</li>

<li>2. B: The same thing we do every night, Pinky. Try to take over the world!</li>

</ol>

EOHTML


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=over

=item Pinky

Gee, Brain, what do you want to do tonight?

=item Brain

The same thing we do every night, Pinky. Try to take over the world!

=back

EOPOD

is($results, <<'EOHTML', "list with text headings");
<ul>

<li>Pinky

<p>Gee, Brain, what do you want to do tonight?</p>

<li>Brain

<p>The same thing we do every night, Pinky. Try to take over the world!</p>

</ul>

EOHTML


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

  1 + 1 = 2;
  2 + 2 = 4;

EOPOD

is($results, <<'EOHTML', "code block");
<pre><code>  1 + 1 = 2;
  2 + 2 = 4;</code></pre>

EOHTML


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with a C<functionname>.
EOPOD
is($results, <<"EOHTML", "code entity in a paragraph");
<p>A plain paragraph with a <code>functionname</code>.</p>

EOHTML


initialize($parser, $results);
$parser->html_header("<html>\n<body>");
$parser->html_footer("</body>\n</html>");
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with body tags turned on.
EOPOD
is($results, <<"EOHTML", "adding html body tags");
<html>
<body>

<p>A plain paragraph with body tags turned on.</p>

</body>
</html>

EOHTML


initialize($parser, $results);
$parser->html_css('style.css');
$parser->html_header(undef);
$parser->html_footer(undef);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with body tags and css tags turned on.
EOPOD
like($results, qr/<link rel='stylesheet' href='style.css' type='text\/css'>/,
"adding html body tags and css tags");


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with S<non breaking text>.
EOPOD
is($results, <<"EOHTML", "Non breaking text in a paragraph");
<p>A plain paragraph with <nobr>non breaking text</nobr>.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with a L<Newlines>.
EOPOD
is($results, <<"EOHTML", "Link entity in a paragraph");
<p>A plain paragraph with a <a href="${PERLDOC}Newlines">Newlines</a>.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with a L<perlport/Newlines>.
EOPOD
is($results, <<"EOHTML", "Link entity in a paragraph");
<p>A plain paragraph with a <a href="${PERLDOC}perlport/Newlines">"Newlines" in perlport</a>.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with a L<Boo|http://link.included.here>.
EOPOD
is($results, <<"EOHTML", "A link in a paragraph");
<p>A plain paragraph with a <a href="http://link.included.here">Boo</a>.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with a L<http://link.included.here>.
EOPOD
is($results, <<"EOHTML", "A link in a paragraph");
<p>A plain paragraph with a <a href="http://link.included.here">http://link.included.here</a>.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with B<bold text>.
EOPOD
is($results, <<"EOHTML", "Bold text in a paragraph");
<p>A plain paragraph with <b>bold text</b>.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with I<italic text>.
EOPOD
is($results, <<"EOHTML", "Italic text in a paragraph");
<p>A plain paragraph with <i>italic text</i>.</p>

EOHTML

initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

A plain paragraph with a F<filename>.
EOPOD
is($results, <<"EOHTML", "File name in a paragraph");
<p>A plain paragraph with a <i>filename</i>.</p>

EOHTML


initialize($parser, $results);
$parser->parse_string_document(<<'EOPOD');
=pod

  # this header is very important & don't you forget it
  my $text = "File is: " . <FILE>;
EOPOD
is($results, <<"EOHTML", "Verbatim text with encodable entities");
<pre><code>  # this header is very important &amp; don&#39;t you forget it
  my \$text = &quot;File is: &quot; . &lt;FILE&gt;;</code></pre>

EOHTML

SKIP: for my $use_html_entities (0, 1) {
  if ($use_html_entities and not $Pod::Simple::XHTML::HAS_HTML_ENTITIES) {
    skip("HTML::Entities not installed", 1);
  }
  local $Pod::Simple::XHTML::HAS_HTML_ENTITIES = $use_html_entities;
  initialize($parser, $results);
  $parser->parse_string_document(<<'EOPOD');
=pod

  # this header is very important & don't you forget it
  B<my $file = <FILEE<gt> || 'Blank!';>
  my $text = "File is: " . <FILE>;
EOPOD
is($results, <<"EOHTML", "Verbatim text with markup and embedded formatting");
<pre><code>  # this header is very important &amp; don&#39;t you forget it
  <b>my \$file = &lt;FILE&gt; || &#39;Blank!&#39;;</b>
  my \$text = &quot;File is: &quot; . &lt;FILE&gt;;</code></pre>

EOHTML
}

######################################

sub initialize {
	$_[0] = Pod::Simple::XHTML->new ();
        $_[0]->html_header("");
        $_[0]->html_footer("");
	$_[0]->output_string( \$results ); # Send the resulting output to a string
	$_[1] = '';
	return;
}
