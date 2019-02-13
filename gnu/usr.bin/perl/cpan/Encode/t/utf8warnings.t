use strict;
use warnings;
BEGIN {
    if ($] < 5.014){
        print "1..0 # Skip: Perl 5.14.0 or later required\n";
        exit 0;
    }
}

use Encode;
use Test::More tests => 10;

my $valid   = "\x61\x00\x00\x00";
my $invalid = "\x78\x56\x34\x12";

my @warnings;
$SIG{__WARN__} = sub {push @warnings, "@_"};

my $enc = find_encoding("UTF32-LE");

{
    @warnings = ();
    my $ret = Encode::Unicode::decode( $enc, $valid );
    is("@warnings", "", "Calling decode in Encode::Unicode on valid string produces no warnings");
}



{
    @warnings = ();
    my $ret = Encode::Unicode::decode( $enc, $invalid );
    like("@warnings", qr/is not Unicode/, "Calling decode in Encode::Unicode on invalid string warns");
}

{
    no warnings 'utf8';
    @warnings = ();
    my $ret = Encode::Unicode::decode( $enc, $invalid );
    is("@warnings", "", "Warning from decode in Encode::Unicode can be silenced via no warnings 'utf8'");
}

{
    no warnings;
    @warnings = ();
    my $ret = Encode::Unicode::decode( $enc, $invalid );
    is("@warnings", "", "Warning from decode in Encode::Unicode can be silenced via no warnings");
}



{
    @warnings = ();
    my $ret = Encode::decode( $enc, $invalid );
    like("@warnings", qr/is not Unicode/, "Calling decode in Encode on invalid string warns");
}

{
    no warnings 'utf8';
    @warnings = ();
    my $ret = Encode::decode( $enc, $invalid );
    is("@warnings", "", "Warning from decode in Encode can be silenced via no warnings 'utf8'");
};

{
    no warnings;
    @warnings = ();
    my $ret = Encode::decode( $enc, $invalid );
    is("@warnings", "", "Warning from decode in Encode can be silenced via no warnings");
};



{
    @warnings = ();
    my $inplace = $invalid;
    Encode::from_to( $inplace, "UTF32-LE", "UTF-8" );
    like("@warnings", qr/is not Unicode/, "Calling from_to in Encode on invalid string warns");
}

{
    no warnings 'utf8';
    @warnings = ();
    my $inplace = $invalid;
    Encode::from_to( $inplace, "UTF32-LE", "UTF-8" );
    is("@warnings", "", "Warning from from_to in Encode can be silenced via no warnings 'utf8'");
};

{
    no warnings;
    @warnings = ();
    my $inplace = $invalid;
    Encode::from_to( $inplace, "UTF32-LE", "UTF-8" );
    is("@warnings", "", "Warning from from_to in Encode can be silenced via no warnings");
};
