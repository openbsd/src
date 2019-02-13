#!./perl

BEGIN {
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSys\/Hostname\b/) {
      print "1..0 # Skip: Sys::Hostname was not built\n";
      exit 0;
    }
}

use Sys::Hostname;

use Test::More tests => 4;

SKIP:
{
    eval {
        $host = hostname;
    };
    skip "No hostname available", 1
      if $@ =~ /Cannot get host name/;
    isnt($host, undef, "got a hostname");
}

{
    use warnings;
    my $warn;
    local $SIG{__WARN__} = sub { $warn = "@_" };
    eval { hostname("dummy") };
    ok($warn, "warns with an argument");
    like($warn, qr/hostname\(\) doesn't accept any arguments/,
         "appropriate message");
    no warnings "deprecated";
    undef $warn;
    eval { hostname("dummy") };
    is($warn, undef, "no warning when disabled");
}
