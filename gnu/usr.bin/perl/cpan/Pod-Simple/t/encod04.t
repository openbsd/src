# The encoding detection heuristic will choose UTF8 or CP1252.  The current
# implementation will usually treat CP1252 (aka "Win-Latin-1") as CP1252 but
# can be fooled into seeing it as UTF8.

BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Test;
BEGIN {
    if ($] lt 5.007_003) {
        plan tests => 5, todo => [4, 5];   # Need utf8::decode() to pass #5
                                           # and isn't available in this
                                           # release
    }
    else {
        plan tests => 5, todo => [4];
    }
}

ok 1;

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;


# Initial, isolated, non-ASCII byte triggers CP1252 guess and later
# multi-byte sequence is not considered by heuristic.

my $x97;
my $x91;
my $dash;
if ($] ge 5.007_003) {
    $x97 = chr utf8::unicode_to_native(0x97);
    $x91 = chr utf8::unicode_to_native(0x91);
    $dash = '&#8212';
}
else {  # Tests will fail for early EBCDICs
    $x97 = chr 0x97;
    $x91 = chr 0x91;
    $dash = '--';
}

my @output_lines = split m/[\r\n]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

Em::Dash $x97 ${x91}CAF\xC9\x92

=cut

} );

my($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
if( $guess ) {
  if( $guess eq 'CP1252' ) {
    if( grep m{Dash $dash}, @output_lines ) {
      ok 1;
    } else {
      ok 0;
      print STDERR "# failed to find expected control character in output\n"
    }
  } else {
    ok 0;
    print STDERR "# parser guessed wrong encoding expected 'CP1252' got '$guess'\n";
  }
} else {
  ok 0;
  print STDERR "# parser failed to detect non-ASCII bytes in input\n";
}


# Initial smart-quote character triggers CP1252 guess as expected

@output_lines = split m/[\r\n]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

Smart::Quote - ${x91}FUT\xC9\x92

=cut

} );

if (ord("A") != 65) { # ASCII-platform dependent test skipped on this platform
    ok (1);
}
else {
    ($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
    if( $guess ) {
        if( $guess eq 'CP1252' ) {
            ok 1;
        } else {
            ok 0;
            print STDERR "# parser guessed wrong encoding expected 'CP1252' got '$guess'\n";
        }
    } else {
        ok 0;
        print STDERR "# parser failed to detect non-ASCII bytes in input\n";
    }
}


# Initial accented character followed by 'smart' apostrophe causes heuristic
# to choose UTF8 (a somewhat contrived example)

@output_lines = split m/[\r\n]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

=head2 JOS\xC9\x92S PLACE

=cut

} );

if (ord("A") != 65) { # ASCII-platform dependent test skipped on this platform
    ok (1);
}
else {
    ($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
    if( $guess ) {
        if( $guess eq 'CP1252' ) {
            ok 1;
        } else {
            ok 0;
            print STDERR "# parser guessed wrong encoding expected 'CP1252' got '$guess'\n";
        }
    } else {
        ok 0;
        print STDERR "# parser failed to detect non-ASCII bytes in input\n";
    }
}


# The previous example used a CP1252 byte sequence that also happened to be a
# valid UTF8 byte sequence.  In this example we use an illegal UTF-8 sequence
# (it needs a third byte), so must be 1252

@output_lines = split m/[\r\n]+/, Pod::Simple::XMLOutStream->_out( qq{

=head1 NAME

Smart::Apostrophe::Fail - L\xE9\x92Strange

=cut

} );

if (ord("A") != 65) { # ASCII-platform dependent test skipped on this platform
    ok (1);
}
else {
    ($guess) = "@output_lines" =~ m{Non-ASCII.*?Assuming ([\w-]+)};
    if( $guess ) {
        if( $guess eq 'CP1252' ) {
            ok 1;
        } else {
            ok 0;
            print STDERR "# parser guessed wrong encoding expected 'CP1252' got '$guess'\n";
        }
    } else {
        ok 0;
        print STDERR "# parser failed to detect non-ASCII bytes in input\n";
    }
}


exit;
