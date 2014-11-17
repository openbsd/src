#!/usr/bin/perl -w

BEGIN {
    unshift @INC, 't/lib/';
}

use File::Temp qw[tempdir];
my $tmpdir = tempdir( DIR => 't', CLEANUP => 1 );
chdir $tmpdir;

my $Is_VMS = $^O eq 'VMS';

use File::Spec;

use Test::More tests => 4;

my $dir = File::Spec->catdir("some", "dir");
my @cd_args = ($dir, "command1", "command2");

{
    package Test::MM_Win32;
    use ExtUtils::MM_Win32;
    @ISA = qw(ExtUtils::MM_Win32);

    my $mm = bless {}, 'Test::MM_Win32';

    {
        local *make = sub { "nmake" };

        my @dirs = (File::Spec->updir) x 2;
        my $expected_updir = File::Spec->catdir(@dirs);

        ::is $mm->cd(@cd_args),
qq{cd $dir
	command1
	command2
	cd $expected_updir};
    }

    {
        local *make = sub { "dmake" };

        ::is $mm->cd(@cd_args),
qq{cd $dir && command1
	cd $dir && command2};
    }
}

{
    is +ExtUtils::MM_Unix->cd(@cd_args),
qq{cd $dir && command1
	cd $dir && command2};
}

SKIP: {
    skip("VMS' cd requires vmspath which is only on VMS", 1) unless $Is_VMS;

    use ExtUtils::MM_VMS;
    is +ExtUtils::MM_VMS->cd(@cd_args),
q{startdir = F$Environment("Default")
	Set Default [.some.dir]
	command1
	command2
	Set Default 'startdir'};
}
