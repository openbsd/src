# copied over from JSON::PP::XS and modified to use JSON::PP

BEGIN { $| = 1; print "1..4\n"; }
BEGIN { $ENV{PERL_JSON_BACKEND} = 0; }

use JSON::PP;

my $xs = JSON::PP->new->allow_nonref;

eval { $xs->decode ("[] ") };
print $@ ? "not " : "", "ok 1\n";
eval { $xs->decode ("[] x") };
print $@ ? "" : "not ", "ok 2\n";
print 2 == ($xs->decode_prefix ("[][]"))[1] ? "" : "not ", "ok 3\n";
print 3 == ($xs->decode_prefix ("[1] t"))[1] ? "" : "not ", "ok 4\n";

