# The encoding detection heuristic will choose UTF8 or Latin-1.  The current
# implementation will usually treat CP1252 (aka "Win-Latin-1") as Latin-1 but
# can be fooled into seeing it as UTF8.
#
# Note 1: Neither guess is 'correct' since even if we choose Latin-1, all the
#         smart quote symbols will be rendered as control characters
#
# Note 2: the guess is only applied if the source POD omits =encoding, so
#         CP1252 source will render correctly if properly declared
#

BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Test;
BEGIN { plan tests => 5 };

ok 1;

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;


# Initial, isolated, non-ASCII byte triggers Latin-1 guess and later
# multi-byte sequence is not considered by heuristic.

my @output_lines = split m/[\cm\cj]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

Em::Dash \x97 \x91CAF\xC9\x92

=cut

} );

my($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
if( $guess ) {
  if( $guess eq 'ISO8859-1' ) {
    if( grep m{Dash (\x97|&#x97;|&#151;)}, @output_lines ) {
      ok 1;
    } else {
      ok 0;
      print "# failed to find expected control character in output\n"
    }
  } else {
    ok 0;
    print "# parser guessed wrong encoding expected 'ISO8859-1' got '$guess'\n";
  }
} else {
  ok 0;
  print "# parser failed to detect non-ASCII bytes in input\n";
}


# Initial smart-quote character triggers Latin-1 guess as expected

@output_lines = split m/[\cm\cj]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

Smart::Quote - \x91FUT\xC9\x92

=cut

} );

($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
if( $guess ) {
  if( $guess eq 'ISO8859-1' ) {
    ok 1;
  } else {
    ok 0;
    print "# parser guessed wrong encoding expected 'ISO8859-1' got '$guess'\n";
  }
} else {
  ok 0;
  print "# parser failed to detect non-ASCII bytes in input\n";
}


# Initial accented character followed by 'smart' apostrophe causes heuristic
# to choose UTF8 (a rather contrived example)

@output_lines = split m/[\cm\cj]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

Smart::Apostrophe::Fail - L\xC9\x92STRANGE

=cut

} );

($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
if( $guess ) {
  if( $guess eq 'UTF-8' ) {
    ok 1;
  } else {
    ok 0;
    print "# parser guessed wrong encoding expected 'UTF-8' got '$guess'\n";
  }
} else {
  ok 0;
  print "# parser failed to detect non-ASCII bytes in input\n";
}


# The previous example used a CP1252 byte sequence that also happened to be a
# valid UTF8 byte sequence.  In this example the heuristic also guesses 'wrong'
# despite the byte sequence not being valid UTF8 (it's too short).  This could
# arguably be 'fixed' by using a less naive regex.

@output_lines = split m/[\cm\cj]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

Smart::Apostrophe::Fail - L\xE9\x92Strange

=cut

} );

($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
if( $guess ) {
  if( $guess eq 'UTF-8' ) {
    ok 1;
  } else {
    ok 0;
    print "# parser guessed wrong encoding expected 'UTF-8' got '$guess'\n";
  }
} else {
  ok 0;
  print "# parser failed to detect non-ASCII bytes in input\n";
}


exit;
