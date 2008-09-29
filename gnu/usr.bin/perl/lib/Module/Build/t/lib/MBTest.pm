package MBTest;

use strict;

use File::Spec;
use File::Path ();

BEGIN {
  # Make sure none of our tests load the users ~/.modulebuildrc file
  $ENV{MODULEBUILDRC} = 'NONE';

  # In case the test wants to use Test::More or our other bundled
  # modules, make sure they can be loaded.  They'll still do "use
  # Test::More" in the test script.
  my $t_lib = File::Spec->catdir('t', 'bundled');

  unless ($ENV{PERL_CORE}) {
    push @INC, $t_lib; # Let user's installed version override
  } else {
    # We change directories, so expand @INC to absolute paths
    # Also add .
    @INC = (map(File::Spec->rel2abs($_), @INC), ".");

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
$VERSION = 0.01;
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
);
push @EXPORT, @extra_exports;
__PACKAGE__->export(scalar caller, @extra_exports);
# XXX ^-- that should really happen in import()
########################################################################

{ # Setup a temp directory if it doesn't exist
  my $cwd = Cwd::cwd;
  my $tmp = File::Spec->catdir( $cwd, 't', '_tmp' . $$);
  mkdir $tmp, 0777 unless -d $tmp;

  sub tmpdir { $tmp }
  END {
    if(-d $tmp) {
      File::Path::rmtree($tmp) or warn "cannot clean dir '$tmp'";
    }
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

1;
# vim:ts=2:sw=2:et:sta
