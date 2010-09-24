BEGIN {
    push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bXS\/APItest\b/) {
        print "1..0 # Skip: XS::APItest was not built\n";
        exit 0;
    }
}

use strict;
use warnings;

use Test::More tests => 3;

BEGIN { use_ok('XS::APItest') };

# I can't see a good way to easily get back perl-space diagnostics for these
# I hope that this isn't a problem.
if ($] > 5.009) {
  ok(sv_setsv_cow_hashkey_core,
     "With PERL_CORE sv_setsv does COW for shared hash key scalars");
} else {
  ok(!sv_setsv_cow_hashkey_core,
     "With PERL_CORE on 5.8.x sv_setsv doesn't COW for shared hash key scalars");
}

ok(!sv_setsv_cow_hashkey_notcore,
   "Without PERL_CORE sv_setsv doesn't COW for shared hash key scalars");
