package MBTest;

use strict;

use File::Spec;
use File::Temp ();
use File::Path ();


# Setup the code to clean out %ENV
BEGIN {
    # Environment variables which might effect our testing
    my @delete_env_keys = qw(
        DEVEL_COVER_OPTIONS
        MODULEBUILDRC
        HARNESS_TIMER
        HARNESS_OPTIONS
        HARNESS_VERBOSE
        PREFIX
        INSTALL_BASE
        INSTALLDIRS
    );

    # Remember the ENV values because on VMS %ENV is global
    # to the user, not the process.
    my %restore_env_keys;

    sub clean_env {
        for my $key (@delete_env_keys) {
            if( exists $ENV{$key} ) {
                $restore_env_keys{$key} = delete $ENV{$key};
            }
            else {
                delete $ENV{$key};
            }
        }
    }

    END {
        while( my($key, $val) = each %restore_env_keys ) {
            $ENV{$key} = $val;
        }
    }
}


BEGIN {
  clean_env();

  # In case the test wants to use our other bundled
  # modules, make sure they can be loaded.
  my $t_lib = File::Spec->catdir('t', 'bundled');

  unless ($ENV{PERL_CORE}) {
    push @INC, $t_lib; # Let user's installed version override
  } else {
    # We change directories, so expand @INC and $^X to absolute paths
    # Also add .
    @INC = (map(File::Spec->rel2abs($_), @INC), ".");
    $^X = File::Spec->rel2abs($^X);

    # we are in 't', go up a level so we don't create t/t/_tmp
    chdir '..' or die "Couldn't chdir to ..";

    push @INC, File::Spec->catdir(qw/lib Module Build/, $t_lib);

    # make sure children get @INC pointing to uninstalled files
    require Cwd;
    $ENV{PERL5LIB} = File::Spec->catdir(Cwd::cwd(), 'lib');
  }
}

use Exporter;
use Test::More;
use Config;
use Cwd ();

# We pass everything through to Test::More
use vars qw($VERSION @ISA @EXPORT %EXPORT_TAGS $TODO);
$VERSION = 0.01_01;
@ISA = qw(Test::More); # Test::More isa Exporter
@EXPORT = @Test::More::EXPORT;
%EXPORT_TAGS = %Test::More::EXPORT_TAGS;

# We have a few extra exports, but Test::More has a special import()
# that won't take extra additions.
my @extra_exports = qw(
  stdout_of
  stderr_of
  stdout_stderr_of
  slurp
  find_in_path
  check_compiler
  have_module
  ensure_blib
);
push @EXPORT, @extra_exports;
__PACKAGE__->export(scalar caller, @extra_exports);
# XXX ^-- that should really happen in import()


########################################################################

# always return to the current directory
{ 
  my $cwd = Cwd::cwd;

  END {
    # Go back to where you came from!
    chdir $cwd or die "Couldn't chdir to $cwd";
  }
}
########################################################################

{ # backwards compatible temp filename recipe adapted from perlfaq
  my $tmp_count = 0;
  my $tmp_base_name = sprintf("%d-%d", $$, time());
  sub temp_file_name {
    sprintf("%s-%04d", $tmp_base_name, ++$tmp_count)
  }
}
########################################################################

# Setup a temp directory 
sub tmpdir { 
  return File::Temp::tempdir( 'MB-XXXXXXXX', 
    CLEANUP => 1, DIR => $ENV{PERL_CORE} ? Cwd::cwd : File::Spec->tmpdir
  );
}

sub save_handle {
  my ($handle, $subr) = @_;
  my $outfile = temp_file_name();

  local *SAVEOUT;
  open SAVEOUT, ">&" . fileno($handle)
    or die "Can't save output handle: $!";
  open $handle, "> $outfile" or die "Can't create $outfile: $!";

  eval {$subr->()};
  open $handle, ">&SAVEOUT" or die "Can't restore output: $!";

  my $ret = slurp($outfile);
  1 while unlink $outfile;
  return $ret;
}

sub stdout_of { save_handle(\*STDOUT, @_) }
sub stderr_of { save_handle(\*STDERR, @_) }
sub stdout_stderr_of {
  my $subr = shift;
  my ($stdout, $stderr);
  $stdout = stdout_of ( sub {
      $stderr = stderr_of( $subr )
  });
  return ($stdout, $stderr);
}

sub slurp {
  my $fh = IO::File->new($_[0]) or die "Can't open $_[0]: $!";
  local $/;
  return scalar <$fh>;
}

# Some extensions we should know about if we're looking for executables
sub exe_exts {

  if ($^O eq 'MSWin32') {
    return split($Config{path_sep}, $ENV{PATHEXT} || '.com;.exe;.bat');
  }
  if ($^O eq 'os2') {
    return qw(.exe .com .pl .cmd .bat .sh .ksh);
  }
  return;
}

sub find_in_path {
  my $thing = shift;
  
  my @path = split $Config{path_sep}, $ENV{PATH};
  my @exe_ext = exe_exts();
  foreach (@path) {
    my $fullpath = File::Spec->catfile($_, $thing);
    foreach my $ext ( '', @exe_ext ) {
      return "$fullpath$ext" if -e "$fullpath$ext";
    }
  }
  return;
}

# returns ($have_c_compiler, $C_support_feature);
sub check_compiler {
  return (1,1) if $ENV{PERL_CORE};

  local $SIG{__WARN__} = sub {};

  my $mb = Module::Build->current;
  $mb->verbose( 0 );

  my $have_c_compiler;
  stderr_of( sub {$have_c_compiler = $mb->have_c_compiler} );

  return ($have_c_compiler, $mb->feature('C_support'));
}

sub have_module {
  my $module = shift;
  return eval "use $module; 1";
}

sub ensure_blib {
  # Make sure the given module was loaded from blib/, not the larger system
  my $mod = shift;
  (my $path = $mod) =~ s{::}{/}g;
 
  local $Test::Builder::Level = $Test::Builder::Level + 1; 
 SKIP: {
    skip "no blib in core", 1 if $ENV{PERL_CORE};
    like $INC{"$path.pm"}, qr/\bblib\b/, "Make sure $mod was loaded from blib/"
      or diag "PERL5LIB: " . ($ENV{PERL5LIB} || '') . "\n" .
              "PERL5OPT: " . ($ENV{PERL5OPT} || '') . "\n" .
              "\@INC contains:\n  " . join("\n  ", @INC) . "\n"; 
  }
}

1;
# vim:ts=2:sw=2:et:sta
