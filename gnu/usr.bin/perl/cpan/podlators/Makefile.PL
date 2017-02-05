# Build instructions for podlators.
#
# We need to use ExtUtils::MakeMaker since this module is part of Perl core,
# which only supports that build method, and because it is a dependency of
# other build systems like Module::Build.
#
# Copyright 1999, 2000, 2001, 2008, 2010, 2012, 2014, 2015, 2016
#     Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use 5.006;
use strict;
use warnings;

use Config;
use ExtUtils::MakeMaker;
use File::Spec;

# Generate full paths for scripts distributed in the bin directory.  Appends
# the .com extension to scripts on VMS, unless they already have the .PL
# extension.
#
# @scripts - List of script names
#
# Returns: (Array) List of relative paths from top of distribution
#          (Scalar) Space-separated relative paths from top of distribution
sub scripts {
    my (@scripts) = @_;
    my @paths = map { File::Spec->catfile('scripts', $_) } @scripts;
    if ($^O eq 'VMS') {
        @paths = map { m{ [.] PL \z }xms ? $_ : $_ . '.com' } @paths;
    }
    return wantarray ? @paths : join(q{ }, @paths);
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
my %metadata = (
    NAME             => 'Pod',
    DISTNAME         => 'podlators',
    ABSTRACT         => 'Convert POD data to various other formats',
    AUTHOR           => 'Russ Allbery <rra@cpan.org>',
    LICENSE          => 'perl_5',
    EXE_FILES        => [scripts('pod2text', 'pod2man')],
    VERSION_FROM     => 'lib/Pod/Man.pm',
    MIN_PERL_VERSION => '5.006',

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
    clean     => { FILES => File::Spec->catdir('t',    'tmp') },
    realclean => { FILES => scalar(scripts('pod2text', 'pod2man')) },

    # Dependencies on other modules.
    PREREQ_PM => {
        'Encode'      => 0,
        'Pod::Simple' => 3.06,
    },

    # Older versions of ExtUtils::MakeMaker don't pick up nested test
    # directories by default.
    test => { TESTS => 't/*/*.t' },

    # For older versions of Perl, we have to force installation into the Perl
    # module directories since site modules did not take precedence over core
    # modules.
    INSTALLDIRS => $] lt '5.011' ? 'perl' : 'site',

    # Additional metadata.
    META_ADD => {
        'meta-spec' => { version => 2 },
        resources   => {
            bugtracker => {
                mailto => 'bug-podlators@rt.cpan.org',
                web => 'https://rt.cpan.org/Dist/Display.html?Name=podlators',
            },
            homepage   => 'http://www.eyrie.org/~eagle/software/podlators/',
            repository => {
                url  => 'git://github.com/rra/podlators.git',
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
