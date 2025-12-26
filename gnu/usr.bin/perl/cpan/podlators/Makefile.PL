# Build instructions for podlators.
#
# We need to use ExtUtils::MakeMaker since this module is part of Perl core,
# which only supports that build method, and because it is a dependency of
# other build systems like Module::Build.
#
# Copyright 1999-2001, 2008, 2010, 2012, 2014-2016, 2018-2019, 2022, 2024
#     Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# SPDX-License-Identifier: GPL-1.0-or-later OR Artistic-1.0-Perl

# Do not use autodie here.  When podlators is built as part of Perl core, it
# is built before autodie is available.
use 5.012;
use warnings;

use Config;
use ExtUtils::MakeMaker;
use File::Spec;

# Determine the version of the distribution, since we need it outside of
# ExtUtils::MakeMaker to add some additional metadata. This is a very simple
# parser since it only has to handle the syntax my modules use.
#
# Returns: Distribution version as a string
sub dist_version {
    my ($path) = @_;
    open(my $fh, '<', $path) or die "$0: cannot open $path: $!\n";
    while (defined(my $line = <$fh>)) {
        if ($line =~ m{ \A package \s+ \S+ \s+ (v[\d.]+) }xms) {
            close($fh) or die "$0: cannot close $path: $!\n";
            return $1;
        }
    }
    close($fh) or die "$0: cannot close $path: $!\n";
    die "$0: cannot find version in lib/Pod/Man.pm\n";
}

# Generate full paths for scripts distributed in the bin directory.  Appends
# the .com extension to scripts on VMS, unless they already have the .PL
# extension.
#
# @scripts - List of script names
#
# Returns: List of relative paths from top of distribution
sub scripts {
    my (@scripts) = @_;
    my @paths = map { File::Spec->catfile('scripts', $_) } @scripts;
    if ($^O eq 'VMS') {
        @paths = map { m{ [.] PL \z }xms ? $_ : $_ . '.com' } @paths;
    }
    return @paths;
}

# Generate an association between a source file and a destination man page for
# non-module man pages.  ExtUtils::MakeMaker only really understands how to
# generate man pages for modules, so we need to help it for the script man
# pages and (particularly) the perlpodstyle man page.
#
# $directory - Directory containing the file
# $file      - File containing POD in that directory
#
# Returns: The path to the file with POD and the output man page, as a pair
sub man1pod {
    my ($directory, $file) = @_;

    # Build the base name of the file by stripping any *.pod or *.PL suffix.
    my $basename = $file;
    $basename =~ s{ [.] (?: pod | PL ) \z }{}xms;

    # Determine the output file name for the generated man page.
    my $outname = $basename . q{.} . $Config{man1ext};
    my $outpath = File::Spec->catfile(qw(blib man1), $outname);
    return (File::Spec->catfile($directory, $file), $outpath);
}

# The hash of all the metadata.  This will be modified before WriteMakefile to
# remove keys not supported by the local version of ExtUtils::MakeMaker.
my $dist_version = dist_version(File::Spec->catfile(qw(lib Pod Man.pm)));
my %metadata = (
    NAME             => 'Pod',
    DISTNAME         => 'podlators',
    ABSTRACT         => 'Convert POD data to various other formats',
    AUTHOR           => 'Russ Allbery <rra@cpan.org>',
    LICENSE          => 'perl_5',
    EXE_FILES        => [scripts('pod2text', 'pod2man')],
    VERSION          => $dist_version,
    MIN_PERL_VERSION => '5.012',

    # Use *.PL files to generate the driver scripts so that we get the correct
    # invocation of Perl on non-UNIX platforms.
    PL_FILES => {
        scripts('pod2man.PL', 'pod2man'), scripts('pod2text.PL', 'pod2text'),
    },

    # Override the files that generate section 1 man pages.
    MAN1PODS => {
        man1pod('scripts', 'pod2man.PL'),
        man1pod('scripts', 'pod2text.PL'),

        # Perl core uses a separate copy in the top-level pod directory.
        ($ENV{PERL_CORE} ? () : man1pod('pod', 'perlpodstyle.pod')),
    },

    # Clean some additional files.
    clean     => { FILES => File::Spec->catdir('t', 'tmp') },
    realclean => { FILES => join(q{ }, scripts('pod2text', 'pod2man')) },

    # Dependencies on other modules.
    PREREQ_PM => { 'Pod::Simple' => 3.26 },

    # Older versions of ExtUtils::MakeMaker don't pick up nested test
    # directories by default.
    test => { TESTS => 't/*/*.t' },

    # Additional metadata.
    META_ADD => {
        'meta-spec' => { version => 2 },
        provides    => {
            'Pod::Man' => {
                file    => 'lib/Pod/Man.pm',
                version => $dist_version,
            },
            'Pod::ParseLink' => {
                file    => 'lib/Pod/ParseLink.pm',
                version => $dist_version,
            },
            'Pod::Text' => {
                file    => 'lib/Pod/Text.pm',
                version => $dist_version,
            },
            'Pod::Text::Color' => {
                file    => 'lib/Pod/Text/Color.pm',
                version => $dist_version,
            },
            'Pod::Text::Overstrike' => {
                file    => 'lib/Pod/Text/Overstrike.pm',
                version => $dist_version,
            },
            'Pod::Text::Termcap' => {
                file    => 'lib/Pod/Text/Termcap.pm',
                version => $dist_version,
            },
        },
        resources => {
            bugtracker => {
                web => 'https://github.com/rra/podlators/issues',
            },
            homepage   => 'https://www.eyrie.org/~eagle/software/podlators/',
            repository => {
                url  => 'https://github.com/rra/podlators.git',
                web  => 'https://github.com/rra/podlators',
                type => 'git',
            },
        },
    },
);

# Remove keys that aren't supported by this version of ExtUtils::MakeMaker.
# This hash maps keys to the minimum supported version.
my %supported = (
    LICENSE          => 6.31,
    META_ADD         => 6.46,
    MIN_PERL_VERSION => 6.48,
);
for my $key (keys(%supported)) {
    if ($ExtUtils::MakeMaker::VERSION < $supported{$key}) {
        delete $metadata{$key};
    }
}

# Generate the actual Makefile.  Pick an arbitrary module to pull the version
# from, since they should all have the same version.
WriteMakefile(%metadata);
