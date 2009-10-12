package DistGen;

use strict;

use vars qw( $VERSION $VERBOSE @EXPORT_OK);

$VERSION = '0.01';
$VERBOSE = 0;


use Carp;

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
########################################################################

sub new {
  my $package = shift;
  my %options = @_;

  $options{name} ||= 'Simple';
  $options{dir}  ||= Cwd::cwd();

  my %data = (
    skip_manifest => 0,
    xs            => 0,
    %options,
  );
  my $self = bless( \%data, $package );

  # So we can clean up later even if the caller chdir()s
  $self->{dir} = File::Spec->rel2abs($self->{dir});

  tie %{$self->{filedata}}, 'Tie::CPHash';

  tie %{$self->{pending}{change}}, 'Tie::CPHash';

  if ( -d $self->dirname ) {
    warn "Warning: Removing existing directory '@{[$self->dirname]}'\n";
    $self->remove;
  }

  $self->_gen_default_filedata();

  return $self;
}

sub _gen_default_filedata {
  my $self = shift;

  # TODO maybe a public method like this (but with a better name?)
  my $add_unless = sub {
    my $self = shift;
    my ($member, $data) = @_;
    $self->add_file($member, $data) unless($self->{filedata}{$member});
  };

  $self->$add_unless('Build.PL', undent(<<"    ---"));
    use strict;
    use Module::Build;

    my \$builder = Module::Build->new(
        module_name         => '$self->{name}',
        license             => 'perl',
    );

    \$builder->create_build_script();
    ---

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
      const char *              T_PV
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
    $self->remove();
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
  my $dist = join( '-', split( /::/, $self->{name} ) );
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

    if ( ! -e $fullname ||
	 ( -e $fullname && $self->{pending}{change}{$file} ) ) {

      print "Changed file '$file'.\n" if $VERBOSE;

      my $dirname = File::Basename::dirname( $fullname );
      unless ( -d $dirname ) {
        File::Path::mkpath( $dirname ) or do {
          $self->remove();
          die "Can't create '$dirname'\n";
        };
      }

      if ( -e $fullname ) {
        1 while unlink( $fullname );
      }

      my $fh = IO::File->new(">$fullname") or do {
        $self->remove();
        die "Can't write '$fullname'\n";
      };
      print $fh $self->{filedata}{$file};
      close( $fh );
    }

    delete( $self->{pending}{change}{$file} );
  }

  my $manifest = File::Spec->catfile( $dist_dirname, 'MANIFEST' );
  unless ( $self->{skip_manifest} ) {
    if ( -e $manifest ) {
      1 while unlink( $manifest );
    }
    $self->_gen_manifest( $manifest );
  }
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

  chdir( $here );
}

sub remove {
  my $self = shift;
  croak("invalid usage -- remove()") if(@_);
  $self->chdir_original if($self->did_chdir);
  File::Path::rmtree( $self->dirname );
  # might as well check
  croak("\nthis test should have used chdir_in()") unless(Cwd::getcwd);
}

sub revert {
  my $self = shift;
  die "Unimplemented.\n";
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
}

sub change_build_pl {
  my ($self, $opts) = @_;

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
}

sub change_file {
  my $self = shift;
  my $file = shift;
  my $data = shift;
  $self->{filedata}{$file} = $data;
  $self->{pending}{change}{$file} = 1;
}

sub get_file {
  my $self = shift;
  my $file = shift;
  exists($self->{filedata}{$file}) or croak("no such entry: '$file'");
  return $self->{filedata}{$file};
}

sub chdir_in {
  my $self = shift;

  $self->{original_dir} ||= Cwd::cwd; # only once
  my $dir = $self->dirname;
  chdir($dir) or die "Can't chdir to '$dir': $!";
}
########################################################################

sub did_chdir {
  my $self = shift;

  return exists($self->{original_dir});
}
########################################################################

sub chdir_original {
  my $self = shift;

  croak("never called chdir_in()") unless($self->{original_dir});
  my $dir = $self->{original_dir};
  chdir($dir) or die "Can't chdir to '$dir': $!";
}
########################################################################

1;

__END__


=head1 NAME

DistGen - Creates simple distributions for testing.

=head1 SYNOPSIS

  use DistGen;

  my $dist = DistGen->new(dir => $tmp);
  ...
  $dist->add_file('t/some_test.t', $contents);
  ...
  $dist->regen;

  chdir($dist->dirname) or
    die "Cannot chdir to '@{[$dist->dirname]}': $!";
  ...
  $dist->clean;
  ...
  chdir($cwd) or die "cannot return to $cwd";
  $dist->remove;

=head1 API

=head2 Constructor

=head3 new()

Create a new object.  Does not write its contents (see L</regen()>.)

  my $tmp = MBTest->tmpdir;
  my $dist = DistGen->new(
    name => 'Foo::Bar',
    dir  => $tmp,
    xs   => 1,
  );

The parameters are as follows.

=over

=item name

The name of the module this distribution represents. The default is
'Simple'.  This should be a "Foo::Bar" (module) name, not a "Foo-Bar"
dist name.

=item dir

The (parent) directory in which to create the distribution directory.
The default is File::Spec->curdir.  The distribution will be created
under this according to the "dist" form of C<name> (e.g. "Foo-Bar".)

=item xs

If true, generates an XS based module.

=back

=head2 Manipulating the Distribution

These methods immediately affect the filesystem.

=head3 regen()

Regenerate all missing or changed files.

  $dist->regen(clean => 1);

If the optional C<clean> argument is given, it also removes any
extraneous files that do not belong to the distribution.

=head2 chdir_in

Change directory into the dist root.

  $dist->chdir_in;

=head2 chdir_original

Returns to whatever directory you were in before chdir_in() (regardless
of the cwd.)

  $dist->chdir_original;

=head3 clean()

Removes any files that are not part of the distribution.

  $dist->clean;

=begin TODO

=head3 revert()

[Unimplemented] Returns the object to its initial state, or given a
$filename it returns that file to it's initial state if it is one of
the built-in files.

  $dist->revert;
  $dist->revert($filename);

=end TODO

=head3 remove()

Removes the entire distribution directory.

=head2 Editing Files

Note that C<$filename> should always be specified with unix-style paths,
and are relative to the distribution root directory, e.g. C<lib/Module.pm>.

No filesystem action is performed until the distribution is regenerated.

=head3 add_file()

Add a $filename containing $content to the distribution.

  $dist->add_file( $filename, $content );

=head3 remove_file()

Removes C<$filename> from the distribution.

  $dist->remove_file( $filename );

=head3 change_file()

Changes the contents of $filename to $content. No action is performed
until the distribution is regenerated.

  $dist->change_file( $filename, $content );

=head2 Properties

=head3 name()

Returns the name of the distribution.

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
