BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Test;
BEGIN { plan tests => 13 };

#use Pod::Simple::Debug (6);

ok 1;

use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";
my $x = 'Pod::Simple::XMLOutStream';
sub e ($$) { $x->_duo(@_) }

$Pod::Simple::XMLOutStream::ATTR_PAD   = ' ';
$Pod::Simple::XMLOutStream::SORT_ATTRS = 1; # for predictably testable output


print "# S as such...\n";

ok( $x->_out("=pod\n\nI like S<bric-a-brac>.\n"),
 =>  '<Document><Para>I like <S>bric-a-brac</S>.</Para></Document>' );
ok( $x->_out("=pod\n\nI like S<bric-a-brac a gogo >.\n"),
 =>  '<Document><Para>I like <S>bric-a-brac a gogo </S>.</Para></Document>' );
ok( $x->_out("=pod\n\nI like S<< bric-a-brac a gogo >>.\n"),
 =>  '<Document><Para>I like <S>bric-a-brac a gogo</S>.</Para></Document>' );

my $unless_ascii = (chr(65) eq 'A') ? '' :
 "Skip because not in ASCIIland";

skip( $unless_ascii,
    $x->_out( sub { $_[0]->nbsp_for_S(1) },
    "=pod\n\nI like S<bric-a-brac a gogo>.\n"),
'<Document><Para>I like bric-a-brac&#160;a&#160;gogo.</Para></Document>'
);
skip( $unless_ascii,
    $x->_out( sub { $_[0]->nbsp_for_S(1) },
    qq{=pod\n\nI like S<L</"bric-a-brac a gogo">>.\n}),
'<Document><Para>I like <L content-implicit="yes" section="bric-a-brac a gogo" type="pod">&#34;bric-a-brac&#160;a&#160;gogo&#34;</L>.</Para></Document>'
);
skip( $unless_ascii,
    $x->_out( sub { $_[0]->nbsp_for_S(1) },
    qq{=pod\n\nI like S<L<Stuff like that|/"bric-a-brac a gogo">>.\n}),
'<Document><Para>I like <L section="bric-a-brac a gogo" type="pod">Stuff&#160;like&#160;that</L>.</Para></Document>'
);
skip( $unless_ascii,
    $x->_out( sub { $_[0]->nbsp_for_S(1) },
    qq{=pod\n\nI like S<L<Stuff I<like that>|/"bric-a-brac a gogo">>.\n}),
'<Document><Para>I like <L section="bric-a-brac a gogo" type="pod">Stuff&#160;<I>like&#160;that</I></L>.</Para></Document>'
);

&ok( $x->_duo( sub { $_[0]->nbsp_for_S(1) },
  "=pod\n\nI like S<bric-a-brac a gogo>.\n",
  "=pod\n\nI like bric-a-bracE<160>aE<160>gogo.\n",
));
&ok(
  map {my $z = $_; $z =~ s/content-implicit="yes" //g; $z }
  $x->_duo( sub { $_[0]->nbsp_for_S(1) },
    qq{=pod\n\nI like S<L</"bric-a-brac a gogo">>.\n},
    qq{=pod\n\nI like L<"bric-a-bracE<160>aE<160>gogo"|/"bric-a-brac a gogo">.\n},
));
&ok( $x->_duo( sub { $_[0]->nbsp_for_S(1) },
    qq{=pod\n\nI like S<L<Stuff like that|"bric-a-brac a gogo">>.\n},
    qq{=pod\n\nI like L<StuffE<160>likeE<160>that|"bric-a-brac a gogo">.\n},
));
&ok(
  map {my $z = $_; $z =~ s/content-implicit="yes" //g; $z }
  $x->_duo( sub { $_[0]->nbsp_for_S(1) },
    qq{=pod\n\nI like S<L<Stuff I<like that>|"bric-a-brac a gogo">>.\n},
    qq{=pod\n\nI like L<StuffE<160>I<likeE<160>that>|"bric-a-brac a gogo">.\n},
));



print "# Wrapping up... one for the road...\n";
ok 1;
print "# --- Done with ", __FILE__, " --- \n";

