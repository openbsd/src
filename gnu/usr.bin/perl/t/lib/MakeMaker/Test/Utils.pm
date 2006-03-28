package MakeMaker::Test::Utils;

use File::Spec;
use strict;
use Config;

use vars qw($VERSION @ISA @EXPORT);

require Exporter;
@ISA = qw(Exporter);

$VERSION = 0.03;

@EXPORT = qw(which_perl perl_lib makefile_name makefile_backup
             make make_run run make_macro calibrate_mtime
             setup_mm_test_root
	     have_compiler
            );

my $Is_VMS   = $^O eq 'VMS';
my $Is_MacOS = $^O eq 'MacOS';


=head1 NAME

MakeMaker::Test::Utils - Utility routines for testing MakeMaker

=head1 SYNOPSIS

  use MakeMaker::Test::Utils;

  my $perl     = which_perl;
  perl_lib;

  my $makefile      = makefile_name;
  my $makefile_back = makefile_backup;

  my $make          = make;
  my $make_run      = make_run;
  make_macro($make, $targ, %macros);

  my $mtime         = calibrate_mtime;

  my $out           = run($cmd);

  my $have_compiler = have_compiler();


=head1 DESCRIPTION

A consolidation of little utility functions used through out the
MakeMaker test suite.

=head2 Functions

The following are exported by default.

=over 4

=item B<which_perl>

  my $perl = which_perl;

Returns a path to perl which is safe to use in a command line, no
matter where you chdir to.

=cut

sub which_perl {
    my $perl = $^X;
    $perl ||= 'perl';

    # VMS should have 'perl' aliased properly
    return $perl if $Is_VMS;

    $perl .= $Config{exe_ext} unless $perl =~ m/$Config{exe_ext}$/i;

    my $perlpath = File::Spec->rel2abs( $perl );
    unless( $Is_MacOS || -x $perlpath ) {
        # $^X was probably 'perl'

        # When building in the core, *don't* go off and find
        # another perl
        die "Can't find a perl to use (\$^X=$^X), (\$perlpath=$perlpath)" 
          if $ENV{PERL_CORE};

        foreach my $path (File::Spec->path) {
            $perlpath = File::Spec->catfile($path, $perl);
            last if -x $perlpath;
        }
    }

    return $perlpath;
}

=item B<perl_lib>

  perl_lib;

Sets up environment variables so perl can find its libraries.

=cut

my $old5lib = $ENV{PERL5LIB};
my $had5lib = exists $ENV{PERL5LIB};
sub perl_lib {
                               # perl-src/t/
    my $lib =  $ENV{PERL_CORE} ? qq{../lib}
                               # ExtUtils-MakeMaker/t/
                               : qq{../blib/lib};
    $lib = File::Spec->rel2abs($lib);
    my @libs = ($lib);
    push @libs, $ENV{PERL5LIB} if exists $ENV{PERL5LIB};
    $ENV{PERL5LIB} = join($Config{path_sep}, @libs);
    unshift @INC, $lib;
}

END { 
    if( $had5lib ) {
        $ENV{PERL5LIB} = $old5lib;
    }
    else {
        delete $ENV{PERL5LIB};
    }
}


=item B<makefile_name>

  my $makefile = makefile_name;

MakeMaker doesn't always generate 'Makefile'.  It returns what it
should generate.

=cut

sub makefile_name {
    return $Is_VMS ? 'Descrip.MMS' : 'Makefile';
}   

=item B<makefile_backup>

  my $makefile_old = makefile_backup;

Returns the name MakeMaker will use for a backup of the current
Makefile.

=cut

sub makefile_backup {
    my $makefile = makefile_name;
    return $Is_VMS ? "$makefile".'_old' : "$makefile.old";
}

=item B<make>

  my $make = make;

Returns a good guess at the make to run.

=cut

sub make {
    my $make = $Config{make};
    $make = $ENV{MAKE} if exists $ENV{MAKE};

    return $make;
}

=item B<make_run>

  my $make_run = make_run;

Returns the make to run as with make() plus any necessary switches.

=cut

sub make_run {
    my $make = make;
    $make .= ' -nologo' if $make eq 'nmake';

    return $make;
}

=item B<make_macro>

    my $make_cmd = make_macro($make, $target, %macros);

Returns the command necessary to run $make on the given $target using
the given %macros.

  my $make_test_verbose = make_macro(make_run(), 'test', 
                                     TEST_VERBOSE => 1);

This is important because VMS's make utilities have a completely
different calling convention than Unix or Windows.

%macros is actually a list of tuples, so the order will be preserved.

=cut

sub make_macro {
    my($make, $target) = (shift, shift);

    my $is_mms = $make =~ /^MM(K|S)/i;

    my $cmd = $make;
    my $macros = '';
    while( my($key,$val) = splice(@_, 0, 2) ) {
        if( $is_mms ) {
            $macros .= qq{/macro="$key=$val"};
        }
        else {
            $macros .= qq{ $key=$val};
        }
    }

    return $is_mms ? "$make$macros $target" : "$make $target $macros";
}

=item B<calibrate_mtime>

  my $mtime = calibrate_mtime;

When building on NFS, file modification times can often lose touch
with reality.  This returns the mtime of a file which has just been
touched.

=cut

sub calibrate_mtime {
    open(FILE, ">calibrate_mtime.tmp") || die $!;
    print FILE "foo";
    close FILE;
    my($mtime) = (stat('calibrate_mtime.tmp'))[9];
    unlink 'calibrate_mtime.tmp';
    return $mtime;
}

=item B<run>

  my $out = run($command);
  my @out = run($command);

Runs the given $command as an external program returning at least STDOUT
as $out.  If possible it will return STDOUT and STDERR combined as you
would expect to see on a screen.

=cut

sub run {
    my $cmd = shift;

    require ExtUtils::MM;

    # Unix can handle 2>&1 and OS/2 from 5.005_54 up.
    # This makes our failure diagnostics nicer to read.
    if( MM->os_flavor_is('Unix') or
        ($] > 5.00554 and MM->os_flavor_is('OS/2'))
      ) {
        return `$cmd 2>&1`;
    }
    else {
        return `$cmd`;
    }
}

=item B<setup_mm_test_root>

Creates a rooted logical to avoid the 8-level limit on older VMS systems.  
No action taken on non-VMS systems.

=cut

sub setup_mm_test_root {
    if( $Is_VMS ) {
        # On older systems we might exceed the 8-level directory depth limit
        # imposed by RMS.  We get around this with a rooted logical, but we
        # can't create logical names with attributes in Perl, so we do it
        # in a DCL subprocess and put it in the job table so the parent sees it.
        open( MMTMP, '>mmtesttmp.com' ) || 
          die "Error creating command file; $!";
        print MMTMP <<'COMMAND';
$ MM_TEST_ROOT = F$PARSE("SYS$DISK:[-]",,,,"NO_CONCEAL")-".][000000"-"]["-"].;"+".]"
$ DEFINE/JOB/NOLOG/TRANSLATION=CONCEALED MM_TEST_ROOT 'MM_TEST_ROOT'
COMMAND
        close MMTMP;

        system '@mmtesttmp.com';
        1 while unlink 'mmtesttmp.com';
    }
}

=item have_compiler

  $have_compiler = have_compiler;

Returns true if there is a compiler available for XS builds.

=cut

sub have_compiler {
    my $have_compiler = 0;

    # ExtUtils::CBuilder prints its compilation lines to the screen.
    # Shut it up.
    require TieOut;
    local *STDOUT = *STDOUT;
    local *STDERR = *STDERR;

    tie *STDOUT, 'TieOut';
    tie *STDERR, 'TieOut';

    eval {
	require ExtUtils::CBuilder;
	my $cb = ExtUtils::CBuilder->new;

	$have_compiler = $cb->have_compiler;
    };

    return $have_compiler;
}


=back

=head1 AUTHOR

Michael G Schwern <schwern@pobox.com>

=cut

1;
