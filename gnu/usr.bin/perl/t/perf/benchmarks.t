#!./perl
#
# Execute the various code snippets in t/perf/benchmarks
# to ensure that they are all syntactically correct

BEGIN {
    chdir 't';
    require './test.pl';
    @INC = ('.', '../lib');
}

use warnings;
use strict;


my $file = 'perf/benchmarks';
my $benchmark_array = do $file;
die $@ if $@;
die "$! while trying to read '$file'" if $!;
die "'$file' did not return an array ref\n"
        unless ref $benchmark_array eq 'ARRAY';

die "Not an even number of key value pairs in '$file'\n"
        if @$benchmark_array % 2;

my %benchmarks;
while (@$benchmark_array) {
    my $key  = shift @$benchmark_array;
    my $hash = shift @$benchmark_array;
    die "Duplicate key '$key' in '$file'\n" if exists $benchmarks{$key};
    $benchmarks{$key} = $hash;
}

plan keys(%benchmarks) * 3;


# check the hash of hashes is minimally consistent in format

for my $token (sort keys %benchmarks) {
    like($token, qr/^[a-zA-Z](\w|::)+$/a, "legal token: $token");
    my $keys = join('-', sort keys %{$benchmarks{$token}});
    is($keys, 'code-desc-setup', "legal keys:  $token");
}

# check that each bit of code compiles and runs

for my $token (sort keys %benchmarks) {
    my $b = $benchmarks{$token};
    my $code = "package $token; $b->{setup}; for (1..1) { $b->{code} } 1;";
    no warnings;
    no strict;
    ok(eval $code, "running $token")
        or do {
            diag("code:");
            diag($code);
            diag("gave:");
            diag($@);
        }
}


