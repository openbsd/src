#!/usr/bin/perl -Tw

use strict;
use Test::More tests => 3;

BEGIN {
    use_ok( 'Locale::Maketext' );
}

{
    package Whunk::L10N;
    use vars qw(@ISA %Lexicon);
    @ISA =  'Locale::Maketext';
    %Lexicon = ('hello' => 'SROBLR!');
}

{
    package Whunk::L10N::en;
    use vars qw(@ISA %Lexicon);
    @ISA =  'Whunk::L10N';
    %Lexicon = ('hello' => 'HI AND STUFF!');
}

{
    package Whunk::L10N::zh_tw;
    use vars qw(@ISA %Lexicon);
    @ISA =  'Whunk::L10N';
    %Lexicon = ('hello' => 'NIHAU JOE!');
}

$ENV{'REQUEST_METHOD'} = 'GET';
$ENV{'HTTP_ACCEPT_LANGUAGE'} = 'en-US, zh-TW';

my $x = Whunk::L10N->get_handle;
isa_ok( $x, 'Whunk::L10N::en' );
print "# LH object: $x\n";
is( $x->maketext('hello'), 'HI AND STUFF!' );
