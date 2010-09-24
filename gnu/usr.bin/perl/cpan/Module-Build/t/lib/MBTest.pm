package MBTest;

use strict;

use IO::File ();
use File::Spec;
use File::Temp ();
use File::Path ();


# Setup the code to clean out %ENV
BEGIN {
    # Environment variables which might effect our testing
    my @delete_env_keys = qw(
        DEVEL_COVER_OPTIONS
        MODULEBUILDRC
        PERL_MB_OPT
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
  push @INC, $t_lib; # Let user's installed version override

  if ($ENV{PERL_CORE}) {
    # We change directories, so expand @INC and $^X to absolute paths
    # Also add .
    @INC = (map(File::Spec->rel2abs($_), @INC), ".");
    $^X = File::Spec->rel2abs($^X);
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
  blib_load
  timed_out
);
push @EXPORT, @extra_exports;
__PACKAGE__->export(scalar caller, @extra_exports);
# XXX ^-- that should really happen in import()


########################################################################

# always return to the current directory
{
  my $cwd = File::Spec->rel2abs(Cwd::cwd);

  sub original_cwd { return $cwd }

  END {
    # Go back to where you came from!
    chdir $cwd or die "Couldn't chdir to $cwd";
  }
}
########################################################################

{ # backwards compatible temp filename recipe adapted from perlfaq
  my $tmp_count = 0;
  my $tmp_base_name = sprintf("MB-%d-%d", $$, time());
  sub temp_file_name {
    sprintf("%s-%04d", $tmp_base_name, ++$tmp_count)
  }
}
########################################################################

# Setup a temp directory
sub tmpdir {
  my ($self, @args) = @_;
  my $dir = $ENV{PERL_CORE} ? MBTest->original_cwd : File::Spec->tmpdir;
  return File::Temp::tempdir('MB-XXXXXXXX', CLEANUP => 1, DIR => $dir, @args);
}

sub save_handle {
  my ($handle, $subr) = @_;
  my $outfile = File::Spec->catfile(File::Spec->tmpdir, temp_file_name());

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
  return wantarray ? ($stdout, $stderr) : $stdout . $stderr;
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

  my @exe_ext = exe_exts();
  if ( File::Spec->file_name_is_absolute( $thing ) ) {
    foreach my $ext ( '', @exe_ext ) {
      return "$thing$ext" if -e "$thing$ext";
    }
  }
  else {
    my @path = split $Config{path_sep}, $ENV{PATH};
    foreach (@path) {
      my $fullpath = File::Spec->catfile($_, $thing);
      foreach my $ext ( '', @exe_ext ) {
        return "$fullpath$ext" if -e "$fullpath$ext";
      }
    }
  }
  return;
}

sub check_compiler {
  return (1,1) if $ENV{PERL_CORE};

  local $SIG{__WARN__} = sub {};

  blib_load('Module::Build');
  my $mb = Module::Build->current;
  $mb->verbose( 0 );

  my $have_c_compiler;
  stderr_of( sub {$have_c_compiler = $mb->have_c_compiler} );

  # check noexec tmpdir
  my $tmp_exec;
  if ( $have_c_compiler ) {
    my $dir = MBTest->tmpdir;
    my $c_file = File::Spec->catfile($dir,'test.c');
    open my $fh, ">", $c_file;
    print {$fh} "int main() { return 0; }\n";
    close $fh;
    my $exe = $mb->cbuilder->link_executable(
      objects => $mb->cbuilder->compile( source => $c_file )
    );
    $tmp_exec = 0 == system( $exe );
  }
  return ($have_c_compiler, $tmp_exec);
}

sub have_module {
  my $module = shift;
  return eval "require $module; 1";
}

sub blib_load {
  # Load the given module and ensure it came from blib/, not the larger system
  my $mod = shift;
  have_module($mod) or die "Error loading $mod\: $@\n";

  (my $path = $mod) =~ s{::}{/}g;
  $path .= ".pm";
  my ($pkg, $file, $line) = caller;
  unless($ENV{PERL_CORE}) {
    unless($INC{$path} =~ m/\bblib\b/) {
      (my $load_from = $INC{$path}) =~ s{$path$}{};
      die "$mod loaded from '$load_from'\nIt should have been loaded from blib.  \@INC contains:\n  ",
      join("\n  ", @INC) . "\nFatal error occured in blib_load() at $file, line $line.\n";
    }
  }
}

sub timed_out {
  my ($sub, $timeout) = @_;
  return unless $sub;
  $timeout ||= 60;

  my $saw_alarm = 0;
  eval {
    local $SIG{ALRM} = sub { $saw_alarm++; die "alarm\n"; }; # NB: \n required
    alarm $timeout;
    $sub->();
    alarm 0;
  };
  if ($@) {
    die unless $@ eq "alarm\n";   # propagate unexpected errors
  }
  return $saw_alarm;
}

sub check_EUI {
  my $timed_out;
  stdout_stderr_of( sub {
      $timed_out = timed_out( sub {
          ExtUtils::Installed->new(extra_libs => [@INC])
        }
      );
    }
  );
  return ! $timed_out;
}

1;
# vim:ts=2:sw=2:et:sta
