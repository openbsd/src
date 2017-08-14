package FilePathTest;
use strict;
use warnings;
use base 'Exporter';
use SelectSaver;
use Cwd;
use File::Spec::Functions;

our @EXPORT = qw(_run_for_warning _run_for_verbose _basedir
                 _cannot_delete_safe_mode
                 _verbose_expected);

sub _basedir {
  return catdir( curdir(),
                 sprintf( 'test-%x-%x-%x', time, $$, rand(99999) ),
  );

}

sub _run_for_warning {
  my $coderef = shift;
  my $warn = '';
  local $SIG{__WARN__} = sub { $warn .= shift };
  &$coderef;
  return $warn;
}

sub _run_for_verbose {
  my $coderef = shift;
  my $stdout = '';
  {
    my $guard = SelectSaver->new(_ref_to_fh(\$stdout));
    &$coderef;
  }
  return $stdout;
}

sub _ref_to_fh {
  my $output = shift;
  open my $fh, '>', $output;
  return $fh;
}

# Whether a directory can be deleted without modifying permissions varies
# by platform and by current privileges, so we really have to do the same
# check the module does in safe mode to determine that.

sub _cannot_delete_safe_mode {
  my $path = shift;
  return $^O eq 'VMS'
         ? !&VMS::Filespec::candelete($path)
         : !-w $path;
}

# What verbose mode reports depends on what it can do in safe mode.
# Plus on VMS, mkpath may report what it's operating on in a
# different format from the format of the path passed to it.

sub _verbose_expected {
  my ($function, $path, $safe_mode, $base) = @_;
  my $expected;

  if ($function =~ m/^(mkpath|make_path)$/) {
    # On VMS, mkpath reports in Unix format.  Maddeningly, it
    # reports the top-level directory without a trailing slash
    # and everything else with.
    if ($^O eq 'VMS') {
      $path = VMS::Filespec::unixify($path);
      $path =~ s/\/$// if defined $base && $base;
    }
    $expected = "mkdir $path\n";
  }
  elsif ($function =~ m/^(rmtree|remove_tree)$/) {
    # N.B. Directories must still/already exist for this to work.
    $expected = $safe_mode && _cannot_delete_safe_mode($path)
              ? "skipped $path\n"
              : "rmdir $path\n";
  }
  elsif ($function =~ m/^(unlink)$/) {
    $expected = "unlink $path\n";
    $expected =~ s/\n\z/\.\n/ if $^O eq 'VMS';
  }
  else {
    die "Unknown function $function in _verbose_expected";
  }
  return $expected;
}

BEGIN {
  if ($] < 5.008000) {
    eval qq{#line @{[__LINE__+1]} "@{[__FILE__]}"\n} . <<'END' or die $@;
      no warnings 'redefine';
      use Symbol ();

      sub _ref_to_fh {
        my $output = shift;
        my $fh = Symbol::gensym();
        tie *$fh, 'StringIO', $output;
        return $fh;
      }

      package StringIO;
      sub TIEHANDLE { bless [ $_[1] ], $_[0] }
      sub CLOSE    { @{$_[0]} = (); 1 }
      sub PRINT    { ${ $_[0][0] } .= $_[1] }
      sub PRINTF   { ${ $_[0][0] } .= sprintf $_[1], @_[2..$#_] }
      1;
END
  }
}

1;
