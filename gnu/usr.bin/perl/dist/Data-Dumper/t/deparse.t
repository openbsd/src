#!./perl -w
# t/deparse.t - Test Deparse()

BEGIN {
    if ($ENV{PERL_CORE}){
        require Config; import Config;
        no warnings 'once';
        if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
            print "1..0 # Skip: Data::Dumper was not built\n";
            exit 0;
        }
    }
}

use strict;

use Data::Dumper;
use Test::More tests =>  8;
use lib qw( ./t/lib );
use Testing qw( _dumptostr );

# Thanks to Arthur Axel "fREW" Schmidt:
# http://search.cpan.org/~frew/Data-Dumper-Concise-2.020/lib/Data/Dumper/Concise.pm

note("\$Data::Dumper::Deparse and Deparse()");

{
    my ($obj, %dumps, $deparse, $starting);
    use strict;
    my $struct = { foo => "bar\nbaz", quux => sub { "fleem" } };
    $obj = Data::Dumper->new( [ $struct ] );
    $dumps{'noprev'} = _dumptostr($obj);

    $starting = $Data::Dumper::Deparse;
    local $Data::Dumper::Deparse = 0;
    $obj = Data::Dumper->new( [ $struct ] );
    $dumps{'dddzero'} = _dumptostr($obj);
    local $Data::Dumper::Deparse = $starting;

    $obj = Data::Dumper->new( [ $struct ] );
    $obj->Deparse();
    $dumps{'objempty'} = _dumptostr($obj);

    $obj = Data::Dumper->new( [ $struct ] );
    $obj->Deparse(0);
    $dumps{'objzero'} = _dumptostr($obj);

    is($dumps{'noprev'}, $dumps{'dddzero'},
        "No previous setting and \$Data::Dumper::Deparse = 0 are equivalent");
    is($dumps{'noprev'}, $dumps{'objempty'},
        "No previous setting and Deparse() are equivalent");
    is($dumps{'noprev'}, $dumps{'objzero'},
        "No previous setting and Deparse(0) are equivalent");

    local $Data::Dumper::Deparse = 1;
    $obj = Data::Dumper->new( [ $struct ] );
    $dumps{'dddtrue'} = _dumptostr($obj);
    local $Data::Dumper::Deparse = $starting;

    $obj = Data::Dumper->new( [ $struct ] );
    $obj->Deparse(1);
    $dumps{'objone'} = _dumptostr($obj);

    is($dumps{'dddtrue'}, $dumps{'objone'},
        "\$Data::Dumper::Deparse = 1 and Deparse(1) are equivalent");

    isnt($dumps{'dddzero'}, $dumps{'dddtrue'},
        "\$Data::Dumper::Deparse = 0 differs from \$Data::Dumper::Deparse = 1");

    like($dumps{'dddzero'},
        qr/quux.*?sub.*?DUMMY/s,
        "\$Data::Dumper::Deparse = 0 reports DUMMY instead of deparsing coderef");
    unlike($dumps{'dddtrue'},
        qr/quux.*?sub.*?DUMMY/s,
        "\$Data::Dumper::Deparse = 1 does not report DUMMY");
    like($dumps{'dddtrue'},
        qr/quux.*?sub.*?use\sstrict.*?fleem/s,
        "\$Data::Dumper::Deparse = 1 deparses coderef");
}

