package inc::latest::private;
use strict;
use vars qw($VERSION);
$VERSION = '0.3603';
$VERSION = eval $VERSION;

use File::Spec;
use IO::File;

# must ultimately "goto" the import routine of the module to be loaded
# so that the calling package is correct when $mod->import() runs.
sub import {
  my ($package, $mod, @args) = @_;
  my $file = $package->_mod2path($mod);

  if ($INC{$file}) {
    # Already loaded, but let _load_module handle import args
    goto \&_load_module;
  }

  # A bundled copy must be present
  my ($bundled, $bundled_dir) = $package->_search_bundled($file)
    or die "No bundled copy of $mod found";

  my $from_inc = $package->_search_INC($file);
  unless ($from_inc) {
    # Only bundled is available
    unshift(@INC, $bundled_dir);
    goto \&_load_module;
  }

  if (_version($from_inc) >= _version($bundled)) {
    # Ignore the bundled copy
    goto \&_load_module;
  }

  # Load the bundled copy
  unshift(@INC, $bundled_dir);
  goto \&_load_module;
}

sub _version {
  require ExtUtils::MakeMaker;
  return ExtUtils::MM->parse_version(shift);
}

# use "goto" for import to preserve caller
sub _load_module {
  my $package = shift; # remaining @_ is ready for goto
  my ($mod, @args) = @_;
  eval "require $mod; 1" or die $@;
  if ( my $import = $mod->can('import') ) {
    goto $import;
  }
  return 1;
}

sub _search_bundled {
  my ($self, $file) = @_;

  my $mypath = 'inc';

  local *DH;   # Maintain 5.005 compatibility
  opendir DH, $mypath or die "Can't open directory $mypath: $!";

  while (defined(my $e = readdir DH)) {
    next unless $e =~ /^inc_/;
    my $try = File::Spec->catfile($mypath, $e, $file);

    return($try, File::Spec->catdir($mypath, $e)) if -e $try;
  }
  return;
}

# Look for the given path in @INC.
sub _search_INC {
  # TODO: doesn't handle coderefs or arrayrefs or objects in @INC, but
  # it probably should
  my ($self, $file) = @_;

  foreach my $dir (@INC) {
    next if ref $dir;
    my $try = File::Spec->catfile($dir, $file);
    return $try if -e $try;
  }

  return;
}

# Translate a module name into a directory/file.pm to search for in @INC
sub _mod2path {
  my ($self, $mod) = @_;
  my @parts = split /::/, $mod;
  $parts[-1] .= '.pm';
  return $parts[0] if @parts == 1;
  return File::Spec->catfile(@parts);
}

1;


