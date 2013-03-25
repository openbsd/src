package DistGen;

use strict;

use vars qw( $VERSION $VERBOSE @EXPORT_OK);

$VERSION = '0.01';
$VERBOSE = 0;

use Carp;

use MBTest ();
use Cwd ();
use File::Basename ();
use File::Find ();
use File::Path ();
use File::Spec ();
use IO::File ();
use Tie::CPHash;
use Data::Dumper;

my $vms_mode;
my $vms_lower_case;

BEGIN {
  $vms_mode = 0;
  $vms_lower_case = 0;
  if( $^O eq 'VMS' ) {
    # For things like vmsify()
    require VMS::Filespec;
    VMS::Filespec->import;
    $vms_mode = 1;
    $vms_lower_case = 1;
    my $vms_efs_case = 0;
    my $unix_rpt = 0;
    if (eval { local $SIG{__DIE__}; require VMS::Feature; }) {
        $unix_rpt = VMS::Feature::current("filename_unix_report");
        $vms_efs_case = VMS::Feature::current("efs_case_preserve");
    } else {
        my $env_unix_rpt = $ENV{'DECC$FILENAME_UNIX_REPORT'} || '';
        $unix_rpt = $env_unix_rpt =~ /^[ET1]/i;
        my $efs_case = $ENV{'DECC$EFS_CASE_PRESERVE'} || '';
        $vms_efs_case = $efs_case =~ /^[ET1]/i;
    }
    $vms_mode = 0 if $unix_rpt;
    $vms_lower_case = 0 if $vms_efs_case;
  }
}
BEGIN {
  require Exporter;
  *{import} = \&Exporter::import;
  @EXPORT_OK = qw(
    undent
  );
}

sub undent {
  my ($string) = @_;

  my ($space) = $string =~ m/^(\s+)/;
  $string =~ s/^$space//gm;

  return($string);
}

sub chdir_all ($) {
  # OS/2 has "current directory per disk", undeletable;
  # doing chdir() to another disk won't change cur-dir of initial disk...
  chdir('/') if $^O eq 'os2';
  chdir shift;
}

########################################################################

END { chdir_all(MBTest->original_cwd); }

sub new {
  my $self = bless {}, shift;
  $self->reset(@_);
}

sub reset {
  my $self = shift;
  my %options = @_;

  $options{name} ||= 'Simple';
  $options{dir} = File::Spec->rel2abs(
    defined $options{dir} ? $options{dir} : MBTest->tmpdir
  );

  my %data = (
    no_manifest   => 0,
    xs            => 0,
    inc           => 0,
    %options,
  );
  %$self = %data;

  tie %{$self->{filedata}}, 'Tie::CPHash';

  tie %{$self->{pending}{change}}, 'Tie::CPHash';

  # start with a fresh, empty directory
  if ( -d $self->dirname ) {
    warn "Warning: Removing existing directory '@{[$self->dirname]}'\n";
    File::Path::rmtree( $self->dirname );
  }
  File::Path::mkpath( $self->dirname );

  $self->_gen_default_filedata();

  return $self;
}

sub remove {
  my $self = shift;
  $self->chdir_original if($self->did_chdir);
  File::Path::rmtree( $self->dirname );
  return $self;
}

sub revert {
  my ($self, $file) = @_;
  if ( defined $file ) {
    delete $self->{filedata}{$file};
    delete $self->{pending}{$_}{$file} for qw/change remove/;
  }
  else {
    delete $self->{filedata}{$_} for keys %{ $self->{filedata} };
    for my $pend ( qw/change remove/ ) {
      delete $self->{pending}{$pend}{$_} for keys %{ $self->{pending}{$pend} };
    }
  }
  $self->_gen_default_filedata;
}

sub _gen_default_filedata {
  my $self = shift;

  # TODO maybe a public method like this (but with a better name?)
  my $add_unless = sub {
    my $self = shift;
    my ($member, $data) = @_;
    $self->add_file($member, $data) unless($self->{filedata}{$member});
  };

  if ( ! $self->{inc} ) {
    $self->$add_unless('Build.PL', undent(<<"      ---"));
      use strict;
      use Module::Build;

      my \$builder = Module::Build->new(
          module_name         => '$self->{name}',
          license             => 'perl',
      );

      \$builder->create_build_script();
      ---
  }
  else {
    $self->$add_unless('Build.PL', undent(<<"      ---"));
      use strict;
      use inc::latest 'Module::Build';

      my \$builder = Module::Build->new(
          module_name         => '$self->{name}',
          license             => 'perl',
      );

      \$builder->create_build_script();
      ---
  }

  my $module_filename =
    join( '/', ('lib', split(/::/, $self->{name})) ) . '.pm';

  unless ( $self->{xs} ) {
    $self->$add_unless($module_filename, undent(<<"      ---"));
      package $self->{name};

      use vars qw( \$VERSION );
      \$VERSION = '0.01';

      use strict;

      1;

      __END__

      =head1 NAME

      $self->{name} - Perl extension for blah blah blah

      =head1 DESCRIPTION

      Stub documentation for $self->{name}.

      =head1 AUTHOR

      A. U. Thor, a.u.thor\@a.galaxy.far.far.away

      =cut
      ---

  $self->$add_unless('t/basic.t', undent(<<"    ---"));
    use Test::More tests => 1;
    use strict;

    use $self->{name};
    ok 1;
    ---

  } else {
    $self->$add_unless($module_filename, undent(<<"      ---"));
      package $self->{name};

      \$VERSION = '0.01';

      require Exporter;
      require DynaLoader;

      \@ISA = qw(Exporter DynaLoader);
      \@EXPORT_OK = qw( okay );

      bootstrap $self->{name} \$VERSION;

      1;

      __END__

      =head1 NAME

      $self->{name} - Perl extension for blah blah blah

      =head1 DESCRIPTION

      Stub documentation for $self->{name}.

      =head1 AUTHOR

      A. U. Thor, a.u.thor\@a.galaxy.far.far.away

      =cut
      ---

    my $xs_filename =
      join( '/', ('lib', split(/::/, $self->{name})) ) . '.xs';
    $self->$add_unless($xs_filename, undent(<<"      ---"));
      #include "EXTERN.h"
      #include "perl.h"
      #include "XSUB.h"

      MODULE = $self->{name}         PACKAGE = $self->{name}

      SV *
      okay()
          CODE:
              RETVAL = newSVpv( "ok", 0 );
          OUTPUT:
              RETVAL

      const char *
      xs_version()
          CODE:
        RETVAL = XS_VERSION;
          OUTPUT:
        RETVAL

      const char *
      version()
          CODE:
        RETVAL = VERSION;
          OUTPUT:
        RETVAL
      ---

  # 5.6 is missing const char * in its typemap
  $self->$add_unless('typemap', undent(<<"      ---"));
      const char *\tT_PV
      ---

  $self->$add_unless('t/basic.t', undent(<<"    ---"));
    use Test::More tests => 2;
    use strict;

    use $self->{name};
    ok 1;

    ok( $self->{name}::okay() eq 'ok' );
    ---
  }
}

sub _gen_manifest {
  my $self     = shift;
  my $manifest = shift;

  my $fh = IO::File->new( ">$manifest" ) or do {
    die "Can't write '$manifest'\n";
  };

  my @files = ( 'MANIFEST', keys %{$self->{filedata}} );
  my $data = join( "\n", sort @files ) . "\n";
  print $fh $data;
  close( $fh );

  $self->{filedata}{MANIFEST} = $data;
  $self->{pending}{change}{MANIFEST} = 1;
}

sub name { shift()->{name} }

sub dirname {
  my $self = shift;
  my $dist = $self->{distdir} || join( '-', split( /::/, $self->{name} ) );
  return File::Spec->catdir( $self->{dir}, $dist );
}

sub _real_filename {
  my $self = shift;
  my $filename = shift;
  return File::Spec->catfile( split( /\//, $filename ) );
}

sub regen {
  my $self = shift;
  my %opts = @_;

  my $dist_dirname = $self->dirname;

  if ( $opts{clean} ) {
    $self->clean() if -d $dist_dirname;
  } else {
    # TODO: This might leave dangling directories; e.g. if the removed file
    # is 'lib/Simple/Simon.pm', the directory 'lib/Simple' will be left
    # even if there are no files left in it. However, clean() will remove it.
    my @files = keys %{$self->{pending}{remove}};
    foreach my $file ( @files ) {
      my $real_filename = $self->_real_filename( $file );
      my $fullname = File::Spec->catfile( $dist_dirname, $real_filename );
      if ( -e $fullname ) {
        1 while unlink( $fullname );
      }
      print "Unlinking pending file '$file'\n" if $VERBOSE;
      delete( $self->{pending}{remove}{$file} );
    }
  }

  foreach my $file ( keys( %{$self->{filedata}} ) ) {
    my $real_filename = $self->_real_filename( $file );
    my $fullname = File::Spec->catfile( $dist_dirname, $real_filename );

    if  ( ! -e $fullname ||
        (   -e $fullname && $self->{pending}{change}{$file} ) ) {

      print "Changed file '$file'.\n" if $VERBOSE;

      my $dirname = File::Basename::dirname( $fullname );
      unless ( -d $dirname ) {
        File::Path::mkpath( $dirname ) or do {
          die "Can't create '$dirname'\n";
        };
      }

      if ( -e $fullname ) {
        1 while unlink( $fullname );
      }

      my $fh = IO::File->new(">$fullname") or do {
        die "Can't write '$fullname'\n";
      };
      print $fh $self->{filedata}{$file};
      close( $fh );
    }

    delete( $self->{pending}{change}{$file} );
  }

  my $manifest = File::Spec->catfile( $dist_dirname, 'MANIFEST' );
  unless ( $self->{no_manifest} ) {
    if ( -e $manifest ) {
      1 while unlink( $manifest );
    }
    $self->_gen_manifest( $manifest );
  }
  return $self;
}

sub clean {
  my $self = shift;

  my $here  = Cwd::abs_path();
  my $there = File::Spec->rel2abs( $self->dirname() );

  if ( -d $there ) {
    chdir( $there ) or die "Can't change directory to '$there'\n";
  } else {
    die "Distribution not found in '$there'\n";
  }

  my %names;
  tie %names, 'Tie::CPHash';
  foreach my $file ( keys %{$self->{filedata}} ) {
    my $filename = $self->_real_filename( $file );
    $filename = lc($filename) if $vms_lower_case;
    my $dirname = File::Basename::dirname( $filename );

    $names{$filename} = 0;

    print "Splitting '$dirname'\n" if $VERBOSE;
    my @dirs = File::Spec->splitdir( $dirname );
    while ( @dirs ) {
      my $dir = ( scalar(@dirs) == 1
                  ? $dirname
                  : File::Spec->catdir( @dirs ) );
      if (length $dir) {
        print "Setting directory name '$dir' in \%names\n" if $VERBOSE;
        $names{$dir} = 0;
      }
      pop( @dirs );
    }
  }

  File::Find::finddepth( sub {
    my $name = File::Spec->canonpath( $File::Find::name );

    if ($vms_mode) {
        if ($name ne '.') {
            $name =~ s/\.\z//;
            $name = vmspath($name) if -d $name;
        }
    }
    if ($^O eq 'VMS') {
        $name = File::Spec->rel2abs($name) if $name eq File::Spec->curdir();
    }

    if ( not exists $names{$name} ) {
      print "Removing '$name'\n" if $VERBOSE;
      File::Path::rmtree( $_ );
    }
  }, ($^O eq 'VMS' ? './' : File::Spec->curdir) );

  chdir_all( $here );
  return $self;
}

sub add_file {
  my $self = shift;
  $self->change_file( @_ );
}

sub remove_file {
  my $self = shift;
  my $file = shift;
  unless ( exists $self->{filedata}{$file} ) {
    warn "Can't remove '$file': It does not exist.\n" if $VERBOSE;
  }
  delete( $self->{filedata}{$file} );
  $self->{pending}{remove}{$file} = 1;
  return $self;
}

sub change_build_pl {
  my ($self, @opts) = @_;

  my $opts = ref $opts[0] eq 'HASH' ? $opts[0] : { @opts };

  local $Data::Dumper::Terse = 1;
  (my $args = Dumper($opts)) =~ s/^\s*\{|\}\s*$//g;

  $self->change_file( 'Build.PL', undent(<<"    ---") );
    use strict;
    use Module::Build;
    my \$b = Module::Build->new(
    # Some CPANPLUS::Dist::Build versions need to allow mismatches
    # On logic: thanks to Module::Install, CPAN.pm must set both keys, but
    # CPANPLUS sets only the one
    allow_mb_mismatch => (
      \$ENV{PERL5_CPANPLUS_IS_RUNNING} && ! \$ENV{PERL5_CPAN_IS_RUNNING} ? 1 : 0
    ),
    $args
    );
    \$b->create_build_script();
    ---
  return $self;
}

sub change_file {
  my $self = shift;
  my $file = shift;
  my $data = shift;
  $self->{filedata}{$file} = $data;
  $self->{pending}{change}{$file} = 1;
  return $self;
}

sub get_file {
  my $self = shift;
  my $file = shift;
  exists($self->{filedata}{$file}) or croak("no such entry: '$file'");
  return $self->{filedata}{$file};
}

sub chdir_in {
  my $self = shift;
  $self->{original_dir} ||= Cwd::cwd; # only once!
  my $dir = $self->dirname;
  chdir($dir) or die "Can't chdir to '$dir': $!";
  return $self;
}
########################################################################

sub did_chdir { exists shift()->{original_dir} }

########################################################################

sub chdir_original {
  my $self = shift;

  my $dir = delete $self->{original_dir};
  chdir_all($dir) or die "Can't chdir to '$dir': $!";
  return $self;
}
########################################################################

sub new_from_context {
  my ($self, @args) = @_;
  require Module::Build;
  return Module::Build->new_from_context( quiet => 1, @args );
}

sub run_build_pl {
  my ($self, @args) = @_;
  require Module::Build;
  return Module::Build->run_perl_script('Build.PL', [], [@args])
}

sub run_build {
  my ($self, @args) = @_;
  require Module::Build;
  my $build_script = $^O eq 'VMS' ? 'Build.com' : 'Build';
  return Module::Build->run_perl_script($build_script, [], [@args])
}

1;

__END__


=head1 NAME

DistGen - Creates simple distributions for testing.

=head1 SYNOPSIS

  use DistGen;

  # create distribution and prepare to test
  my $dist = DistGen->new(name => 'Foo::Bar');
  $dist->chdir_in;

  # change distribution files
  $dist->add_file('t/some_test.t', $contents);
  $dist->change_file('MANIFEST.SKIP', $new_contents);
  $dist->remove_file('t/some_test.t');
  $dist->regen;

  # undo changes and clean up extraneous files
  $dist->revert;
  $dist->clean;

  # exercise the command-line interface
  $dist->run_build_pl();
  $dist->run_build('test');

  # start over as a new distribution
  $dist->reset( name => 'Foo::Bar', xs => 1 );
  $dist->chdir_in;

=head1 USAGE

A DistGen object manages a set of files in a distribution directory.

The C<new()> constructor initializes the object and creates an empty
directory for the distribution. It does not create files or chdir into
the directory.  The C<reset()> method re-initializes the object in a
new directory with new parameters.  It also does not create files or change
the current directory.

Some methods only define the target state of the distribution.  They do B<not>
make any changes to the filesystem:

  add_file
  change_file
  change_build_pl
  remove_file
  revert

Other methods then change the filesystem to match the target state of
the distribution:

  clean
  regen
  remove

Other methods are provided for a convenience during testing. The
most important is the one to enter the distribution directory:

  chdir_in

Additional methods portably encapsulate running Build.PL and Build:

  run_build_pl
  run_build

=head1 API

=head2 Constructors

=head3 new()

Create a new object and an empty directory to hold the distribution's files.
If no C<dir> option is provided, it defaults to MBTest->tmpdir, which sets
a different temp directory for Perl core testing and CPAN testing.

The C<new> method does not write any files -- see L</regen()> below.

  my $dist = DistGen->new(
    name        => 'Foo::Bar',
    dir         => MBTest->tmpdir,
    xs          => 1,
    no_manifest => 0,
  );

The parameters are as follows.

=over

=item name

The name of the module this distribution represents. The default is
'Simple'.  This should be a "Foo::Bar" (module) name, not a "Foo-Bar"
dist name.

=item dir

The (parent) directory in which to create the distribution directory.  The
distribution will be created under this according to C<distdir> parameter
below.  Defaults to a temporary directory.

  $dist = DistGen->new( dir => '/tmp/MB-test' );
  $dist->regen;

  # distribution files have been created in /tmp/MB-test/Simple

=item distdir

The name of the distribution directory to create.  Defaults to the dist form of
C<name>, e.g. 'Foo-Bar' if C<name> is 'Foo::Bar'.

=item xs

If true, generates an XS based module.

=item no_manifest

If true, C<regen()> will not create a MANIFEST file.

=back

The following files are added as part of the default distribution:

  Build.PL
  lib/Simple.pm # based on name parameter
  t/basic.t

If an XS module is generated, Simple.pm and basic.t are different and
the following files are also added:

  typemap
  lib/Simple.xs # based on name parameter

=head3 reset()

The C<reset> method re-initializes the object as if it were generated
from a fresh call to C<new>.  It takes the same optional parameters as C<new>.

  $dist->reset( name => 'Foo::Bar', xs => 0 );

=head2 Adding and editing files

Note that C<$filename> should always be specified with unix-style paths,
and are relative to the distribution root directory, e.g. C<lib/Module.pm>.

No changes are made to the filesystem until the distribution is regenerated.

=head3 add_file()

Add a $filename containing $content to the distribution.

  $dist->add_file( $filename, $content );

=head3 change_file()

Changes the contents of $filename to $content. No action is performed
until the distribution is regenerated.

  $dist->change_file( $filename, $content );

=head3 change_build_pl()

A wrapper around change_file specifically for setting Build.PL.  Instead
of file C<$content>, it takes a hash-ref of Module::Build constructor
arguments:

  $dist->change_build_pl(
    {
      module_name         => $dist->name,
      dist_version        => '3.14159265',
      license             => 'perl',
      create_readme       => 1,
    }
  );

=head3 get_file

Retrieves the target contents of C<$filename>.

  $content = $dist->get_file( $filename );

=head3 remove_file()

Removes C<$filename> from the distribution.

  $dist->remove_file( $filename );

=head3 revert()

Returns the object to its initial state, or given a $filename it returns that
file to its initial state if it is one of the built-in files.

  $dist->revert;
  $dist->revert($filename);

=head2 Changing the distribution directory

These methods immediately affect the filesystem.

=head3 regen()

Regenerate all missing or changed files.  Also deletes any files
flagged for removal with remove_file().

  $dist->regen(clean => 1);

If the optional C<clean> argument is given, it also calls C<clean>.  These
can also be chained like this, instead:

  $dist->clean->regen;

=head3 clean()

Removes any files that are not part of the distribution.

  $dist->clean;

=head3 remove()

Changes back to the original directory and removes the distribution
directory (but not the temporary directory set during C<new()>).

  $dist = DistGen->new->chdir->regen;
  # ... do some testing ...

  $dist->remove->chdir_in->regen;
  # ... do more testing ...

This is like a more aggressive form of C<clean>.  Generally, calling C<clean>
and C<regen> should be sufficient.

=head2 Changing directories

=head3 chdir_in

Change directory into the dist root.

  $dist->chdir_in;

=head3 chdir_original

Returns to whatever directory you were in before chdir_in() (regardless
of the cwd.)

  $dist->chdir_original;

=head2 Command-line helpers

These use Module::Build->run_perl_script() to ensure that Build.PL or Build are
run in a separate process using the current perl interpreter.  (Module::Build
is loaded on demand).  They also ensure appropriate naming for operating
systems that require a suffix for Build.

=head3 run_build_pl

Runs Build.PL using the current perl interpreter.  Any arguments are
passed on the command line.

  $dist->run_build_pl('--quiet');

=head3 run_build

Runs Build using the current perl interpreter.  Any arguments are
passed on the command line.

  $dist->run_build(qw/test --verbose/);

=head2 Properties

=head3 name()

Returns the name of the distribution.

  $dist->name: # e.g. Foo::Bar

=head3 dirname()

Returns the directory where the distribution is created.

  $dist->dirname; # e.g. t/_tmp/Simple

=head2 Functions

=head3 undent()

Removes leading whitespace from a multi-line string according to the
amount of whitespace on the first line.

  my $string = undent("  foo(\n    bar => 'baz'\n  )");
  $string eq "foo(
    bar => 'baz'
  )";

=cut

# vim:ts=2:sw=2:et:sta
