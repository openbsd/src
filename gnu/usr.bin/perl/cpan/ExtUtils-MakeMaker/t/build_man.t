#!/usr/bin/perl -w

# Test if MakeMaker declines to build man pages under the right conditions.

BEGIN {
    unshift @INC, 't/lib';
}

use strict;
use Test::More tests => 10;

use File::Spec;
use File::Temp qw[tempdir];
use TieOut;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::BFD;

use ExtUtils::MakeMaker;
use ExtUtils::MakeMaker::Config;

# Simulate an installation which has man page generation turned off to
# ensure these tests will still work.
$Config{installman3dir} = 'none';

chdir 't';
perl_lib; # sets $ENV{PERL5LIB} relative to t/

my $tmpdir = tempdir( DIR => '../t', CLEANUP => 1 );
use Cwd; my $cwd = getcwd; END { chdir $cwd } # so File::Temp can cleanup
chdir $tmpdir;

ok( setup_recurs(), 'setup' );
END {
    ok chdir File::Spec->updir, 'chdir updir';
    ok teardown_recurs(), 'teardown';
}

ok( chdir 'Big-Dummy', "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $!");
my $README = 'README.pod';
{ open my $fh, '>', $README or die "$README: $!"; }

ok((my $stdout = tie *STDOUT, 'TieOut'), 'tie stdout');

{
    local $Config{installman3dir} = File::Spec->catdir(qw(t lib));
    my $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        VERSION_FROM    => 'lib/Big/Dummy.pm',
    );
    my %got = %{ $mm->{MAN3PODS} };
    # because value too OS-specific
    my $delete_key = $^O eq 'VMS' ? '[.lib.Big]Dummy.pm' : 'lib/Big/Dummy.pm';
    ok delete($got{$delete_key}), 'normal man3pod';
    is_deeply \%got, {}, 'no extra man3pod';
}

{
    my $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        VERSION_FROM    => 'lib/Big/Dummy.pm',
        INSTALLMAN3DIR  => 'none'
    );
    is_deeply $mm->{MAN3PODS}, {}, 'suppress man3pod with "none"';
}

{
    my $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        VERSION_FROM    => 'lib/Big/Dummy.pm',
        MAN3PODS        => {}
    );
    is_deeply $mm->{MAN3PODS}, {}, 'suppress man3pod with {}';
}

{
    my $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        VERSION_FROM    => 'lib/Big/Dummy.pm',
        MAN3PODS        => { "Foo.pm" => "Foo.1" }
    );
    is_deeply $mm->{MAN3PODS}, { "Foo.pm" => "Foo.1" }, 'override man3pod';
}
