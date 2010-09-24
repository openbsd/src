#!/usr/bin/perl -wT

use strict;
use lib 't/lib';

use Test::More tests => 47;

use TAP::Parser;
use TAP::Parser::IteratorFactory;

my $STREAMED   = 'TAP::Parser';
my $ITER       = 'TAP::Parser::Iterator';
my $ITER_FH    = "${ITER}::Stream";
my $ITER_ARRAY = "${ITER}::Array";

my $factory = TAP::Parser::IteratorFactory->new;
my $stream  = $factory->make_iterator( \*DATA );
isa_ok $stream, 'TAP::Parser::Iterator';
my $parser = TAP::Parser->new( { stream => $stream } );
isa_ok $parser, 'TAP::Parser',
  '... and creating a streamed parser should succeed';

can_ok $parser, '_stream';
is ref $parser->_stream, $ITER_FH,
  '... and it should return the proper iterator';
can_ok $parser, 'next';
is $parser->next->as_string, '1..5',
  '... and the plan should parse correctly';
is $parser->next->as_string, 'ok 1 - input file opened',
  '... and the first test should parse correctly';
is $parser->next->as_string, '... this is junk',
  '... and junk should parse correctly';
is $parser->next->as_string,
  'not ok 2 first line of the input valid # TODO some data',
  '... and the second test should parse correctly';
is $parser->next->as_string, '# this is a comment',
  '... and comments should parse correctly';
is $parser->next->as_string, 'ok 3 - read the rest of the file',
  '... and the third test should parse correctly';
is $parser->next->as_string, 'not ok 4 - this is a real failure',
  '... and the fourth test should parse correctly';
is $parser->next->as_string, 'ok 5 # SKIP we have no description',
  '... and fifth test should parse correctly';

ok !$parser->parse_errors, '... and we should have no parse errors';

# plan at end

my $tap = <<'END_TAP';
ok 1 - input file opened
... this is junk
not ok first line of the input valid # todo some data
# this is a comment
ok 3 - read the rest of the file
not ok 4 - this is a real failure
ok 5 # skip we have no description
1..5
END_TAP

$stream = $factory->make_iterator( [ split /\n/ => $tap ] );
ok $parser = TAP::Parser->new( { stream => $stream } ),
  'Now we create a parser with the plan at the end';
isa_ok $parser->_stream, $ITER_ARRAY,
  '... and now we should have an array iterator';
is $parser->next->as_string, 'ok 1 - input file opened',
  '... and the first test should parse correctly';
is $parser->next->as_string, '... this is junk',
  '... and junk should parse correctly';
is $parser->next->as_string,
  'not ok 2 first line of the input valid # TODO some data',
  '... and the second test should parse correctly';
is $parser->next->as_string, '# this is a comment',
  '... and comments should parse correctly';
is $parser->next->as_string, 'ok 3 - read the rest of the file',
  '... and the third test should parse correctly';
is $parser->next->as_string, 'not ok 4 - this is a real failure',
  '... and the fourth test should parse correctly';
is $parser->next->as_string, 'ok 5 # SKIP we have no description',
  '... and fifth test should parse correctly';
is $parser->next->as_string, '1..5',
  '... and the plan should parse correctly';

ok !$parser->parse_errors, '... and we should have no parse errors';

# misplaced plan (and one-off errors)

$tap = <<'END_TAP';
ok 1 - input file opened
1..5
... this is junk
not ok first line of the input valid # todo some data
# this is a comment
ok 3 - read the rest of the file
not ok 4 - this is a real failure
ok 5 # skip we have no description
END_TAP

$stream = $factory->make_iterator( [ split /\n/ => $tap ] );

ok $parser = TAP::Parser->new( { stream => $stream } ),
  'Now we create a parser with a plan as the second line';
is $parser->next->as_string, 'ok 1 - input file opened',
  '... and the first test should parse correctly';
is $parser->next->as_string, '1..5',
  '... and the plan should parse correctly';
is $parser->next->as_string, '... this is junk',
  '... and junk should parse correctly';
is $parser->next->as_string,
  'not ok 2 first line of the input valid # TODO some data',
  '... and the second test should parse correctly';
is $parser->next->as_string, '# this is a comment',
  '... and comments should parse correctly';
is $parser->next->as_string, 'ok 3 - read the rest of the file',
  '... and the third test should parse correctly';
is $parser->next->as_string, 'not ok 4 - this is a real failure',
  '... and the fourth test should parse correctly';
is $parser->next->as_string, 'ok 5 # SKIP we have no description',
  '... and fifth test should parse correctly';

ok $parser->parse_errors, '... and we should have one parse error';
is + ( $parser->parse_errors )[0],
  'Plan (1..5) must be at the beginning or end of the TAP output',
  '... telling us that our plan went awry';

$tap = <<'END_TAP';
ok 1 - input file opened
... this is junk
not ok first line of the input valid # todo some data
# this is a comment
ok 3 - read the rest of the file
not ok 4 - this is a real failure
1..5
ok 5 # skip we have no description
END_TAP

$stream = $factory->make_iterator( [ split /\n/ => $tap ] );

ok $parser = TAP::Parser->new( { stream => $stream } ),
  'Now we create a parser with the plan as the second to last line';
is $parser->next->as_string, 'ok 1 - input file opened',
  '... and the first test should parse correctly';
is $parser->next->as_string, '... this is junk',
  '... and junk should parse correctly';
is $parser->next->as_string,
  'not ok 2 first line of the input valid # TODO some data',
  '... and the second test should parse correctly';
is $parser->next->as_string, '# this is a comment',
  '... and comments should parse correctly';
is $parser->next->as_string, 'ok 3 - read the rest of the file',
  '... and the third test should parse correctly';
is $parser->next->as_string, 'not ok 4 - this is a real failure',
  '... and the fourth test should parse correctly';
is $parser->next->as_string, '1..5',
  '... and the plan should parse correctly';
is $parser->next->as_string, 'ok 5 # SKIP we have no description',
  '... and fifth test should parse correctly';

ok $parser->parse_errors, '... and we should have one parse error';
is + ( $parser->parse_errors )[0],
  'Plan (1..5) must be at the beginning or end of the TAP output',
  '... telling us that our plan went awry';

__DATA__
1..5
ok 1 - input file opened
... this is junk
not ok first line of the input valid # todo some data
# this is a comment
ok 3 - read the rest of the file
not ok 4 - this is a real failure
ok 5 # skip we have no description
