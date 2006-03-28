#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use File::Find;
use File::Spec;
use Test::More;

my $Has_Test_Pod;
BEGIN {
    $Has_Test_Pod = eval 'use Test::Pod 0.95; 1';
}

chdir File::Spec->updir;
my $manifest = File::Spec->catfile('MANIFEST');
open(MANIFEST, $manifest) or die "Can't open $manifest: $!";
my @modules = map { m{^lib/(\S+)}; $1 } 
              grep { m{^lib/ExtUtils/\S*\.pm} } 
              grep { !m{/t/} } <MANIFEST>;
chomp @modules;
close MANIFEST;

chdir 'lib';
plan tests => scalar @modules * 2;
foreach my $file (@modules) {
    # 5.8.0 has a bug about require alone in an eval.  Thus the extra
    # statement.
    eval { require($file); 1 };
    is( $@, '', "require $file" );

    SKIP: {
        skip "Test::Pod not installed", 1 unless $Has_Test_Pod;
        pod_file_ok($file);
    }
}
