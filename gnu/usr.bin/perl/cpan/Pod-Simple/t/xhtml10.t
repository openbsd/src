#!/usr/bin/perl -w

# t/xhtml01.t - check basic output from Pod::Simple::XHTML

BEGIN {
    chdir 't' if -d 't';
}

use strict;
use lib '../lib';
use Test::More tests => 44;
#use Test::More 'no_plan';

use_ok('Pod::Simple::XHTML') or exit;

isa_ok my $parser = Pod::Simple::XHTML->new, 'Pod::Simple::XHTML';
my $header = $parser->html_header;
my $footer = $parser->html_footer;

for my $spec (
    [ 'foo'    => 'foo',   'foo'     ],
    [ '12foo'  => 'foo1',  'foo'     ],
    [ 'fo$bar' => 'fo-bar', 'fo-bar' ],
    [ 'f12'    => 'f12',    'f12'    ],
    [ '13'     => 'pod13',  'pod13'  ],
    [ '**.:'   => 'pod-.:', 'pod-.:' ],
) {
    is $parser->idify( $spec->[0] ), $spec->[1],
        qq{ID for "$spec->[0]" should be "$spec->[1]"};
    is $parser->idify( $spec->[0], 1 ), $spec->[2],
        qq{Non-unique ID for "$spec->[0]" should be "$spec->[2]"};
}

my $results;

initialize($parser, $results);
$parser->html_header($header);
$parser->html_footer($footer);
ok $parser->parse_string_document( '=head1 Foo' ), 'Parse one header';
is $results, <<'EOF', 'Should have the index';

<html>
<head>
<title></title>
<meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1" />
</head>
<body>


<ul id="index">
  <li><a href="#Foo">Foo</a></li>
</ul>

<h1 id="Foo">Foo</h1>

</body>
</html>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( '=head1 Foo Bar' ), 'Parse multiword header';
is $results, <<'EOF', 'Should have the index';
<ul id="index">
  <li><a href="#Foo-Bar">Foo Bar</a></li>
</ul>

<h1 id="Foo-Bar">Foo Bar</h1>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo B<Bar>\n\n=head1 Foo B<Baz>" ),
    'Parse two multiword headers';
is $results, <<'EOF', 'Should have the index';
<ul id="index">
  <li><a href="#Foo-Bar">Foo <b>Bar</b></a></li>
  <li><a href="#Foo-Baz">Foo <b>Baz</b></a></li>
</ul>

<h1 id="Foo-Bar">Foo <b>Bar</b></h1>

<h1 id="Foo-Baz">Foo <b>Baz</b></h1>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head1 Bar" ), 'Parse two headers';
is $results, <<'EOF', 'Should have both and the index';
<ul id="index">
  <li><a href="#Foo">Foo</a></li>
  <li><a href="#Bar">Bar</a></li>
</ul>

<h1 id="Foo">Foo</h1>

<h1 id="Bar">Bar</h1>

EOF
initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head1 Bar\n\n=head1 Baz" ),
    'Parse three headers';
is $results, <<'EOF', 'Should have all three and the index';
<ul id="index">
  <li><a href="#Foo">Foo</a></li>
  <li><a href="#Bar">Bar</a></li>
  <li><a href="#Baz">Baz</a></li>
</ul>

<h1 id="Foo">Foo</h1>

<h1 id="Bar">Bar</h1>

<h1 id="Baz">Baz</h1>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head2 Bar" ), 'Parse two levels';
is $results, <<'EOF', 'Should have the dual-level index';
<ul id="index">
  <li><a href="#Foo">Foo</a>
    <ul>
      <li><a href="#Bar">Bar</a></li>
    </ul>
  </li>
</ul>

<h1 id="Foo">Foo</h1>

<h2 id="Bar">Bar</h2>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head2 Bar\n\n=head3 Baz" ),
    'Parse three levels';
is $results, <<'EOF', 'Should have the three-level index';
<ul id="index">
  <li><a href="#Foo">Foo</a>
    <ul>
      <li><a href="#Bar">Bar</a>
        <ul>
          <li><a href="#Baz">Baz</a></li>
        </ul>
      </li>
    </ul>
  </li>
</ul>

<h1 id="Foo">Foo</h1>

<h2 id="Bar">Bar</h2>

<h3 id="Baz">Baz</h3>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head2 Bar\n\n=head3 Baz\n\n=head4 Howdy" ),
    'Parse four levels';
is $results, <<'EOF', 'Should have the four-level index';
<ul id="index">
  <li><a href="#Foo">Foo</a>
    <ul>
      <li><a href="#Bar">Bar</a>
        <ul>
          <li><a href="#Baz">Baz</a>
            <ul>
              <li><a href="#Howdy">Howdy</a></li>
            </ul>
          </li>
        </ul>
      </li>
    </ul>
  </li>
</ul>

<h1 id="Foo">Foo</h1>

<h2 id="Bar">Bar</h2>

<h3 id="Baz">Baz</h3>

<h4 id="Howdy">Howdy</h4>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head2 Bar\n\n=head2 Baz" ),
    'Parse 1/2';
is $results, <<'EOF', 'Should have the 1/s index';
<ul id="index">
  <li><a href="#Foo">Foo</a>
    <ul>
      <li><a href="#Bar">Bar</a></li>
      <li><a href="#Baz">Baz</a></li>
    </ul>
  </li>
</ul>

<h1 id="Foo">Foo</h1>

<h2 id="Bar">Bar</h2>

<h2 id="Baz">Baz</h2>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head3 Bar" ), 'Parse jump from one to three';
is $results, <<'EOF', 'Should have the 1-3 index';
<ul id="index">
  <li><a href="#Foo">Foo</a>
    <ul>
      <li>
        <ul>
          <li><a href="#Bar">Bar</a></li>
        </ul>
      </li>
    </ul>
  </li>
</ul>

<h1 id="Foo">Foo</h1>

<h3 id="Bar">Bar</h3>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head1 Foo\n\n=head4 Bar" ), 'Parse jump from one to four';
is $results, <<'EOF', 'Should have the 1-4 index';
<ul id="index">
  <li><a href="#Foo">Foo</a>
    <ul>
      <li>
        <ul>
          <li>
            <ul>
              <li><a href="#Bar">Bar</a></li>
            </ul>
          </li>
        </ul>
      </li>
    </ul>
  </li>
</ul>

<h1 id="Foo">Foo</h1>

<h4 id="Bar">Bar</h4>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head2 Foo\n\n=head1 Bar" ),
    'Parse two down to 1';
is $results, <<'EOF', 'Should have the 2-1 index';
<ul id="index">
  <li>
    <ul>
      <li><a href="#Foo">Foo</a></li>
    </ul>
  </li>
  <li><a href="#Bar">Bar</a></li>
</ul>

<h2 id="Foo">Foo</h2>

<h1 id="Bar">Bar</h1>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head2 Foo\n\n=head1 Bar\n\n=head4 Four\n\n=head4 Four2" ),
    'Parse two down to 1';
is $results, <<'EOF', 'Should have the 2-1 index';
<ul id="index">
  <li>
    <ul>
      <li><a href="#Foo">Foo</a></li>
    </ul>
  </li>
  <li><a href="#Bar">Bar</a>
    <ul>
      <li>
        <ul>
          <li>
            <ul>
              <li><a href="#Four">Four</a></li>
              <li><a href="#Four2">Four2</a></li>
            </ul>
          </li>
        </ul>
      </li>
    </ul>
  </li>
</ul>

<h2 id="Foo">Foo</h2>

<h1 id="Bar">Bar</h1>

<h4 id="Four">Four</h4>

<h4 id="Four2">Four2</h4>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( "=head4 Foo" ),
    'Parse just a four';
is $results, <<'EOF', 'Should have the 2-1 index';
<ul id="index">
  <li>
    <ul>
      <li>
        <ul>
          <li>
            <ul>
              <li><a href="#Foo">Foo</a></li>
            </ul>
          </li>
        </ul>
      </li>
    </ul>
  </li>
</ul>

<h4 id="Foo">Foo</h4>

EOF

initialize($parser, $results);
ok $parser->parse_string_document( <<'EOF' ), 'Parse a mixture';
=head2 Foo

=head3 Bar

=head1 Baz

=head4 Drink

=head3 Sip

=head4 Ouch

=head1 Drip
EOF

is $results, <<'EOF', 'And it should work!';
<ul id="index">
  <li>
    <ul>
      <li><a href="#Foo">Foo</a>
        <ul>
          <li><a href="#Bar">Bar</a></li>
        </ul>
      </li>
    </ul>
  </li>
  <li><a href="#Baz">Baz</a>
    <ul>
      <li>
        <ul>
          <li>
            <ul>
              <li><a href="#Drink">Drink</a></li>
            </ul>
          </li>
          <li><a href="#Sip">Sip</a>
            <ul>
              <li><a href="#Ouch">Ouch</a></li>
            </ul>
          </li>
        </ul>
      </li>
    </ul>
  </li>
  <li><a href="#Drip">Drip</a></li>
</ul>

<h2 id="Foo">Foo</h2>

<h3 id="Bar">Bar</h3>

<h1 id="Baz">Baz</h1>

<h4 id="Drink">Drink</h4>

<h3 id="Sip">Sip</h3>

<h4 id="Ouch">Ouch</h4>

<h1 id="Drip">Drip</h1>

EOF

sub initialize {
	$_[0] = Pod::Simple::XHTML->new;
        $_[0]->html_header('');
        $_[0]->html_footer('');
        $_[0]->index(1);
	$_[0]->output_string( \$results ); # Send the resulting output to a string
	$_[1] = '';
	return;
}
