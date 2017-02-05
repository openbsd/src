package MakeMaker::Test::Setup::Unicode;

@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(setup_recurs teardown_recurs);

use strict;
use File::Path;
use File::Basename;
use MakeMaker::Test::Utils;
use utf8;
use Config;

my %Files = (
             'Problem-Module/Makefile.PL'   => <<'PL_END',
use ExtUtils::MakeMaker;
use utf8;

WriteMakefile(
    NAME          => 'Problem::Module',
    ABSTRACT_FROM => 'lib/Problem/Module.pm',
    AUTHOR        => q{Danijel Tašov},
    EXE_FILES     => [ qw(bin/probscript) ],
    INSTALLMAN1DIR => "some", # even if disabled in $Config{man1dir}
    MAN1EXT       => 1, # set to 0 if man pages disabled
);
PL_END

            'Problem-Module/lib/Problem/Module.pm'  => <<'pm_END',
use utf8;

=pod

=encoding utf8

=head1 NAME

Problem::Module - Danijel Tašov's great new module

=cut

1;
pm_END

            'Problem-Module/bin/probscript'  => <<'pl_END',
#!/usr/bin/perl
use utf8;

=encoding utf8

=head1 NAME

文档 - Problem script
pl_END
);


sub setup_recurs {
    while(my($file, $text) = each %Files) {
        # Convert to a relative, native file path.
        $file = File::Spec->catfile(File::Spec->curdir, split m{\/}, $file);

        my $dir = dirname($file);
        mkpath $dir;
        my $utf8 = ($] < 5.008 or !$Config{useperlio}) ? "" : ":utf8";
        open(FILE, ">$utf8", $file) || die "Can't create $file: $!";
        print FILE $text;
        close FILE;

        # ensure file at least 1 second old for makes that assume
        # files with the same time are out of date.
        my $time = calibrate_mtime();
        utime $time, $time - 1, $file;
    }

    return 1;
}

sub teardown_recurs {
    foreach my $file (keys %Files) {
        my $dir = dirname($file);
        if( -e $dir ) {
            rmtree($dir) || return;
        }
    }
    return 1;
}


1;
