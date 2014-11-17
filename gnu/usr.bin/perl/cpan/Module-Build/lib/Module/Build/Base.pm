# -*- mode: cperl; tab-width: 8; indent-tabs-mode: nil; basic-offset: 2 -*-
# vim:ts=8:sw=2:et:sta:sts=2
package Module::Build::Base;

use strict;
use vars qw($VERSION);
use warnings;

$VERSION = '0.4205';
$VERSION = eval $VERSION;
BEGIN { require 5.006001 }

use Carp;
use Cwd ();
use File::Copy ();
use File::Find ();
use File::Path ();
use File::Basename ();
use File::Spec 0.82 ();
use File::Compare ();
use Module::Build::Dumper ();
use Text::ParseWords ();

use Module::Build::ModuleInfo;
use Module::Build::Notes;
use Module::Build::Config;
use Module::Build::Version;


#################### Constructors ###########################
sub new {
  my $self = shift()->_construct(@_);

  $self->{invoked_action} = $self->{action} ||= 'Build_PL';
  $self->cull_args(@ARGV);

  die "Too early to specify a build action '$self->{action}'.  Do 'Build $self->{action}' instead.\n"
    if $self->{action} && $self->{action} ne 'Build_PL';

  $self->check_manifest;
  $self->auto_require;

  # All checks must run regardless if one fails, so no short circuiting!
  if( grep { !$_ } $self->check_prereq, $self->check_autofeatures ) {
    $self->log_warn(<<EOF);

ERRORS/WARNINGS FOUND IN PREREQUISITES.  You may wish to install the versions
of the modules indicated above before proceeding with this installation

EOF
    unless (
      $self->dist_name eq 'Module-Build' ||
      $ENV{PERL5_CPANPLUS_IS_RUNNING} || $ENV{PERL5_CPAN_IS_RUNNING}
    ) {
      $self->log_warn(
        "Run 'Build installdeps' to install missing prerequisites.\n\n"
      );
    }
  }

  # record for later use in resume;
  $self->{properties}{_added_to_INC} = [ $self->_added_to_INC ];

  $self->set_bundle_inc;

  $self->dist_name;
  $self->dist_version;
  $self->release_status;
  $self->_guess_module_name unless $self->module_name;

  $self->_find_nested_builds;

  return $self;
}

sub resume {
  my $package = shift;
  my $self = $package->_construct(@_);
  $self->read_config;

  my @added_earlier = @{ $self->{properties}{_added_to_INC} || [] };

  @INC = ($self->_added_to_INC, @added_earlier, $self->_default_INC);

  # If someone called Module::Build->current() or
  # Module::Build->new_from_context() and the correct class to use is
  # actually a *subclass* of Module::Build, we may need to load that
  # subclass here and re-delegate the resume() method to it.
  unless ( UNIVERSAL::isa($package, $self->build_class) ) {
    my $build_class = $self->build_class;
    my $config_dir = $self->config_dir || '_build';
    my $build_lib = File::Spec->catdir( $config_dir, 'lib' );
    unshift( @INC, $build_lib );
    unless ( $build_class->can('new') ) {
      eval "require $build_class; 1" or die "Failed to re-load '$build_class': $@";
    }
    return $build_class->resume(@_);
  }

  unless ($self->_perl_is_same($self->{properties}{perl})) {
    my $perl = $self->find_perl_interpreter;
    die(<<"DIEFATAL");
* FATAL ERROR: Perl interpreter mismatch. Configuration was initially
  created with '$self->{properties}{perl}'
  but we are now using '$perl'.  You must
  run 'Build realclean' or 'make realclean' and re-configure.
DIEFATAL
  }

  $self->cull_args(@ARGV);

  unless ($self->allow_mb_mismatch) {
    my $mb_version = $Module::Build::VERSION;
    if ( $mb_version ne $self->{properties}{mb_version} ) {
      $self->log_warn(<<"MISMATCH");
* WARNING: Configuration was initially created with Module::Build
  version '$self->{properties}{mb_version}' but we are now using version '$mb_version'.
  If errors occur, you must re-run the Build.PL or Makefile.PL script.
MISMATCH
    }
  }

  $self->{invoked_action} = $self->{action} ||= 'build';

  return $self;
}

sub new_from_context {
  my ($package, %args) = @_;

  $package->run_perl_script('Build.PL',[],[$package->unparse_args(\%args)]);
  return $package->resume;
}

sub current {
  # hmm, wonder what the right thing to do here is
  local @ARGV;
  return shift()->resume;
}

sub _construct {
  my ($package, %input) = @_;

  my $args   = delete $input{args}   || {};
  my $config = delete $input{config} || {};

  my $self = bless {
      args => {%$args},
      config => Module::Build::Config->new(values => $config),
      properties => {
          base_dir        => $package->cwd,
          mb_version      => $Module::Build::VERSION,
          %input,
      },
      phash => {},
      stash => {}, # temporary caching, not stored in _build
  }, $package;

  $self->_set_defaults;
  my ($p, $ph) = ($self->{properties}, $self->{phash});

  foreach (qw(notes config_data features runtime_params cleanup auto_features)) {
    my $file = File::Spec->catfile($self->config_dir, $_);
    $ph->{$_} = Module::Build::Notes->new(file => $file);
    $ph->{$_}->restore if -e $file;
    if (exists $p->{$_}) {
      my $vals = delete $p->{$_};
      while (my ($k, $v) = each %$vals) {
        $self->$_($k, $v);
      }
    }
  }

  # The following warning could be unnecessary if the user is running
  # an embedded perl, but there aren't too many of those around, and
  # embedded perls aren't usually used to install modules, and the
  # installation process sometimes needs to run external scripts
  # (e.g. to run tests).
  $p->{perl} = $self->find_perl_interpreter
    or $self->log_warn("Warning: Can't locate your perl binary");

  my $blibdir = sub { File::Spec->catdir($p->{blib}, @_) };
  $p->{bindoc_dirs} ||= [ $blibdir->("script") ];
  $p->{libdoc_dirs} ||= [ $blibdir->("lib"), $blibdir->("arch") ];

  $p->{dist_author} = [ $p->{dist_author} ] if defined $p->{dist_author} and not ref $p->{dist_author};

  # Synonyms
  $p->{requires} = delete $p->{prereq} if defined $p->{prereq};
  $p->{script_files} = delete $p->{scripts} if defined $p->{scripts};

  # Convert to from shell strings to arrays
  for ('extra_compiler_flags', 'extra_linker_flags') {
    $p->{$_} = [ $self->split_like_shell($p->{$_}) ] if exists $p->{$_};
  }

  # Convert to arrays
  for ('include_dirs') {
    $p->{$_} = [ $p->{$_} ] if exists $p->{$_} && !ref $p->{$_}
  }

  $self->add_to_cleanup( @{delete $p->{add_to_cleanup}} )
    if $p->{add_to_cleanup};

  return $self;
}

################## End constructors #########################

sub log_info {
  my $self = shift;
  print @_ if ref($self) && ( $self->verbose || ! $self->quiet );
}
sub log_verbose {
  my $self = shift;
  print @_ if ref($self) && $self->verbose;
}
sub log_debug {
  my $self = shift;
  print @_ if ref($self) && $self->debug;
}

sub log_warn {
  # Try to make our call stack invisible
  shift;
  if (@_ and $_[-1] !~ /\n$/) {
    my (undef, $file, $line) = caller();
    warn @_, " at $file line $line.\n";
  } else {
    warn @_;
  }
}


# install paths must be generated when requested to be sure all changes
# to config (from various sources) are included
sub _default_install_paths {
  my $self = shift;
  my $c = $self->{config};
  my $p = {};

  my @libstyle = $c->get('installstyle') ?
      File::Spec->splitdir($c->get('installstyle')) : qw(lib perl5);
  my $arch     = $c->get('archname');
  my $version  = $c->get('version');

  my $bindoc  = $c->get('installman1dir') || undef;
  my $libdoc  = $c->get('installman3dir') || undef;

  my $binhtml = $c->get('installhtml1dir') || $c->get('installhtmldir') || undef;
  my $libhtml = $c->get('installhtml3dir') || $c->get('installhtmldir') || undef;

  $p->{install_sets} =
    {
     core   => {
       lib     => $c->get('installprivlib'),
       arch    => $c->get('installarchlib'),
       bin     => $c->get('installbin'),
       script  => $c->get('installscript'),
       bindoc  => $bindoc,
       libdoc  => $libdoc,
       binhtml => $binhtml,
       libhtml => $libhtml,
     },
     site   => {
       lib     => $c->get('installsitelib'),
       arch    => $c->get('installsitearch'),
       bin     => $c->get('installsitebin')      || $c->get('installbin'),
       script  => $c->get('installsitescript')   ||
         $c->get('installsitebin') || $c->get('installscript'),
       bindoc  => $c->get('installsiteman1dir')  || $bindoc,
       libdoc  => $c->get('installsiteman3dir')  || $libdoc,
       binhtml => $c->get('installsitehtml1dir') || $binhtml,
       libhtml => $c->get('installsitehtml3dir') || $libhtml,
     },
     vendor => {
       lib     => $c->get('installvendorlib'),
       arch    => $c->get('installvendorarch'),
       bin     => $c->get('installvendorbin')      || $c->get('installbin'),
       script  => $c->get('installvendorscript')   ||
         $c->get('installvendorbin') || $c->get('installscript'),
       bindoc  => $c->get('installvendorman1dir')  || $bindoc,
       libdoc  => $c->get('installvendorman3dir')  || $libdoc,
       binhtml => $c->get('installvendorhtml1dir') || $binhtml,
       libhtml => $c->get('installvendorhtml3dir') || $libhtml,
     },
    };

  $p->{original_prefix} =
    {
     core   => $c->get('installprefixexp') || $c->get('installprefix') ||
               $c->get('prefixexp')        || $c->get('prefix') || '',
     site   => $c->get('siteprefixexp'),
     vendor => $c->get('usevendorprefix') ? $c->get('vendorprefixexp') : '',
    };
  $p->{original_prefix}{site} ||= $p->{original_prefix}{core};

  # Note: you might be tempted to use $Config{installstyle} here
  # instead of hard-coding lib/perl5, but that's been considered and
  # (at least for now) rejected.  `perldoc Config` has some wisdom
  # about it.
  $p->{install_base_relpaths} =
    {
     lib     => ['lib', 'perl5'],
     arch    => ['lib', 'perl5', $arch],
     bin     => ['bin'],
     script  => ['bin'],
     bindoc  => ['man', 'man1'],
     libdoc  => ['man', 'man3'],
     binhtml => ['html'],
     libhtml => ['html'],
    };

  $p->{prefix_relpaths} =
    {
     core => {
       lib        => [@libstyle],
       arch       => [@libstyle, $version, $arch],
       bin        => ['bin'],
       script     => ['bin'],
       bindoc     => ['man', 'man1'],
       libdoc     => ['man', 'man3'],
       binhtml    => ['html'],
       libhtml    => ['html'],
     },
     vendor => {
       lib        => [@libstyle],
       arch       => [@libstyle, $version, $arch],
       bin        => ['bin'],
       script     => ['bin'],
       bindoc     => ['man', 'man1'],
       libdoc     => ['man', 'man3'],
       binhtml    => ['html'],
       libhtml    => ['html'],
     },
     site => {
       lib        => [@libstyle, 'site_perl'],
       arch       => [@libstyle, 'site_perl', $version, $arch],
       bin        => ['bin'],
       script     => ['bin'],
       bindoc     => ['man', 'man1'],
       libdoc     => ['man', 'man3'],
       binhtml    => ['html'],
       libhtml    => ['html'],
     },
    };
    return $p
}

sub _find_nested_builds {
  my $self = shift;
  my $r = $self->recurse_into or return;

  my ($file, @r);
  if (!ref($r) && $r eq 'auto') {
    local *DH;
    opendir DH, $self->base_dir
      or die "Can't scan directory " . $self->base_dir . " for nested builds: $!";
    while (defined($file = readdir DH)) {
      my $subdir = File::Spec->catdir( $self->base_dir, $file );
      next unless -d $subdir;
      push @r, $subdir if -e File::Spec->catfile( $subdir, 'Build.PL' );
    }
  }

  $self->recurse_into(\@r);
}

sub cwd {
  return Cwd::cwd();
}

sub _quote_args {
  # Returns a string that can become [part of] a command line with
  # proper quoting so that the subprocess sees this same list of args.
  my ($self, @args) = @_;

  my @quoted;

  for (@args) {
    if ( /^[^\s*?!\$<>;\\|'"\[\]\{\}]+$/ ) {
      # Looks pretty safe
      push @quoted, $_;
    } else {
      # XXX this will obviously have to improve - is there already a
      # core module lying around that does proper quoting?
      s/('+)/'"$1"'/g;
      push @quoted, qq('$_');
    }
  }

  return join " ", @quoted;
}

sub _backticks {
  my ($self, @cmd) = @_;
  if ($self->have_forkpipe) {
    local *FH;
    my $pid = open *FH, "-|";
    if ($pid) {
      return wantarray ? <FH> : join '', <FH>;
    } else {
      die "Can't execute @cmd: $!\n" unless defined $pid;
      exec { $cmd[0] } @cmd;
    }
  } else {
    my $cmd = $self->_quote_args(@cmd);
    return `$cmd`;
  }
}

# Tells us whether the construct open($fh, '-|', @command) is
# supported.  It would probably be better to dynamically sense this.
sub have_forkpipe { 1 }

# Determine whether a given binary is the same as the perl
# (configuration) that started this process.
sub _perl_is_same {
  my ($self, $perl) = @_;

  my @cmd = ($perl);

  # When run from the perl core, @INC will include the directories
  # where perl is yet to be installed. We need to reference the
  # absolute path within the source distribution where it can find
  # it's Config.pm This also prevents us from picking up a Config.pm
  # from a different configuration that happens to be already
  # installed in @INC.
  if ($ENV{PERL_CORE}) {
    push @cmd, '-I' . File::Spec->catdir(File::Basename::dirname($perl), 'lib');
  }

  push @cmd, qw(-MConfig=myconfig -e print -e myconfig);
  return $self->_backticks(@cmd) eq Config->myconfig;
}

# cache _discover_perl_interpreter() results
{
  my $known_perl;
  sub find_perl_interpreter {
    my $self = shift;

    return $known_perl if defined($known_perl);
    return $known_perl = $self->_discover_perl_interpreter;
  }
}

# Returns the absolute path of the perl interpreter used to invoke
# this process. The path is derived from $^X or $Config{perlpath}. On
# some platforms $^X contains the complete absolute path of the
# interpreter, on other it may contain a relative path, or simply
# 'perl'. This can also vary depending on whether a path was supplied
# when perl was invoked. Additionally, the value in $^X may omit the
# executable extension on platforms that use one. It's a fatal error
# if the interpreter can't be found because it can result in undefined
# behavior by routines that depend on it (generating errors or
# invoking the wrong perl.)
sub _discover_perl_interpreter {
  my $proto = shift;
  my $c     = ref($proto) ? $proto->{config} : 'Module::Build::Config';

  my $perl  = $^X;
  my $perl_basename = File::Basename::basename($perl);

  my @potential_perls;

  # Try 1, Check $^X for absolute path
  push( @potential_perls, $perl )
      if File::Spec->file_name_is_absolute($perl);

  # Try 2, Check $^X for a valid relative path
  my $abs_perl = File::Spec->rel2abs($perl);
  push( @potential_perls, $abs_perl );

  # Try 3, Last ditch effort: These two option use hackery to try to locate
  # a suitable perl. The hack varies depending on whether we are running
  # from an installed perl or an uninstalled perl in the perl source dist.
  if ($ENV{PERL_CORE}) {

    # Try 3.A, If we are in a perl source tree, running an uninstalled
    # perl, we can keep moving up the directory tree until we find our
    # binary. We wouldn't do this under any other circumstances.

    # CBuilder is also in the core, so it should be available here
    require ExtUtils::CBuilder;
    my $perl_src = Cwd::realpath( ExtUtils::CBuilder->perl_src );
    if ( defined($perl_src) && length($perl_src) ) {
      my $uninstperl =
        File::Spec->rel2abs(File::Spec->catfile( $perl_src, $perl_basename ));
      push( @potential_perls, $uninstperl );
    }

  } else {

    # Try 3.B, First look in $Config{perlpath}, then search the user's
    # PATH. We do not want to do either if we are running from an
    # uninstalled perl in a perl source tree.

    push( @potential_perls, $c->get('perlpath') );

    push( @potential_perls,
          map File::Spec->catfile($_, $perl_basename), File::Spec->path() );
  }

  # Now that we've enumerated the potential perls, it's time to test
  # them to see if any of them match our configuration, returning the
  # absolute path of the first successful match.
  my $exe = $c->get('exe_ext');
  foreach my $thisperl ( @potential_perls ) {

    if (defined $exe) {
      $thisperl .= $exe unless $thisperl =~ m/$exe$/i;
    }

    if ( -f $thisperl && $proto->_perl_is_same($thisperl) ) {
      return $thisperl;
    }
  }

  # We've tried all alternatives, and didn't find a perl that matches
  # our configuration. Throw an exception, and list alternatives we tried.
  my @paths = map File::Basename::dirname($_), @potential_perls;
  die "Can't locate the perl binary used to run this script " .
      "in (@paths)\n";
}

# Adapted from IPC::Cmd::can_run()
sub find_command {
  my ($self, $command) = @_;

  if( File::Spec->file_name_is_absolute($command) ) {
    return $self->_maybe_command($command);

  } else {
    for my $dir ( File::Spec->path ) {
      my $abs = File::Spec->catfile($dir, $command);
      return $abs if $abs = $self->_maybe_command($abs);
    }
  }
}

# Copied from ExtUtils::MM_Unix::maybe_command
sub _maybe_command {
  my($self,$file) = @_;
  return $file if -x $file && ! -d $file;
  return;
}

sub _is_interactive {
  return -t STDIN && (-t STDOUT || !(-f STDOUT || -c STDOUT)) ;   # Pipe?
}

# NOTE this is a blocking operation if(-t STDIN)
sub _is_unattended {
  my $self = shift;
  return $ENV{PERL_MM_USE_DEFAULT} ||
    ( !$self->_is_interactive && eof STDIN );
}

sub _readline {
  my $self = shift;
  return undef if $self->_is_unattended;

  my $answer = <STDIN>;
  chomp $answer if defined $answer;
  return $answer;
}

sub prompt {
  my $self = shift;
  my $mess = shift
    or die "prompt() called without a prompt message";

  # use a list to distinguish a default of undef() from no default
  my @def;
  @def = (shift) if @_;
  # use dispdef for output
  my @dispdef = scalar(@def) ?
    ('[', (defined($def[0]) ? $def[0] . ' ' : ''), ']') :
    (' ', '');

  local $|=1;
  print "$mess ", @dispdef;

  if ( $self->_is_unattended && !@def ) {
    die <<EOF;
ERROR: This build seems to be unattended, but there is no default value
for this question.  Aborting.
EOF
  }

  my $ans = $self->_readline();

  if ( !defined($ans)        # Ctrl-D or unattended
       or !length($ans) ) {  # User hit return
    print "$dispdef[1]\n";
    $ans = scalar(@def) ? $def[0] : '';
  }

  return $ans;
}

sub y_n {
  my $self = shift;
  my ($mess, $def)  = @_;

  die "y_n() called without a prompt message" unless $mess;
  die "Invalid default value: y_n() default must be 'y' or 'n'"
    if $def && $def !~ /^[yn]/i;

  my $answer;
  while (1) { # XXX Infinite or a large number followed by an exception ?
    $answer = $self->prompt(@_);
    return 1 if $answer =~ /^y/i;
    return 0 if $answer =~ /^n/i;
    local $|=1;
    print "Please answer 'y' or 'n'.\n";
  }
}

sub current_action { shift->{action} }
sub invoked_action { shift->{invoked_action} }

sub notes        { shift()->{phash}{notes}->access(@_) }
sub config_data  { shift()->{phash}{config_data}->access(@_) }
sub runtime_params { shift->{phash}{runtime_params}->read( @_ ? shift : () ) }  # Read-only
sub auto_features  { shift()->{phash}{auto_features}->access(@_) }

sub features     {
  my $self = shift;
  my $ph = $self->{phash};

  if (@_) {
    my $key = shift;
    if ($ph->{features}->exists($key)) {
      return $ph->{features}->access($key, @_);
    }

    if (my $info = $ph->{auto_features}->access($key)) {
      my $disabled;
      for my $type ( @{$self->prereq_action_types} ) {
        next if $type eq 'description' || $type eq 'recommends' || ! exists $info->{$type};
        my $prereqs = $info->{$type};
        for my $modname ( sort keys %$prereqs ) {
          my $spec = $prereqs->{$modname};
          my $status = $self->check_installed_status($modname, $spec);
          if ((!$status->{ok}) xor ($type =~ /conflicts$/)) { return 0; }
          if ( ! eval "require $modname; 1" ) { return 0; }
        }
      }
      return 1;
    }

    return $ph->{features}->access($key, @_);
  }

  # No args - get the auto_features & overlay the regular features
  my %features;
  my %auto_features = $ph->{auto_features}->access();
  while (my ($name, $info) = each %auto_features) {
    my $failures = $self->prereq_failures($info);
    my $disabled = grep( /^(?:\w+_)?(?:requires|conflicts)$/,
                        keys %$failures ) ? 1 : 0;
    $features{$name} = $disabled ? 0 : 1;
  }
  %features = (%features, $ph->{features}->access());

  return wantarray ? %features : \%features;
}
BEGIN { *feature = \&features } # Alias

sub _mb_feature {
  my $self = shift;

  if (($self->module_name || '') eq 'Module::Build') {
    # We're building Module::Build itself, so ...::ConfigData isn't
    # valid, but $self->features() should be.
    return $self->feature(@_);
  } else {
    require Module::Build::ConfigData;
    return Module::Build::ConfigData->feature(@_);
  }
}

sub _warn_mb_feature_deps {
  my $self = shift;
  my $name = shift;
  $self->log_warn(
    "The '$name' feature is not available.  Please install missing\n" .
    "feature dependencies and try again.\n".
    $self->_feature_deps_msg($name) . "\n"
  );
}

sub add_build_element {
    my ($self, $elem) = @_;
    my $elems = $self->build_elements;
    push @$elems, $elem unless grep { $_ eq $elem } @$elems;
}

sub ACTION_config_data {
  my $self = shift;
  return unless $self->has_config_data;

  my $module_name = $self->module_name
    or die "The config_data feature requires that 'module_name' be set";
  my $notes_name = $module_name . '::ConfigData'; # TODO: Customize name ???
  my $notes_pm = File::Spec->catfile($self->blib, 'lib', split /::/, "$notes_name.pm");

  return if $self->up_to_date(['Build.PL',
                               $self->config_file('config_data'),
                               $self->config_file('features')
                              ], $notes_pm);

  $self->log_verbose("Writing config notes to $notes_pm\n");
  File::Path::mkpath(File::Basename::dirname($notes_pm));

  Module::Build::Notes->write_config_data
    (
     file => $notes_pm,
     module => $module_name,
     config_module => $notes_name,
     config_data => scalar $self->config_data,
     feature => scalar $self->{phash}{features}->access(),
     auto_features => scalar $self->auto_features,
    );
}

########################################################################
{ # enclosing these lexicals -- TODO
  my %valid_properties = ( __PACKAGE__,  {} );
  my %additive_properties;

  sub _mb_classes {
    my $class = ref($_[0]) || $_[0];
    return ($class, $class->mb_parents);
  }

  sub valid_property {
    my ($class, $prop) = @_;
    return grep exists( $valid_properties{$_}{$prop} ), $class->_mb_classes;
  }

  sub valid_properties {
    return keys %{ shift->valid_properties_defaults() };
  }

  sub valid_properties_defaults {
    my %out;
    for my $class (reverse shift->_mb_classes) {
      @out{ keys %{ $valid_properties{$class} } } = map {
        $_->()
      } values %{ $valid_properties{$class} };
    }
    return \%out;
  }

  sub array_properties {
    map { exists $additive_properties{$_}->{ARRAY} ? @{$additive_properties{$_}->{ARRAY}} : () } shift->_mb_classes;
  }

  sub hash_properties {
    map { exists $additive_properties{$_}->{HASH} ? @{$additive_properties{$_}->{HASH}} : () } shift->_mb_classes;
  }

  sub add_property {
    my ($class, $property) = (shift, shift);
    die "Property '$property' already exists"
      if $class->valid_property($property);
    my %p = @_ == 1 ? ( default => shift ) : @_;

    my $type = ref $p{default};
    $valid_properties{$class}{$property} =
      $type eq 'CODE' ? $p{default}                           :
      $type eq 'HASH' ? sub { return { %{ $p{default} } }   } :
      $type eq 'ARRAY'? sub { return [ @{ $p{default} } ]   } :
                        sub { return $p{default}            } ;

    push @{$additive_properties{$class}->{$type}}, $property
      if $type;

    unless ($class->can($property)) {
      # TODO probably should put these in a util package
      my $sub = $type eq 'HASH'
        ? _make_hash_accessor($property, \%p)
        : _make_accessor($property, \%p);
      no strict 'refs';
      *{"$class\::$property"} = $sub;
    }

    return $class;
  }

  sub property_error {
    my $self = shift;
    die 'ERROR: ', @_;
  }

  sub _set_defaults {
    my $self = shift;

    # Set the build class.
    $self->{properties}{build_class} ||= ref $self;

    # If there was no orig_dir, set to the same as base_dir
    $self->{properties}{orig_dir} ||= $self->{properties}{base_dir};

    my $defaults = $self->valid_properties_defaults;

    foreach my $prop (keys %$defaults) {
      $self->{properties}{$prop} = $defaults->{$prop}
        unless exists $self->{properties}{$prop};
    }

    # Copy defaults for arrays any arrays.
    for my $prop ($self->array_properties) {
      $self->{properties}{$prop} = [@{$defaults->{$prop}}]
        unless exists $self->{properties}{$prop};
    }
    # Copy defaults for arrays any hashes.
    for my $prop ($self->hash_properties) {
      $self->{properties}{$prop} = {%{$defaults->{$prop}}}
        unless exists $self->{properties}{$prop};
    }
  }

} # end enclosure
########################################################################
sub _make_hash_accessor {
  my ($property, $p) = @_;
  my $check = $p->{check} || sub { 1 };

  return sub {
    my $self = shift;

    # This is only here to deprecate the historic accident of calling
    # properties as class methods - I suspect it only happens in our
    # test suite.
    unless(ref($self)) {
      carp("\n$property not a class method (@_)");
      return;
    }

    my $x = $self->{properties};
    return $x->{$property} unless @_;

    my $prop = $x->{$property};
    if ( defined $_[0] && !ref $_[0] ) {
      if ( @_ == 1 ) {
        return exists $prop->{$_[0]} ? $prop->{$_[0]} : undef;
      } elsif ( @_ % 2 == 0 ) {
        my %new = (%{ $prop }, @_);
        local $_ = \%new;
        $x->{$property} = \%new if $check->($self);
        return $x->{$property};
      } else {
        die "Unexpected arguments for property '$property'\n";
      }
    } else {
      die "Unexpected arguments for property '$property'\n"
          if defined $_[0] && ref $_[0] ne 'HASH';
      local $_ = $_[0];
      $x->{$property} = shift if $check->($self);
    }
  };
}
########################################################################
sub _make_accessor {
  my ($property, $p) = @_;
  my $check = $p->{check} || sub { 1 };

  return sub {
    my $self = shift;

    # This is only here to deprecate the historic accident of calling
    # properties as class methods - I suspect it only happens in our
    # test suite.
    unless(ref($self)) {
      carp("\n$property not a class method (@_)");
      return;
    }

    my $x = $self->{properties};
    return $x->{$property} unless @_;
    local $_ = $_[0];
    $x->{$property} = shift if $check->($self);
    return $x->{$property};
  };
}
########################################################################

# Add the default properties.
__PACKAGE__->add_property(auto_configure_requires => 1);
__PACKAGE__->add_property(blib => 'blib');
__PACKAGE__->add_property(build_class => 'Module::Build');
__PACKAGE__->add_property(build_elements => [qw(PL support pm xs share_dir pod script)]);
__PACKAGE__->add_property(build_script => 'Build');
__PACKAGE__->add_property(build_bat => 0);
__PACKAGE__->add_property(bundle_inc => []);
__PACKAGE__->add_property(bundle_inc_preload => []);
__PACKAGE__->add_property(config_dir => '_build');
__PACKAGE__->add_property(dynamic_config => 1);
__PACKAGE__->add_property(include_dirs => []);
__PACKAGE__->add_property(license => 'unknown');
__PACKAGE__->add_property(metafile => 'META.yml');
__PACKAGE__->add_property(mymetafile => 'MYMETA.yml');
__PACKAGE__->add_property(metafile2 => 'META.json');
__PACKAGE__->add_property(mymetafile2 => 'MYMETA.json');
__PACKAGE__->add_property(recurse_into => []);
__PACKAGE__->add_property(use_rcfile => 1);
__PACKAGE__->add_property(create_packlist => 1);
__PACKAGE__->add_property(allow_mb_mismatch => 0);
__PACKAGE__->add_property(config => undef);
__PACKAGE__->add_property(test_file_exts => ['.t']);
__PACKAGE__->add_property(use_tap_harness => 0);
__PACKAGE__->add_property(cpan_client => 'cpan');
__PACKAGE__->add_property(tap_harness_args => {});
__PACKAGE__->add_property(pureperl_only => 0);
__PACKAGE__->add_property(allow_pureperl => 0);
__PACKAGE__->add_property(
  'installdirs',
  default => 'site',
  check   => sub {
    return 1 if /^(core|site|vendor)$/;
    return shift->property_error(
      $_ eq 'perl'
      ? 'Perhaps you meant installdirs to be "core" rather than "perl"?'
      : 'installdirs must be one of "core", "site", or "vendor"'
    );
    return shift->property_error("Perhaps you meant 'core'?") if $_ eq 'perl';
    return 0;
  },
);

{
  __PACKAGE__->add_property(html_css => '');
}

{
  my @prereq_action_types = qw(requires build_requires test_requires conflicts recommends);
  foreach my $type (@prereq_action_types) {
    __PACKAGE__->add_property($type => {});
  }
  __PACKAGE__->add_property(prereq_action_types => \@prereq_action_types);
}

__PACKAGE__->add_property($_ => {}) for qw(
  get_options
  install_base_relpaths
  install_path
  install_sets
  meta_add
  meta_merge
  original_prefix
  prefix_relpaths
  configure_requires
);

__PACKAGE__->add_property($_) for qw(
  PL_files
  autosplit
  base_dir
  bindoc_dirs
  c_source
  create_license
  create_makefile_pl
  create_readme
  debugger
  destdir
  dist_abstract
  dist_author
  dist_name
  dist_suffix
  dist_version
  dist_version_from
  extra_compiler_flags
  extra_linker_flags
  has_config_data
  install_base
  libdoc_dirs
  magic_number
  mb_version
  module_name
  needs_compiler
  orig_dir
  perl
  pm_files
  pod_files
  pollute
  prefix
  program_name
  quiet
  recursive_test_files
  release_status
  script_files
  scripts
  share_dir
  sign
  test_files
  verbose
  debug
  xs_files
  extra_manify_args
);

sub config {
  my $self = shift;
  my $c = ref($self) ? $self->{config} : 'Module::Build::Config';
  return $c->all_config unless @_;

  my $key = shift;
  return $c->get($key) unless @_;

  my $val = shift;
  return $c->set($key => $val);
}

sub mb_parents {
    # Code borrowed from Class::ISA.
    my @in_stack = (shift);
    my %seen = ($in_stack[0] => 1);

    my ($current, @out);
    while (@in_stack) {
        next unless defined($current = shift @in_stack)
          && $current->isa('Module::Build::Base');
        push @out, $current;
        next if $current eq 'Module::Build::Base';
        no strict 'refs';
        unshift @in_stack,
          map {
              my $c = $_; # copy, to avoid being destructive
              substr($c,0,2) = "main::" if substr($c,0,2) eq '::';
              # Canonize the :: -> main::, ::foo -> main::foo thing.
              # Should I ever canonize the Foo'Bar = Foo::Bar thing?
              $seen{$c}++ ? () : $c;
          } @{"$current\::ISA"};

        # I.e., if this class has any parents (at least, ones I've never seen
        # before), push them, in order, onto the stack of classes I need to
        # explore.
    }
    shift @out;
    return @out;
}

sub extra_linker_flags   { shift->_list_accessor('extra_linker_flags',   @_) }
sub extra_compiler_flags { shift->_list_accessor('extra_compiler_flags', @_) }

sub _list_accessor {
  (my $self, local $_) = (shift, shift);
  my $p = $self->{properties};
  $p->{$_} = [@_] if @_;
  $p->{$_} = [] unless exists $p->{$_};
  return ref($p->{$_}) ? $p->{$_} : [$p->{$_}];
}

# XXX Problem - if Module::Build is loaded from a different directory,
# it'll look for (and perhaps destroy/create) a _build directory.
sub subclass {
  my ($pack, %opts) = @_;

  my $build_dir = '_build'; # XXX The _build directory is ostensibly settable by the user.  Shouldn't hard-code here.
  $pack->delete_filetree($build_dir) if -e $build_dir;

  die "Must provide 'code' or 'class' option to subclass()\n"
    unless $opts{code} or $opts{class};

  $opts{code}  ||= '';
  $opts{class} ||= 'MyModuleBuilder';

  my $filename = File::Spec->catfile($build_dir, 'lib', split '::', $opts{class}) . '.pm';
  my $filedir  = File::Basename::dirname($filename);
  $pack->log_verbose("Creating custom builder $filename in $filedir\n");

  File::Path::mkpath($filedir);
  die "Can't create directory $filedir: $!" unless -d $filedir;

  open(my $fh, '>', $filename) or die "Can't create $filename: $!";
  print $fh <<EOF;
package $opts{class};
use $pack;
\@ISA = qw($pack);
$opts{code}
1;
EOF
  close $fh;

  unshift @INC, File::Spec->catdir(File::Spec->rel2abs($build_dir), 'lib');
  eval "use $opts{class}";
  die $@ if $@;

  return $opts{class};
}

sub _guess_module_name {
  my $self = shift;
  my $p = $self->{properties};
  return if $p->{module_name};
  if ( $p->{dist_version_from} && -e $p->{dist_version_from} ) {
    my $mi = Module::Build::ModuleInfo->new_from_file($self->dist_version_from);
    $p->{module_name} = $mi->name;
  }
  else {
    my $mod_path = my $mod_name = $p->{dist_name};
    $mod_name =~ s{-}{::}g;
    $mod_path =~ s{-}{/}g;
    $mod_path .= ".pm";
    if ( -e $mod_path || -e "lib/$mod_path" ) {
      $p->{module_name} = $mod_name;
    }
    else {
      $self->log_warn( << 'END_WARN' );
No 'module_name' was provided and it could not be inferred
from other properties.  This will prevent a packlist from
being written for this file.  Please set either 'module_name'
or 'dist_version_from' in Build.PL.
END_WARN
    }
  }
}

sub dist_name {
  my $self = shift;
  my $p = $self->{properties};
  my $me = 'dist_name';
  return $p->{$me} if defined $p->{$me};

  die "Can't determine distribution name, must supply either 'dist_name' or 'module_name' parameter"
    unless $self->module_name;

  ($p->{$me} = $self->module_name) =~ s/::/-/g;

  return $p->{$me};
}

sub release_status {
  my ($self) = @_;
  my $me = 'release_status';
  my $p = $self->{properties};

  if ( ! defined $p->{$me} ) {
    $p->{$me} = $self->_is_dev_version ? 'testing' : 'stable';
  }

  unless ( $p->{$me} =~ qr/\A(?:stable|testing|unstable)\z/ ) {
    die "Illegal value '$p->{$me}' for $me\n";
  }

  if ( $p->{$me} eq 'stable' && $self->_is_dev_version ) {
    my $version = $self->dist_version;
    die "Illegal value '$p->{$me}' with version '$version'\n";
  }
  return $p->{$me};
}

sub dist_suffix {
  my ($self) = @_;
  my $p = $self->{properties};
  my $me = 'dist_suffix';

  return $p->{$me} if defined $p->{$me};

  if ( $self->release_status eq 'stable' ) {
    $p->{$me} = "";
  }
  else {
    # non-stable release but non-dev version number needs '-TRIAL' appended
    $p->{$me} = $self->_is_dev_version ? "" : "TRIAL" ;
  }

  return $p->{$me};
}

sub dist_version_from {
  my ($self) = @_;
  my $p = $self->{properties};
  my $me = 'dist_version_from';

  if ($self->module_name) {
    $p->{$me} ||=
      join( '/', 'lib', split(/::/, $self->module_name) ) . '.pm';
  }
  return $p->{$me} || undef;
}

sub dist_version {
  my ($self) = @_;
  my $p = $self->{properties};
  my $me = 'dist_version';

  return $p->{$me} if defined $p->{$me};

  if ( my $dist_version_from = $self->dist_version_from ) {
    my $version_from = File::Spec->catfile( split( qr{/}, $dist_version_from ) );
    my $pm_info = Module::Build::ModuleInfo->new_from_file( $version_from )
      or die "Can't find file $version_from to determine version";
    #$p->{$me} is undef here
    $p->{$me} = $self->normalize_version( $pm_info->version() );
    unless (defined $p->{$me}) {
      die "Can't determine distribution version from $version_from";
    }
  }

  die ("Can't determine distribution version, must supply either 'dist_version',\n".
       "'dist_version_from', or 'module_name' parameter")
    unless defined $p->{$me};

  return $p->{$me};
}

sub _is_dev_version {
  my ($self) = @_;
  my $dist_version = $self->dist_version;
  my $version_obj = eval { Module::Build::Version->new( $dist_version ) };
  # assume it's normal if the version string is fatal -- in this case
  # the author might be doing something weird so should play along and
  # assume they'll specify all necessary behavior
  return $@ ? 0 : $version_obj->is_alpha;
}

sub dist_author   { shift->_pod_parse('author')   }
sub dist_abstract { shift->_pod_parse('abstract') }

sub _pod_parse {
  my ($self, $part) = @_;
  my $p = $self->{properties};
  my $member = "dist_$part";
  return $p->{$member} if defined $p->{$member};

  my $docfile = $self->_main_docfile
    or return;
  open(my $fh, '<', $docfile)
    or return;

  require Module::Build::PodParser;
  my $parser = Module::Build::PodParser->new(fh => $fh);
  my $method = "get_$part";
  return $p->{$member} = $parser->$method();
}

sub version_from_file { # Method provided for backwards compatibility
  return Module::Build::ModuleInfo->new_from_file($_[1])->version();
}

sub find_module_by_name { # Method provided for backwards compatibility
  return Module::Build::ModuleInfo->find_module_by_name(@_[1,2]);
}

{
  # $unlink_list_for_pid{$$} = [ ... ]
  my %unlink_list_for_pid;

  sub _unlink_on_exit {
    my $self = shift;
    for my $f ( @_ ) {
      push @{$unlink_list_for_pid{$$}}, $f if -f $f;
    }
    return 1;
  }

  END {
    for my $f ( map glob($_), @{ $unlink_list_for_pid{$$} || [] } ) {
      next unless -e $f;
      File::Path::rmtree($f, 0, 0);
    }
  }
}

sub add_to_cleanup {
  my $self = shift;
  my %files = map {$self->localize_file_path($_), 1} @_;
  $self->{phash}{cleanup}->write(\%files);
}

sub cleanup {
  my $self = shift;
  my $all = $self->{phash}{cleanup}->read;
  return keys %$all;
}

sub config_file {
  my $self = shift;
  return unless -d $self->config_dir;
  return File::Spec->catfile($self->config_dir, @_);
}

sub read_config {
  my ($self) = @_;

  my $file = $self->config_file('build_params')
    or die "Can't find 'build_params' in " . $self->config_dir;
  open(my $fh, '<', $file) or die "Can't read '$file': $!";
  my $ref = eval do {local $/; <$fh>};
  die if $@;
  close $fh;
  my $c;
  ($self->{args}, $c, $self->{properties}) = @$ref;
  $self->{config} = Module::Build::Config->new(values => $c);
}

sub has_config_data {
  my $self = shift;
  return scalar grep $self->{phash}{$_}->has_data(), qw(config_data features auto_features);
}

sub _write_data {
  my ($self, $filename, $data) = @_;

  my $file = $self->config_file($filename);
  open(my $fh, '>', $file) or die "Can't create '$file': $!";
  unless (ref($data)) {  # e.g. magicnum
    print $fh $data;
    return;
  }

  print {$fh} Module::Build::Dumper->_data_dump($data);
  close $fh;
}

sub write_config {
  my ($self) = @_;

  File::Path::mkpath($self->{properties}{config_dir});
  -d $self->{properties}{config_dir} or die "Can't mkdir $self->{properties}{config_dir}: $!";

  my @items = @{ $self->prereq_action_types };
  $self->_write_data('prereqs', { map { $_, $self->$_() } @items });
  $self->_write_data('build_params', [$self->{args}, $self->{config}->values_set, $self->{properties}]);

  # Set a new magic number and write it to a file
  $self->_write_data('magicnum', $self->magic_number(int rand 1_000_000));

  $self->{phash}{$_}->write() foreach qw(notes cleanup features auto_features config_data runtime_params);
}

{
  # packfile map -- keys are guts of regular expressions;  If they match,
  # values are module names corresponding to the packlist
  my %packlist_map = (
    '^File::Spec'         => 'Cwd',
    '^Devel::AssertOS'    => 'Devel::CheckOS',
  );

  sub _find_packlist {
    my ($self, $inst, $mod) = @_;
    my $lookup = $mod;
    my $packlist = eval { $inst->packlist($lookup) };
    if ( ! $packlist ) {
      # try from packlist_map
      while ( my ($re, $new_mod) = each %packlist_map ) {
        if ( $mod =~ qr/$re/ ) {
          $lookup = $new_mod;
          $packlist = eval { $inst->packlist($lookup) };
          last;
        }
      }
    }
    return $packlist ? $lookup : undef;
  }

  sub set_bundle_inc {
    my $self = shift;

    my $bundle_inc = $self->{properties}{bundle_inc};
    my $bundle_inc_preload = $self->{properties}{bundle_inc_preload};
    # We're in author mode if inc::latest is loaded, but not from cwd
    return unless inc::latest->can('loaded_modules');
    require ExtUtils::Installed;
    # ExtUtils::Installed is buggy about finding additions to default @INC
    my $inst = eval { ExtUtils::Installed->new(extra_libs => [@INC]) };
    if ($@) {
      $self->log_warn( << "EUI_ERROR" );
Bundling in inc/ is disabled because ExtUtils::Installed could not
create a list of your installed modules.  Here is the error:
$@
EUI_ERROR
      return;
    }
    my @bundle_list = map { [ $_, 0 ] } inc::latest->loaded_modules;

    # XXX TODO: Need to get ordering of prerequisites correct so they are
    # are loaded in the right order. Use an actual tree?!

    while( @bundle_list ) {
      my ($mod, $prereq) = @{ shift @bundle_list };

      # XXX TODO: Append prereqs to list
      # skip if core or already in bundle or preload lists
      # push @bundle_list, [$_, 1] for prereqs()

      # Locate packlist for bundling
      my $lookup = $self->_find_packlist($inst,$mod);
      if ( ! $lookup ) {
        # XXX Really needs a more helpful error message here
        die << "NO_PACKLIST";
Could not find a packlist for '$mod'.  If it's a core module, try
force installing it from CPAN.
NO_PACKLIST
      }
      else {
        push @{ $prereq ? $bundle_inc_preload : $bundle_inc }, $lookup;
      }
    }
  } # sub check_bundling
}

sub check_autofeatures {
  my ($self) = @_;
  my $features = $self->auto_features;

  return 1 unless %$features;

  # TODO refactor into ::Util
  my $longest = sub {
    my @str = @_ or croak("no strings given");

    my @len = map({length($_)} @str);
    my $max = 0;
    my $longest;
    for my $i (0..$#len) {
      ($max, $longest) = ($len[$i], $str[$i]) if($len[$i] > $max);
    }
    return($longest);
  };
  my $max_name_len = length($longest->(keys %$features));

  my ($num_disabled, $log_text) = (0, "\nChecking optional features...\n");
  for my $name ( sort keys %$features ) {
    $log_text .= $self->_feature_deps_msg($name, $max_name_len);
  }

  $num_disabled = () = $log_text =~ /disabled/g;

  # warn user if features disabled
  if ( $num_disabled ) {
    $self->log_warn( $log_text );
    return 0;
  }
  else {
    $self->log_verbose( $log_text );
    return 1;
  }
}

sub _feature_deps_msg {
  my ($self, $name, $max_name_len) = @_;
    $max_name_len ||= length $name;
    my $features = $self->auto_features;
    my $info = $features->{$name};
    my $feature_text = "$name" . '.' x ($max_name_len - length($name) + 4);

    my ($log_text, $disabled) = ('','');
    if ( my $failures = $self->prereq_failures($info) ) {
      $disabled = grep( /^(?:\w+_)?(?:requires|conflicts)$/,
                  keys %$failures ) ? 1 : 0;
      $feature_text .= $disabled ? "disabled\n" : "enabled\n";

      for my $type ( @{ $self->prereq_action_types } ) {
        next unless exists $failures->{$type};
        $feature_text .= "  $type:\n";
        my $prereqs = $failures->{$type};
        for my $module ( sort keys %$prereqs ) {
          my $status = $prereqs->{$module};
          my $required =
            ($type =~ /^(?:\w+_)?(?:requires|conflicts)$/) ? 1 : 0;
          my $prefix = ($required) ? '!' : '*';
          $feature_text .= "    $prefix $status->{message}\n";
        }
      }
    } else {
      $feature_text .= "enabled\n";
    }
    $log_text .= $feature_text if $disabled || $self->verbose;
    return $log_text;
}

# Automatically detect configure_requires prereqs
sub auto_config_requires {
  my ($self) = @_;
  my $p = $self->{properties};

  # add current Module::Build to configure_requires if there
  # isn't one already specified (but not ourself, so we're not circular)
  if ( $self->dist_name ne 'Module-Build'
    && $self->auto_configure_requires
    && ! exists $p->{configure_requires}{'Module::Build'}
  ) {
    (my $ver = $VERSION) =~ s/^(\d+\.\d\d).*$/$1/; # last major release only
    $self->log_warn(<<EOM);
Module::Build was not found in configure_requires! Adding it now
automatically as: configure_requires => { 'Module::Build' => $ver }
EOM
    $self->_add_prereq('configure_requires', 'Module::Build', $ver);
  }

  # if we're in author mode, add inc::latest modules to
  # configure_requires if not already set.  If we're not in author mode
  # then configure_requires will have been satisfied, or we'll just
  # live with what we've bundled
  if ( inc::latest->can('loaded_module') ) {
    for my $mod ( inc::latest->loaded_modules ) {
      next if exists $p->{configure_requires}{$mod};
      $self->_add_prereq('configure_requires', $mod, $mod->VERSION);
    }
  }

  return;
}

# Automatically detect and add prerequisites based on configuration
sub auto_require {
  my ($self) = @_;
  my $p = $self->{properties};

  # If needs_compiler is not explicitly set, automatically set it
  # If set, we need ExtUtils::CBuilder (and a compiler)
  my $xs_files = $self->find_xs_files;
  if ( ! defined $p->{needs_compiler} ) {
    $self->needs_compiler( keys %$xs_files || defined $self->c_source );
  }
  if ($self->needs_compiler) {
    $self->_add_prereq('build_requires', 'ExtUtils::CBuilder', 0);
    if ( ! $self->have_c_compiler ) {
      $self->log_warn(<<'EOM');
Warning: ExtUtils::CBuilder not installed or no compiler detected
Proceeding with configuration, but compilation may fail during Build

EOM
    }
  }

  # If using share_dir, require File::ShareDir
  if ( $self->share_dir ) {
    $self->_add_prereq( 'requires', 'File::ShareDir', '1.00' );
  }

  return;
}

sub _add_prereq {
  my ($self, $type, $module, $version) = @_;
  my $p = $self->{properties};
  $version = 0 unless defined $version;
  if ( exists $p->{$type}{$module} ) {
    return if $self->compare_versions( $version, '<=', $p->{$type}{$module} );
  }
  $self->log_verbose("Adding to $type\: $module => $version\n");
  $p->{$type}{$module} = $version;
  return 1;
}

sub prereq_failures {
  my ($self, $info) = @_;

  my @types = @{ $self->prereq_action_types };
  $info ||= {map {$_, $self->$_()} @types};

  my $out;

  foreach my $type (@types) {
    my $prereqs = $info->{$type};
    for my $modname ( keys %$prereqs ) {
      my $spec = $prereqs->{$modname};
      my $status = $self->check_installed_status($modname, $spec);

      if ($type =~ /^(?:\w+_)?conflicts$/) {
        next if !$status->{ok};
        $status->{conflicts} = delete $status->{need};
        $status->{message} = "$modname ($status->{have}) conflicts with this distribution";

      } elsif ($type =~ /^(?:\w+_)?recommends$/) {
        next if $status->{ok};
        $status->{message} = (!ref($status->{have}) && $status->{have} eq '<none>'
                              ? "$modname is not installed"
                              : "$modname ($status->{have}) is installed, but we prefer to have $spec");
      } else {
        next if $status->{ok};
      }

      $out->{$type}{$modname} = $status;
    }
  }

  return $out;
}

# returns a hash of defined prerequisites; i.e. only prereq types with values
sub _enum_prereqs {
  my $self = shift;
  my %prereqs;
  foreach my $type ( @{ $self->prereq_action_types } ) {
    if ( $self->can( $type ) ) {
      my $prereq = $self->$type() || {};
      $prereqs{$type} = $prereq if %$prereq;
    }
  }
  return \%prereqs;
}

sub check_prereq {
  my $self = shift;

  # Check to see if there are any prereqs to check
  my $info = $self->_enum_prereqs;
  return 1 unless $info;

  my $log_text = "Checking prerequisites...\n";

  my $failures = $self->prereq_failures($info);

  if ( $failures ) {
    $self->log_warn($log_text);
    for my $type ( @{ $self->prereq_action_types } ) {
      my $prereqs = $failures->{$type};
      $self->log_warn("  ${type}:\n") if keys %$prereqs;
      for my $module ( sort keys %$prereqs ) {
        my $status = $prereqs->{$module};
        my $prefix = ($type =~ /^(?:\w+_)?recommends$/) ? "* " : "! ";
        $self->log_warn("    $prefix $status->{message}\n");
      }
    }
    return 0;
  } else {
    $self->log_verbose($log_text . "Looks good\n\n");
    return 1;
  }
}

sub perl_version {
  my ($self) = @_;
  # Check the current perl interpreter
  # It's much more convenient to use $] here than $^V, but 'man
  # perlvar' says I'm not supposed to.  Bloody tyrant.
  return $^V ? $self->perl_version_to_float(sprintf "%vd", $^V) : $];
}

sub perl_version_to_float {
  my ($self, $version) = @_;
  return $version if grep( /\./, $version ) < 2;
  $version =~ s/\./../;
  $version =~ s/\.(\d+)/sprintf '%03d', $1/eg;
  return $version;
}

sub _parse_conditions {
  my ($self, $spec) = @_;

  return ">= 0" if not defined $spec;
  if ($spec =~ /^\s*([\w.]+)\s*$/) { # A plain number, maybe with dots, letters, and underscores
    return (">= $spec");
  } else {
    return split /\s*,\s*/, $spec;
  }
}

sub try_require {
  my ($self, $modname, $spec) = @_;
  my $status = $self->check_installed_status($modname, defined($spec) ? $spec : 0);
  return unless $status->{ok};
  my $path = $modname;
  $path =~ s{::}{/}g;
  $path .= ".pm";
  if ( defined $INC{$path} ) {
    return 1;
  }
  elsif ( exists $INC{$path} ) { # failed before, don't try again
    return;
  }
  else {
    return eval "require $modname";
  }
}

sub check_installed_status {
  my ($self, $modname, $spec) = @_;
  my %status = (need => $spec);

  if ($modname eq 'perl') {
    $status{have} = $self->perl_version;

  } elsif (eval { no strict; $status{have} = ${"${modname}::VERSION"} }) {
    # Don't try to load if it's already loaded

  } else {
    my $pm_info = Module::Build::ModuleInfo->new_from_module( $modname );
    unless (defined( $pm_info )) {
      @status{ qw(have message) } = ('<none>', "$modname is not installed");
      return \%status;
    }

    $status{have} = eval { $pm_info->version() };
    if ($spec and !defined($status{have})) {
      @status{ qw(have message) } = (undef, "Couldn't find a \$VERSION in prerequisite $modname");
      return \%status;
    }
  }

  my @conditions = $self->_parse_conditions($spec);

  foreach (@conditions) {
    my ($op, $version) = /^\s*  (<=?|>=?|==|!=)  \s*  ([\w.]+)  \s*$/x
      or die "Invalid prerequisite condition '$_' for $modname";

    $version = $self->perl_version_to_float($version)
      if $modname eq 'perl';

    next if $op eq '>=' and !$version;  # Module doesn't have to actually define a $VERSION

    unless ($self->compare_versions( $status{have}, $op, $version )) {
      $status{message} = "$modname ($status{have}) is installed, but we need version $op $version";
      return \%status;
    }
  }

  $status{ok} = 1;
  return \%status;
}

sub compare_versions {
  my $self = shift;
  my ($v1, $op, $v2) = @_;
  $v1 = Module::Build::Version->new($v1)
    unless UNIVERSAL::isa($v1,'Module::Build::Version');

  my $eval_str = "\$v1 $op \$v2";
  my $result   = eval $eval_str;
  $self->log_warn("error comparing versions: '$eval_str' $@") if $@;

  return $result;
}

# I wish I could set $! to a string, but I can't, so I use $@
sub check_installed_version {
  my ($self, $modname, $spec) = @_;

  my $status = $self->check_installed_status($modname, $spec);

  if ($status->{ok}) {
    return $status->{have} if $status->{have} and "$status->{have}" ne '<none>';
    return '0 but true';
  }

  $@ = $status->{message};
  return 0;
}

sub make_executable {
  # Perl's chmod() is mapped to useful things on various non-Unix
  # platforms, so we use it in the base class even though it looks
  # Unixish.

  my $self = shift;
  foreach (@_) {
    my $current_mode = (stat $_)[2];
    chmod $current_mode | oct(111), $_;
  }
}

sub is_executable {
  # We assume this does the right thing on generic platforms, though
  # we do some other more specific stuff on Unixish platforms.
  my ($self, $file) = @_;
  return -x $file;
}

sub _startperl { shift()->config('startperl') }

# Return any directories in @INC which are not in the default @INC for
# this perl.  For example, stuff passed in with -I or loaded with "use lib".
sub _added_to_INC {
  my $self = shift;

  my %seen;
  $seen{$_}++ foreach $self->_default_INC;
  return grep !$seen{$_}++, @INC;
}

# Determine the default @INC for this Perl
{
  my @default_inc; # Memoize
  sub _default_INC {
    my $self = shift;
    return @default_inc if @default_inc;

    local $ENV{PERL5LIB};  # this is not considered part of the default.

    my $perl = ref($self) ? $self->perl : $self->find_perl_interpreter;

    my @inc = $self->_backticks($perl, '-le', 'print for @INC');
    chomp @inc;

    return @default_inc = @inc;
  }
}

sub print_build_script {
  my ($self, $fh) = @_;

  my $build_package = $self->build_class;

  my $closedata="";

  my $config_requires;
  if ( -f $self->metafile ) {
    my $meta = eval { $self->read_metafile( $self->metafile ) };
    $config_requires = $meta && $meta->{configure_requires}{'Module::Build'};
  }
  $config_requires ||= 0;

  my %q = map {$_, $self->$_()} qw(config_dir base_dir);

  $q{base_dir} = Win32::GetShortPathName($q{base_dir}) if $self->is_windowsish;

  $q{magic_numfile} = $self->config_file('magicnum');

  my @myINC = $self->_added_to_INC;
  for (@myINC, values %q) {
    $_ = File::Spec->canonpath( $_ ) unless $self->is_vmsish;
    s/([\\\'])/\\$1/g;
  }

  my $quoted_INC = join ",\n", map "     '$_'", @myINC;
  my $shebang = $self->_startperl;
  my $magic_number = $self->magic_number;

  print $fh <<EOF;
$shebang

use strict;
use Cwd;
use File::Basename;
use File::Spec;

sub magic_number_matches {
  return 0 unless -e '$q{magic_numfile}';
  my \$FH;
  open \$FH, '<','$q{magic_numfile}' or return 0;
  my \$filenum = <\$FH>;
  close \$FH;
  return \$filenum == $magic_number;
}

my \$progname;
my \$orig_dir;
BEGIN {
  \$^W = 1;  # Use warnings
  \$progname = basename(\$0);
  \$orig_dir = Cwd::cwd();
  my \$base_dir = '$q{base_dir}';
  if (!magic_number_matches()) {
    unless (chdir(\$base_dir)) {
      die ("Couldn't chdir(\$base_dir), aborting\\n");
    }
    unless (magic_number_matches()) {
      die ("Configuration seems to be out of date, please re-run 'perl Build.PL' again.\\n");
    }
  }
  unshift \@INC,
    (
$quoted_INC
    );
}

close(*DATA) unless eof(*DATA); # ensure no open handles to this script

use $build_package;
Module::Build->VERSION(q{$config_requires});

# Some platforms have problems setting \$^X in shebang contexts, fix it up here
\$^X = Module::Build->find_perl_interpreter;

if (-e 'Build.PL' and not $build_package->up_to_date('Build.PL', \$progname)) {
   warn "Warning: Build.PL has been altered.  You may need to run 'perl Build.PL' again.\\n";
}

# This should have just enough arguments to be able to bootstrap the rest.
my \$build = $build_package->resume (
  properties => {
    config_dir => '$q{config_dir}',
    orig_dir => \$orig_dir,
  },
);

\$build->dispatch;
EOF
}

sub create_mymeta {
  my ($self) = @_;

  my ($meta_obj, $mymeta);
  my @metafiles = ( $self->metafile2, $self->metafile,  );
  my @mymetafiles = ( $self->mymetafile2, $self->mymetafile, );

  # cleanup old MYMETA
  for my $f ( @mymetafiles ) {
    if ( $self->delete_filetree($f) ) {
      $self->log_verbose("Removed previous '$f'\n");
    }
  }

  # Try loading META.json or META.yml
  if ( $self->try_require("CPAN::Meta", "2.110420") ) {
    for my $file ( @metafiles ) {
      next unless -f $file;
      $meta_obj = eval { CPAN::Meta->load_file($file, { lazy_validation => 0 }) };
      last if $meta_obj;
    }
  }

  # maybe get a copy in spec v2 format (regardless of original source)

  my $mymeta_obj;
  if ($meta_obj) {
    # if we have metadata, just update it
    my %updated = (
      %{ $meta_obj->as_struct({ version => 2.0 }) },
      prereqs => $self->_normalize_prereqs,
      dynamic_config => 0,
      generated_by => "Module::Build version $Module::Build::VERSION",
    );
    $mymeta_obj = CPAN::Meta->new( \%updated, { lazy_validation => 0 } );
  }
  else {
    $mymeta_obj = $self->_get_meta_object(quiet => 0, dynamic => 0, fatal => 1, auto => 0);
  }

  my @created = $self->_write_meta_files( $mymeta_obj, 'MYMETA' );

  $self->log_warn("Could not create MYMETA files\n")
    unless @created;

  return 1;
}

sub create_build_script {
  my ($self) = @_;

  $self->write_config;
  $self->create_mymeta;

  # Create Build
  my ($build_script, $dist_name, $dist_version)
    = map $self->$_(), qw(build_script dist_name dist_version);

  if ( $self->delete_filetree($build_script) ) {
    $self->log_verbose("Removed previous script '$build_script'\n");
  }

  $self->log_info("Creating new '$build_script' script for ",
                  "'$dist_name' version '$dist_version'\n");
  open(my $fh, '>', $build_script) or die "Can't create '$build_script': $!";
  $self->print_build_script($fh);
  close $fh;

  $self->make_executable($build_script);

  return 1;
}

sub check_manifest {
  my $self = shift;
  return unless -e 'MANIFEST';

  # Stolen nearly verbatim from MakeMaker.  But ExtUtils::Manifest
  # could easily be re-written into a modern Perl dialect.

  require ExtUtils::Manifest;  # ExtUtils::Manifest is not warnings clean.
  local ($^W, $ExtUtils::Manifest::Quiet) = (0,1);

  $self->log_verbose("Checking whether your kit is complete...\n");
  if (my @missed = ExtUtils::Manifest::manicheck()) {
    $self->log_warn("WARNING: the following files are missing in your kit:\n",
                    "\t", join("\n\t", @missed), "\n",
                    "Please inform the author.\n\n");
  } else {
    $self->log_verbose("Looks good\n\n");
  }
}

sub dispatch {
  my $self = shift;
  local $self->{_completed_actions} = {};

  if (@_) {
    my ($action, %p) = @_;
    my $args = $p{args} ? delete($p{args}) : {};

    local $self->{invoked_action} = $action;
    local $self->{args} = {%{$self->{args}}, %$args};
    local $self->{properties} = {%{$self->{properties}}, %p};
    return $self->_call_action($action);
  }

  die "No build action specified" unless $self->{action};
  local $self->{invoked_action} = $self->{action};
  $self->_call_action($self->{action});
}

sub _call_action {
  my ($self, $action) = @_;

  return if $self->{_completed_actions}{$action}++;

  local $self->{action} = $action;
  my $method = $self->can_action( $action );
  die "No action '$action' defined, try running the 'help' action.\n" unless $method;
  $self->log_debug("Starting ACTION_$action\n");
  my $rc = $self->$method();
  $self->log_debug("Finished ACTION_$action\n");
  return $rc;
}

sub can_action {
  my ($self, $action) = @_;
  return $self->can( "ACTION_$action" );
}

# cuts the user-specified options out of the command-line args
sub cull_options {
    my $self = shift;
    my (@argv) = @_;

    # XXX is it even valid to call this as a class method?
    return({}, @argv) unless(ref($self)); # no object

    my $specs = $self->get_options;
    return({}, @argv) unless($specs and %$specs); # no user options

    require Getopt::Long;
    # XXX Should we let Getopt::Long handle M::B's options? That would
    # be easy-ish to add to @specs right here, but wouldn't handle options
    # passed without "--" as M::B currently allows. We might be able to
    # get around this by setting the "prefix_pattern" Configure option.
    my @specs;
    my $args = {};
    # Construct the specifications for GetOptions.
    while (my ($k, $v) = each %$specs) {
        # Throw an error if specs conflict with our own.
        die "Option specification '$k' conflicts with a " . ref $self
          . " option of the same name"
          if $self->valid_property($k);
        push @specs, $k . (defined $v->{type} ? $v->{type} : '');
        push @specs, $v->{store} if exists $v->{store};
        $args->{$k} = $v->{default} if exists $v->{default};
    }

    local @ARGV = @argv; # No other way to dupe Getopt::Long

    # Get the options values and return them.
    # XXX Add option to allow users to set options?
    if ( @specs ) {
      Getopt::Long::Configure('pass_through');
      Getopt::Long::GetOptions($args, @specs);
    }

    return $args, @ARGV;
}

sub unparse_args {
  my ($self, $args) = @_;
  my @out;
  while (my ($k, $v) = each %$args) {
    push @out, (UNIVERSAL::isa($v, 'HASH')  ? map {+"--$k", "$_=$v->{$_}"} keys %$v :
                UNIVERSAL::isa($v, 'ARRAY') ? map {+"--$k", $_} @$v :
                ("--$k", $v));
  }
  return @out;
}

sub args {
    my $self = shift;
    return wantarray ? %{ $self->{args} } : $self->{args} unless @_;
    my $key = shift;
    $self->{args}{$key} = shift if @_;
    return $self->{args}{$key};
}

# allows select parameters (with underscores) to be spoken with dashes
# when used as command-line options
sub _translate_option {
  my $self = shift;
  my $opt  = shift;

  (my $tr_opt = $opt) =~ tr/-/_/;

  return $tr_opt if grep $tr_opt =~ /^(?:no_?)?$_$/, qw(
    create_license
    create_makefile_pl
    create_readme
    extra_compiler_flags
    extra_linker_flags
    install_base
    install_path
    meta_add
    meta_merge
    test_files
    use_rcfile
    use_tap_harness
    tap_harness_args
    cpan_client
    pureperl_only
    allow_pureperl
  ); # normalize only selected option names

  return $opt;
}

my %singular_argument = map { ($_ => 1) } qw/install_base prefix destdir installdir verbose quiet uninst debug sign/;

sub _read_arg {
  my ($self, $args, $key, $val) = @_;

  $key = $self->_translate_option($key);

  if ( exists $args->{$key} and not $singular_argument{$key} ) {
    $args->{$key} = [ $args->{$key} ] unless ref $args->{$key};
    push @{$args->{$key}}, $val;
  } else {
    $args->{$key} = $val;
  }
}

# decide whether or not an option requires/has an operand
sub _optional_arg {
  my $self = shift;
  my $opt  = shift;
  my $argv = shift;

  $opt = $self->_translate_option($opt);

  my @bool_opts = qw(
    build_bat
    create_license
    create_readme
    pollute
    quiet
    uninst
    use_rcfile
    verbose
    debug
    sign
    use_tap_harness
    pureperl_only
    allow_pureperl
  );

  # inverted boolean options; eg --noverbose or --no-verbose
  # converted to proper name & returned with false value (verbose, 0)
  if ( grep $opt =~ /^no[-_]?$_$/, @bool_opts ) {
    $opt =~ s/^no-?//;
    return ($opt, 0);
  }

  # non-boolean option; return option unchanged along with its argument
  return ($opt, shift(@$argv)) unless grep $_ eq $opt, @bool_opts;

  # we're punting a bit here, if an option appears followed by a digit
  # we take the digit as the argument for the option. If there is
  # nothing that looks like a digit, we pretend the option is a flag
  # that is being set and has no argument.
  my $arg = 1;
  $arg = shift(@$argv) if @$argv && $argv->[0] =~ /^\d+$/;

  return ($opt, $arg);
}

sub read_args {
  my $self = shift;

  (my $args, @_) = $self->cull_options(@_);
  my %args = %$args;

  my $opt_re = qr/[\w\-]+/;

  my ($action, @argv);
  while (@_) {
    local $_ = shift;
    if ( /^(?:--)?($opt_re)=(.*)$/ ) {
      $self->_read_arg(\%args, $1, $2);
    } elsif ( /^--($opt_re)$/ ) {
      my($opt, $arg) = $self->_optional_arg($1, \@_);
      $self->_read_arg(\%args, $opt, $arg);
    } elsif ( /^($opt_re)$/ and !defined($action)) {
      $action = $1;
    } else {
      push @argv, $_;
    }
  }
  $args{ARGV} = \@argv;

  for ('extra_compiler_flags', 'extra_linker_flags') {
    $args{$_} = [ $self->split_like_shell($args{$_}) ] if exists $args{$_};
  }

  # Convert to arrays
  for ('include_dirs') {
    $args{$_} = [ $args{$_} ] if exists $args{$_} && !ref $args{$_}
  }

  # Hashify these parameters
  for ($self->hash_properties, 'config') {
    next unless exists $args{$_};
    my %hash;
    $args{$_} ||= [];
    $args{$_} = [ $args{$_} ] unless ref $args{$_};
    foreach my $arg ( @{$args{$_}} ) {
      $arg =~ /($opt_re)=(.*)/
        or die "Malformed '$_' argument: '$arg' should be something like 'foo=bar'";
      $hash{$1} = $2;
    }
    $args{$_} = \%hash;
  }

  # De-tilde-ify any path parameters
  for my $key (qw(prefix install_base destdir)) {
    next if !defined $args{$key};
    $args{$key} = $self->_detildefy($args{$key});
  }

  for my $key (qw(install_path)) {
    next if !defined $args{$key};

    for my $subkey (keys %{$args{$key}}) {
      next if !defined $args{$key}{$subkey};
      my $subkey_ext = $self->_detildefy($args{$key}{$subkey});
      if ( $subkey eq 'html' ) { # translate for compatibility
        $args{$key}{binhtml} = $subkey_ext;
        $args{$key}{libhtml} = $subkey_ext;
      } else {
        $args{$key}{$subkey} = $subkey_ext;
      }
    }
  }

  if ($args{makefile_env_macros}) {
    require Module::Build::Compat;
    %args = (%args, Module::Build::Compat->makefile_to_build_macros);
  }

  return \%args, $action;
}

# Default: do nothing.  Overridden for Unix & Windows.
sub _detildefy {}


# merge Module::Build argument lists that have already been parsed
# by read_args(). Takes two references to option hashes and merges
# the contents, giving priority to the first.
sub _merge_arglist {
  my( $self, $opts1, $opts2 ) = @_;

  $opts1 ||= {};
  $opts2 ||= {};
  my %new_opts = %$opts1;
  while (my ($key, $val) = each %$opts2) {
    if ( exists( $opts1->{$key} ) ) {
      if ( ref( $val ) eq 'HASH' ) {
        while (my ($k, $v) = each %$val) {
          $new_opts{$key}{$k} = $v unless exists( $opts1->{$key}{$k} );
        }
      }
    } else {
      $new_opts{$key} = $val
    }
  }

  return %new_opts;
}

# Look for a home directory on various systems.
sub _home_dir {
  my @home_dirs;
  push( @home_dirs, $ENV{HOME} ) if $ENV{HOME};

  push( @home_dirs, File::Spec->catpath($ENV{HOMEDRIVE}, $ENV{HOMEPATH}, '') )
      if $ENV{HOMEDRIVE} && $ENV{HOMEPATH};

  my @other_home_envs = qw( USERPROFILE APPDATA WINDIR SYS$LOGIN );
  push( @home_dirs, map $ENV{$_}, grep $ENV{$_}, @other_home_envs );

  my @real_home_dirs = grep -d, @home_dirs;

  return wantarray ? @real_home_dirs : shift( @real_home_dirs );
}

sub _find_user_config {
  my $self = shift;
  my $file = shift;
  foreach my $dir ( $self->_home_dir ) {
    my $path = File::Spec->catfile( $dir, $file );
    return $path if -e $path;
  }
  return undef;
}

# read ~/.modulebuildrc returning global options '*' and
# options specific to the currently executing $action.
sub read_modulebuildrc {
  my( $self, $action ) = @_;

  return () unless $self->use_rcfile;

  my $modulebuildrc;
  if ( exists($ENV{MODULEBUILDRC}) && $ENV{MODULEBUILDRC} eq 'NONE' ) {
    return ();
  } elsif ( exists($ENV{MODULEBUILDRC}) && -e $ENV{MODULEBUILDRC} ) {
    $modulebuildrc = $ENV{MODULEBUILDRC};
  } elsif ( exists($ENV{MODULEBUILDRC}) ) {
    $self->log_warn("WARNING: Can't find resource file " .
                    "'$ENV{MODULEBUILDRC}' defined in environment.\n" .
                    "No options loaded\n");
    return ();
  } else {
    $modulebuildrc = $self->_find_user_config( '.modulebuildrc' );
    return () unless $modulebuildrc;
  }

  open(my $fh, '<', $modulebuildrc )
      or die "Can't open $modulebuildrc: $!";

  my %options; my $buffer = '';
  while (defined( my $line = <$fh> )) {
    chomp( $line );
    $line =~ s/#.*$//;
    next unless length( $line );

    if ( $line =~ /^\S/ ) {
      if ( $buffer ) {
        my( $action, $options ) = split( /\s+/, $buffer, 2 );
        $options{$action} .= $options . ' ';
        $buffer = '';
      }
      $buffer = $line;
    } else {
      $buffer .= $line;
    }
  }

  if ( $buffer ) { # anything left in $buffer ?
    my( $action, $options ) = split( /\s+/, $buffer, 2 );
    $options{$action} .= $options . ' '; # merge if more than one line
  }

  my ($global_opts) =
    $self->read_args( $self->split_like_shell( $options{'*'} || '' ) );

  # let fakeinstall act like install if not provided
  if ( $action eq 'fakeinstall' && ! exists $options{fakeinstall} ) {
    $action = 'install';
  }
  my ($action_opts) =
    $self->read_args( $self->split_like_shell( $options{$action} || '' ) );

  # specific $action options take priority over global options '*'
  return $self->_merge_arglist( $action_opts, $global_opts );
}

# merge the relevant options in ~/.modulebuildrc into Module::Build's
# option list where they do not conflict with commandline options.
sub merge_modulebuildrc {
  my( $self, $action, %cmdline_opts ) = @_;
  my %rc_opts = $self->read_modulebuildrc( $action || $self->{action} || 'build' );
  my %new_opts = $self->_merge_arglist( \%cmdline_opts, \%rc_opts );
  $self->merge_args( $action, %new_opts );
}

sub merge_args {
  my ($self, $action, %args) = @_;
  $self->{action} = $action if defined $action;

  my %additive = map { $_ => 1 } $self->hash_properties;

  # Extract our 'properties' from $cmd_args, the rest are put in 'args'.
  while (my ($key, $val) = each %args) {
    $self->{phash}{runtime_params}->access( $key => $val )
      if $self->valid_property($key);

    if ($key eq 'config') {
      $self->config($_ => $val->{$_}) foreach keys %$val;
    } else {
      my $add_to = $additive{$key}             ? $self->{properties}{$key} :
                   $self->valid_property($key) ? $self->{properties}       :
                   $self->{args}               ;

      if ($additive{$key}) {
        $add_to->{$_} = $val->{$_} foreach keys %$val;
      } else {
        $add_to->{$key} = $val;
      }
    }
  }
}

sub cull_args {
  my $self = shift;
  my @arg_list = @_;
  unshift @arg_list, $self->split_like_shell($ENV{PERL_MB_OPT})
    if $ENV{PERL_MB_OPT};
  my ($args, $action) = $self->read_args(@arg_list);
  $self->merge_args($action, %$args);
  $self->merge_modulebuildrc( $action, %$args );
}

sub super_classes {
  my ($self, $class, $seen) = @_;
  $class ||= ref($self) || $self;
  $seen  ||= {};

  no strict 'refs';
  my @super = grep {not $seen->{$_}++} $class, @{ $class . '::ISA' };
  return @super, map {$self->super_classes($_,$seen)} @super;
}

sub known_actions {
  my ($self) = @_;

  my %actions;
  no strict 'refs';

  foreach my $class ($self->super_classes) {
    foreach ( keys %{ $class . '::' } ) {
      $actions{$1}++ if /^ACTION_(\w+)/;
    }
  }

  return wantarray ? sort keys %actions : \%actions;
}

sub get_action_docs {
  my ($self, $action) = @_;
  my $actions = $self->known_actions;
  die "No known action '$action'" unless $actions->{$action};

  my ($files_found, @docs) = (0);
  foreach my $class ($self->super_classes) {
    (my $file = $class) =~ s{::}{/}g;
    # NOTE: silently skipping relative paths if any chdir() happened
    $file = $INC{$file . '.pm'} or next;
    open(my $fh, '<', $file) or next;
    $files_found++;

    # Code below modified from /usr/bin/perldoc

    # Skip to ACTIONS section
    local $_;
    while (<$fh>) {
      last if /^=head1 ACTIONS\s/;
    }

    # Look for our action and determine the style
    my $style;
    while (<$fh>) {
      last if /^=head1 /;

      # only item and head2 are allowed (3&4 are not in 5.005)
      if(/^=(item|head2)\s+\Q$action\E\b/) {
        $style = $1;
        push @docs, $_;
        last;
      }
    }
    $style or next; # not here

    # and the content
    if($style eq 'item') {
      my ($found, $inlist) = (0, 0);
      while (<$fh>) {
        if (/^=(item|back)/) {
          last unless $inlist;
        }
        push @docs, $_;
        ++$inlist if /^=over/;
        --$inlist if /^=back/;
      }
    }
    else { # head2 style
      # stop at anything equal or greater than the found level
      while (<$fh>) {
        last if(/^=(?:head[12]|cut)/);
        push @docs, $_;
      }
    }
    # TODO maybe disallow overriding just pod for an action
    # TODO and possibly: @docs and last;
  }

  unless ($files_found) {
    $@ = "Couldn't find any documentation to search";
    return;
  }
  unless (@docs) {
    $@ = "Couldn't find any docs for action '$action'";
    return;
  }

  return join '', @docs;
}

sub ACTION_prereq_report {
  my $self = shift;
  $self->log_info( $self->prereq_report );
}

sub ACTION_prereq_data {
  my $self = shift;
  $self->log_info( Module::Build::Dumper->_data_dump( $self->prereq_data ) );
}

sub prereq_data {
  my $self = shift;
  my @types = ('configure_requires', @{ $self->prereq_action_types } );
  my $info = { map { $_ => $self->$_() } grep { %{$self->$_()} } @types };
  return $info;
}

sub prereq_report {
  my $self = shift;
  my $info = $self->prereq_data;

  my $output = '';
  foreach my $type (keys %$info) {
    my $prereqs = $info->{$type};
    $output .= "\n$type:\n";
    my $mod_len = 2;
    my $ver_len = 4;
    my %mods;
    while ( my ($modname, $spec) = each %$prereqs ) {
      my $len  = length $modname;
      $mod_len = $len if $len > $mod_len;
      $spec    ||= '0';
      $len     = length $spec;
      $ver_len = $len if $len > $ver_len;

      my $mod = $self->check_installed_status($modname, $spec);
      $mod->{name} = $modname;
      $mod->{ok} ||= 0;
      $mod->{ok} = ! $mod->{ok} if $type =~ /^(\w+_)?conflicts$/;

      $mods{lc $modname} = $mod;
    }

    my $space  = q{ } x ($mod_len - 3);
    my $vspace = q{ } x ($ver_len - 3);
    my $sline  = q{-} x ($mod_len - 3);
    my $vline  = q{-} x ($ver_len - 3);
    my $disposition = ($type =~ /^(\w+_)?conflicts$/) ?
                        'Clash' : 'Need';
    $output .=
      "    Module $space  $disposition $vspace  Have\n".
      "    ------$sline+------$vline-+----------\n";


    for my $k (sort keys %mods) {
      my $mod = $mods{$k};
      my $space  = q{ } x ($mod_len - length $k);
      my $vspace = q{ } x ($ver_len - length $mod->{need});
      my $f = $mod->{ok} ? ' ' : '!';
      $output .=
        "  $f $mod->{name} $space     $mod->{need}  $vspace   ".
        (defined($mod->{have}) ? $mod->{have} : "")."\n";
    }
  }
  return $output;
}

sub ACTION_help {
  my ($self) = @_;
  my $actions = $self->known_actions;

  if (@{$self->{args}{ARGV}}) {
    my $msg = eval {$self->get_action_docs($self->{args}{ARGV}[0], $actions)};
    print $@ ? "$@\n" : $msg;
    return;
  }

  print <<EOF;

 Usage: $0 <action> --arg1=value --arg2=value ...
 Example: $0 test --verbose=1

 Actions defined:
EOF

  print $self->_action_listing($actions);

  print "\nRun `Build help <action>` for details on an individual action.\n";
  print "See `perldoc Module::Build` for complete documentation.\n";
}

sub _action_listing {
  my ($self, $actions) = @_;

  # Flow down columns, not across rows
  my @actions = sort keys %$actions;
  @actions = map $actions[($_ + ($_ % 2) * @actions) / 2],  0..$#actions;

  my $out = '';
  while (my ($one, $two) = splice @actions, 0, 2) {
    $out .= sprintf("  %-12s                   %-12s\n", $one, $two||'');
  }
  $out =~ s{\s*$}{}mg; # remove trailing spaces
  return $out;
}

sub ACTION_retest {
  my ($self) = @_;

  # Protect others against our @INC changes
  local @INC = @INC;

  # Filter out nonsensical @INC entries - some versions of
  # Test::Harness will really explode the number of entries here
  @INC = grep {ref() || -d} @INC if @INC > 100;

  $self->do_tests;
}

sub ACTION_testall {
  my ($self) = @_;

  my @types;
  for my $action (grep { $_ ne 'all' } $self->get_test_types) {
    # XXX We can't just dispatch because we get multiple summaries but
    # we'll need to dispatch to support custom setup/teardown in the
    # action.  To support that, we'll need to call something besides
    # Harness::runtests() because we'll need to collect the results in
    # parts, then run the summary.
    push(@types, $action);
    #$self->_call_action( "test$action" );
  }
  $self->generic_test(types => ['default', @types]);
}

sub get_test_types {
  my ($self) = @_;

  my $t = $self->{properties}->{test_types};
  return ( defined $t ? ( keys %$t ) : () );
}


sub ACTION_test {
  my ($self) = @_;
  $self->generic_test(type => 'default');
}

sub generic_test {
  my $self = shift;
  (@_ % 2) and croak('Odd number of elements in argument hash');
  my %args = @_;

  my $p = $self->{properties};

  my @types = (
    (exists($args{type})  ? $args{type} : ()),
    (exists($args{types}) ? @{$args{types}} : ()),
  );
  @types or croak "need some types of tests to check";

  my %test_types = (
    default => $p->{test_file_exts},
    (defined($p->{test_types}) ? %{$p->{test_types}} : ()),
  );

  for my $type (@types) {
    croak "$type not defined in test_types!"
      unless defined $test_types{ $type };
  }

  # we use local here because it ends up two method calls deep
  local $p->{test_file_exts} = [ map { ref $_ ? @$_ : $_ } @test_types{@types} ];
  $self->depends_on('code');

  # Protect others against our @INC changes
  local @INC = @INC;

  # Make sure we test the module in blib/
  unshift @INC, (File::Spec->catdir($p->{base_dir}, $self->blib, 'lib'),
                 File::Spec->catdir($p->{base_dir}, $self->blib, 'arch'));

  # Filter out nonsensical @INC entries - some versions of
  # Test::Harness will really explode the number of entries here
  @INC = grep {ref() || -d} @INC if @INC > 100;

  $self->do_tests;
}

# Test::Harness dies on failure but TAP::Harness does not, so we must
# die if running under TAP::Harness
sub do_tests {
  my $self = shift;

  my $tests = $self->find_test_files;

  local $ENV{PERL_DL_NONLAZY} = 1;

  if(@$tests) {
    my $args = $self->tap_harness_args;
    if($self->use_tap_harness or ($args and %$args)) {
      my $aggregate = $self->run_tap_harness($tests);
      if ( $aggregate->has_errors ) {
        die "Errors in testing.  Cannot continue.\n";
      }
    }
    else {
      $self->run_test_harness($tests);
    }
  }
  else {
    $self->log_info("No tests defined.\n");
  }

  $self->run_visual_script;
}

sub run_tap_harness {
  my ($self, $tests) = @_;

  require TAP::Harness;

  # TODO allow the test @INC to be set via our API?

  my $aggregate = TAP::Harness->new({
    lib => [@INC],
    verbosity => $self->{properties}{verbose},
    switches  => [ $self->harness_switches ],
    %{ $self->tap_harness_args },
  })->runtests(@$tests);

  return $aggregate;
}

sub run_test_harness {
    my ($self, $tests) = @_;
    require Test::Harness;

    local $Test::Harness::verbose = $self->verbose || 0;
    local $Test::Harness::switches = join ' ', $self->harness_switches;

    Test::Harness::runtests(@$tests);
}

sub run_visual_script {
    my $self = shift;
    # This will get run and the user will see the output.  It doesn't
    # emit Test::Harness-style output.
    $self->run_perl_script('visual.pl', '-Mblib='.$self->blib)
        if -e 'visual.pl';
}

sub harness_switches {
    shift->{properties}{debugger} ? qw(-w -d) : ();
}

sub test_files {
  my $self = shift;
  my $p = $self->{properties};
  if (@_) {
    return $p->{test_files} = (@_ == 1 ? shift : [@_]);
  }
  return $self->find_test_files;
}

sub expand_test_dir {
  my ($self, $dir) = @_;
  my $exts = $self->{properties}{test_file_exts};

  return sort map { @{$self->rscan_dir($dir, qr{^[^.].*\Q$_\E$})} } @$exts
    if $self->recursive_test_files;

  return sort map { glob File::Spec->catfile($dir, "*$_") } @$exts;
}

sub ACTION_testdb {
  my ($self) = @_;
  local $self->{properties}{debugger} = 1;
  $self->depends_on('test');
}

sub ACTION_testcover {
  my ($self) = @_;

  unless (Module::Build::ModuleInfo->find_module_by_name('Devel::Cover')) {
    warn("Cannot run testcover action unless Devel::Cover is installed.\n");
    return;
  }

  $self->add_to_cleanup('coverage', 'cover_db');
  $self->depends_on('code');

  # See whether any of the *.pm files have changed since last time
  # testcover was run.  If so, start over.
  if (-e 'cover_db') {
    my $pm_files = $self->rscan_dir
        (File::Spec->catdir($self->blib, 'lib'), $self->file_qr('\.pm$') );
    my $cover_files = $self->rscan_dir('cover_db', sub {-f $_ and not /\.html$/});

    $self->do_system(qw(cover -delete))
      unless $self->up_to_date($pm_files,         $cover_files)
          && $self->up_to_date($self->test_files, $cover_files);
  }

  local $Test::Harness::switches    =
  local $Test::Harness::Switches    =
  local $ENV{HARNESS_PERL_SWITCHES} = "-MDevel::Cover";

  $self->depends_on('test');
  $self->do_system('cover');
}

sub ACTION_code {
  my ($self) = @_;

  # All installable stuff gets created in blib/ .
  # Create blib/arch to keep blib.pm happy
  my $blib = $self->blib;
  $self->add_to_cleanup($blib);
  File::Path::mkpath( File::Spec->catdir($blib, 'arch') );

  if (my $split = $self->autosplit) {
    $self->autosplit_file($_, $blib) for ref($split) ? @$split : ($split);
  }

  foreach my $element (@{$self->build_elements}) {
    my $method = "process_${element}_files";
    $method = "process_files_by_extension" unless $self->can($method);
    $self->$method($element);
  }

  $self->depends_on('config_data');
}

sub ACTION_build {
  my $self = shift;
  $self->log_info("Building " . $self->dist_name . "\n");
  $self->depends_on('code');
  $self->depends_on('docs');
}

sub process_files_by_extension {
  my ($self, $ext) = @_;

  my $method = "find_${ext}_files";
  my $files = $self->can($method) ? $self->$method() : $self->_find_file_by_type($ext,  'lib');

  while (my ($file, $dest) = each %$files) {
    $self->copy_if_modified(from => $file, to => File::Spec->catfile($self->blib, $dest) );
  }
}

sub process_support_files {
  my $self = shift;
  my $p = $self->{properties};
  return unless $p->{c_source};

  my $files;
  if (ref($p->{c_source}) eq "ARRAY") {
      push @{$p->{include_dirs}}, @{$p->{c_source}};
      for my $path (@{$p->{c_source}}) {
          push @$files, @{ $self->rscan_dir($path, $self->file_qr('\.c(c|p|pp|xx|\+\+)?$')) };
      }
  } else {
      push @{$p->{include_dirs}}, $p->{c_source};
      $files = $self->rscan_dir($p->{c_source}, $self->file_qr('\.c(c|p|pp|xx|\+\+)?$'));
  }

  foreach my $file (@$files) {
      push @{$p->{objects}}, $self->compile_c($file);
  }
}

sub process_share_dir_files {
  my $self = shift;
  my $files = $self->_find_share_dir_files;
  return unless $files;

  # root for all File::ShareDir paths
  my $share_prefix = File::Spec->catdir($self->blib, qw/lib auto share/);

  # copy all share files to blib
  while (my ($file, $dest) = each %$files) {
    $self->copy_if_modified(
      from => $file, to => File::Spec->catfile( $share_prefix, $dest )
    );
  }
}

sub _find_share_dir_files {
  my $self = shift;
  my $share_dir = $self->share_dir;
  return unless $share_dir;

  my @file_map;
  if ( $share_dir->{dist} ) {
    my $prefix = "dist/".$self->dist_name;
    push @file_map, $self->_share_dir_map( $prefix, $share_dir->{dist} );
  }

  if ( $share_dir->{module} ) {
    for my $mod ( keys %{ $share_dir->{module} } ) {
      (my $altmod = $mod) =~ s{::}{-}g;
      my $prefix = "module/$altmod";
      push @file_map, $self->_share_dir_map($prefix, $share_dir->{module}{$mod});
    }
  }

  return { @file_map };
}

sub _share_dir_map {
  my ($self, $prefix, $list) = @_;
  my %files;
  for my $dir ( @$list ) {
    for my $f ( @{ $self->rscan_dir( $dir, sub {-f} )} ) {
      $f =~ s{\A.*?\Q$dir\E/}{};
      $files{"$dir/$f"} = "$prefix/$f";
    }
  }
  return %files;
}

sub process_PL_files {
  my ($self) = @_;
  my $files = $self->find_PL_files;

  while (my ($file, $to) = each %$files) {
    unless ($self->up_to_date( $file, $to )) {
      $self->run_perl_script($file, [], [@$to]) or die "$file failed";
      $self->add_to_cleanup(@$to);
    }
  }
}

sub process_xs_files {
  my $self = shift;
  return if $self->pureperl_only && $self->allow_pureperl;
  my $files = $self->find_xs_files;
  croak 'Can\'t build xs files under --pureperl-only' if %$files && $self->pureperl_only;
  while (my ($from, $to) = each %$files) {
    unless ($from eq $to) {
      $self->add_to_cleanup($to);
      $self->copy_if_modified( from => $from, to => $to );
    }
    $self->process_xs($to);
  }
}

sub process_pod_files { shift()->process_files_by_extension(shift()) }
sub process_pm_files  { shift()->process_files_by_extension(shift()) }

sub process_script_files {
  my $self = shift;
  my $files = $self->find_script_files;
  return unless keys %$files;

  my $script_dir = File::Spec->catdir($self->blib, 'script');
  File::Path::mkpath( $script_dir );

  foreach my $file (keys %$files) {
    my $result = $self->copy_if_modified($file, $script_dir, 'flatten') or next;
    $self->fix_shebang_line($result) unless $self->is_vmsish;
    $self->make_executable($result);
  }
}

sub find_PL_files {
  my $self = shift;
  if (my $files = $self->{properties}{PL_files}) {
    # 'PL_files' is given as a Unix file spec, so we localize_file_path().

    if (UNIVERSAL::isa($files, 'ARRAY')) {
      return { map {$_, [/^(.*)\.PL$/]}
               map $self->localize_file_path($_),
               @$files };

    } elsif (UNIVERSAL::isa($files, 'HASH')) {
      my %out;
      while (my ($file, $to) = each %$files) {
        $out{ $self->localize_file_path($file) } = [ map $self->localize_file_path($_),
                                                     ref $to ? @$to : ($to) ];
      }
      return \%out;

    } else {
      die "'PL_files' must be a hash reference or array reference";
    }
  }

  return unless -d 'lib';
  return {
    map {$_, [/^(.*)\.PL$/i ]}
    @{ $self->rscan_dir('lib', $self->file_qr('\.PL$')) }
  };
}

sub find_pm_files  { shift->_find_file_by_type('pm',  'lib') }
sub find_pod_files { shift->_find_file_by_type('pod', 'lib') }
sub find_xs_files  { shift->_find_file_by_type('xs',  'lib') }

sub find_script_files {
  my $self = shift;
  if (my $files = $self->script_files) {
    # Always given as a Unix file spec.  Values in the hash are
    # meaningless, but we preserve if present.
    return { map {$self->localize_file_path($_), $files->{$_}} keys %$files };
  }

  # No default location for script files
  return {};
}

sub find_test_files {
  my $self = shift;
  my $p = $self->{properties};

  if (my $files = $p->{test_files}) {
    $files = [keys %$files] if UNIVERSAL::isa($files, 'HASH');
    $files = [map { -d $_ ? $self->expand_test_dir($_) : $_ }
              map glob,
              $self->split_like_shell($files)];

    # Always given as a Unix file spec.
    return [ map $self->localize_file_path($_), @$files ];

  } else {
    # Find all possible tests in t/ or test.pl
    my @tests;
    push @tests, 'test.pl'                          if -e 'test.pl';
    push @tests, $self->expand_test_dir('t')        if -e 't' and -d _;
    return \@tests;
  }
}

sub _find_file_by_type {
  my ($self, $type, $dir) = @_;

  if (my $files = $self->{properties}{"${type}_files"}) {
    # Always given as a Unix file spec
    return { map $self->localize_file_path($_), %$files };
  }

  return {} unless -d $dir;
  return { map {$_, $_}
           map $self->localize_file_path($_),
           grep !/\.\#/,
           @{ $self->rscan_dir($dir, $self->file_qr("\\.$type\$")) } };
}

sub localize_file_path {
  my ($self, $path) = @_;
  return File::Spec->catfile( split m{/}, $path );
}

sub localize_dir_path {
  my ($self, $path) = @_;
  return File::Spec->catdir( split m{/}, $path );
}

sub fix_shebang_line { # Adapted from fixin() in ExtUtils::MM_Unix 1.35
  my ($self, @files) = @_;
  my $c = ref($self) ? $self->{config} : 'Module::Build::Config';

  my ($does_shbang) = $c->get('sharpbang') =~ /^\s*\#\!/;
  for my $file (@files) {
    open(my $FIXIN, '<', $file) or die "Can't process '$file': $!";
    local $/ = "\n";
    chomp(my $line = <$FIXIN>);
    next unless $line =~ s/^\s*\#!\s*//;     # Not a shebang file.

    my ($cmd, $arg) = (split(' ', $line, 2), '');
    next unless $cmd =~ /perl/i;
    my $interpreter = $self->{properties}{perl};

    $self->log_verbose("Changing sharpbang in $file to $interpreter\n");
    my $shb = '';
    $shb .= $c->get('sharpbang')."$interpreter $arg\n" if $does_shbang;

    # I'm not smart enough to know the ramifications of changing the
    # embedded newlines here to \n, so I leave 'em in.
    $shb .= qq{
eval 'exec $interpreter $arg -S \$0 \${1+"\$\@"}'
    if 0; # not running under some shell
} unless $self->is_windowsish; # this won't work on win32, so don't

    open(my $FIXOUT, '>', "$file.new")
      or die "Can't create new $file: $!\n";

    # Print out the new #! line (or equivalent).
    local $\;
    undef $/; # Was localized above
    print $FIXOUT $shb, <$FIXIN>;
    close $FIXIN;
    close $FIXOUT;

    rename($file, "$file.bak")
      or die "Can't rename $file to $file.bak: $!";

    rename("$file.new", $file)
      or die "Can't rename $file.new to $file: $!";

    $self->delete_filetree("$file.bak")
      or $self->log_warn("Couldn't clean up $file.bak, leaving it there");

    $self->do_system($c->get('eunicefix'), $file) if $c->get('eunicefix') ne ':';
  }
}


sub ACTION_testpod {
  my $self = shift;
  $self->depends_on('docs');

  eval q{use Test::Pod 0.95; 1}
    or die "The 'testpod' action requires Test::Pod version 0.95";

  my @files = sort keys %{$self->_find_pods($self->libdoc_dirs)},
                   keys %{$self->_find_pods
                             ($self->bindoc_dirs,
                              exclude => [ $self->file_qr('\.bat$') ])}
    or die "Couldn't find any POD files to test\n";

  { package # hide from PAUSE
      Module::Build::PodTester;  # Don't want to pollute the main namespace
    Test::Pod->import( tests => scalar @files );
    pod_file_ok($_) foreach @files;
  }
}

sub ACTION_testpodcoverage {
  my $self = shift;

  $self->depends_on('docs');

  eval q{use Test::Pod::Coverage 1.00; 1}
    or die "The 'testpodcoverage' action requires ",
           "Test::Pod::Coverage version 1.00";

  # TODO this needs test coverage!

  # XXX work-around a bug in Test::Pod::Coverage previous to v1.09
  # Make sure we test the module in blib/
  local @INC = @INC;
  my $p = $self->{properties};
  unshift(@INC,
    # XXX any reason to include arch?
    File::Spec->catdir($p->{base_dir}, $self->blib, 'lib'),
    #File::Spec->catdir($p->{base_dir}, $self->blib, 'arch')
  );

  all_pod_coverage_ok();
}

sub ACTION_docs {
  my $self = shift;

  $self->depends_on('code');
  $self->depends_on('manpages', 'html');
}

# Given a file type, will return true if the file type would normally
# be installed when neither install-base nor prefix has been set.
# I.e. it will be true only if the path is set from Config.pm or
# set explicitly by the user via install-path.
sub _is_default_installable {
  my $self = shift;
  my $type = shift;
  return ( $self->install_destination($type) &&
           ( $self->install_path($type) ||
             $self->install_sets($self->installdirs)->{$type} )
         ) ? 1 : 0;
}

sub _is_ActivePerl {
#  return 0;
  my $self = shift;
  unless (exists($self->{_is_ActivePerl})) {
    $self->{_is_ActivePerl} = (eval { require ActivePerl::DocTools; } || 0);
  }
  return $self->{_is_ActivePerl};
}

sub _is_ActivePPM {
#  return 0;
  my $self = shift;
  unless (exists($self->{_is_ActivePPM})) {
    $self->{_is_ActivePPM} = (eval { require ActivePerl::PPM; } || 0);
  }
  return $self->{_is_ActivePPM};
}

sub ACTION_manpages {
  my $self = shift;

  return unless $self->_mb_feature('manpage_support');

  $self->depends_on('code');

  my %extra_manify_args = $self->{properties}{'extra_manify_args'} ? %{ $self->{properties}{'extra_manify_args'} } : ();

  foreach my $type ( qw(bin lib) ) {
    next unless ( $self->invoked_action eq 'manpages' || $self->_is_default_installable("${type}doc"));
    my $files = $self->_find_pods( $self->{properties}{"${type}doc_dirs"},
                                   exclude => [ $self->file_qr('\.bat$') ] );
    next unless %$files;

    my $sub = $self->can("manify_${type}_pods");
    $self->$sub( %extra_manify_args ) if defined( $sub );
  }
}

sub manify_bin_pods {
  my $self    = shift;
  my %podman_args = (section =>  1, @_); # binaries go in section 1

  my $files   = $self->_find_pods( $self->{properties}{bindoc_dirs},
                                   exclude => [ $self->file_qr('\.bat$') ] );
  return unless keys %$files;

  my $mandir = File::Spec->catdir( $self->blib, 'bindoc' );
  File::Path::mkpath( $mandir, 0, oct(777) );

  require Pod::Man;
  foreach my $file (keys %$files) {
    # Pod::Simple based parsers only support one document per instance.
    # This is expected to change in a future version (Pod::Simple > 3.03).
    my $parser  = Pod::Man->new( %podman_args );
    my $manpage = $self->man1page_name( $file ) . '.' .
                  $self->config( 'man1ext' );
    my $outfile = File::Spec->catfile($mandir, $manpage);
    next if $self->up_to_date( $file, $outfile );
    $self->log_verbose("Manifying $file -> $outfile\n");
    eval { $parser->parse_from_file( $file, $outfile ); 1 }
      or $self->log_warn("Error creating '$outfile': $@\n");
    $files->{$file} = $outfile;
  }
}

sub manify_lib_pods {
  my $self    = shift;
  my %podman_args = (section => 3, @_); # libraries go in section 3

  my $files   = $self->_find_pods($self->{properties}{libdoc_dirs});
  return unless keys %$files;

  my $mandir = File::Spec->catdir( $self->blib, 'libdoc' );
  File::Path::mkpath( $mandir, 0, oct(777) );

  require Pod::Man;
  while (my ($file, $relfile) = each %$files) {
    # Pod::Simple based parsers only support one document per instance.
    # This is expected to change in a future version (Pod::Simple > 3.03).
    my $parser  = Pod::Man->new( %podman_args );
    my $manpage = $self->man3page_name( $relfile ) . '.' .
                  $self->config( 'man3ext' );
    my $outfile = File::Spec->catfile( $mandir, $manpage);
    next if $self->up_to_date( $file, $outfile );
    $self->log_verbose("Manifying $file -> $outfile\n");
    eval { $parser->parse_from_file( $file, $outfile ); 1 }
      or $self->log_warn("Error creating '$outfile': $@\n");
    $files->{$file} = $outfile;
  }
}

sub _find_pods {
  my ($self, $dirs, %args) = @_;
  my %files;
  foreach my $spec (@$dirs) {
    my $dir = $self->localize_dir_path($spec);
    next unless -e $dir;

    FILE: foreach my $file ( @{ $self->rscan_dir( $dir ) } ) {
      foreach my $regexp ( @{ $args{exclude} } ) {
        next FILE if $file =~ $regexp;
      }
      $file = $self->localize_file_path($file);
      $files{$file} = File::Spec->abs2rel($file, $dir) if $self->contains_pod( $file )
    }
  }
  return \%files;
}

sub contains_pod {
  my ($self, $file) = @_;
  return '' unless -T $file;  # Only look at text files

  open(my $fh, '<', $file ) or die "Can't open $file: $!";
  while (my $line = <$fh>) {
    return 1 if $line =~ /^\=(?:head|pod|item)/;
  }

  return '';
}

sub ACTION_html {
  my $self = shift;

  return unless $self->_mb_feature('HTML_support');

  $self->depends_on('code');

  foreach my $type ( qw(bin lib) ) {
    next unless ( $self->invoked_action eq 'html' || $self->_is_default_installable("${type}html"));
    $self->htmlify_pods( $type );
  }
}

# 1) If it's an ActiveState perl install, we need to run
#    ActivePerl::DocTools->UpdateTOC;
# 2) Links to other modules are not being generated
sub htmlify_pods {
  my $self = shift;
  my $type = shift;
  my $htmldir = shift || File::Spec->catdir($self->blib, "${type}html");

  $self->add_to_cleanup('pod2htm*');

  my $pods = $self->_find_pods( $self->{properties}{"${type}doc_dirs"},
                                exclude => [ $self->file_qr('\.(?:bat|com|html)$') ] );
  return unless %$pods;  # nothing to do

  unless ( -d $htmldir ) {
    File::Path::mkpath($htmldir, 0, oct(755))
      or die "Couldn't mkdir $htmldir: $!";
  }

  my @rootdirs = ($type eq 'bin') ? qw(bin) :
      $self->installdirs eq 'core' ? qw(lib) : qw(site lib);
  my $podroot = $ENV{PERL_CORE}
              ? File::Basename::dirname($ENV{PERL_CORE})
              : $self->original_prefix('core');

  my $htmlroot = $self->install_sets('core')->{libhtml};
  my $podpath;
  unless (defined $self->args('html_links') and !$self->args('html_links')) {
    my @podpath = ( (map { File::Spec->abs2rel($_ ,$podroot) } grep { -d  }
                     ( $self->install_sets('core', 'lib'), # lib
                       $self->install_sets('core', 'bin'), # bin
                       $self->install_sets('site', 'lib'), # site/lib
                     ) ), File::Spec->rel2abs($self->blib) );

    $podpath = $ENV{PERL_CORE}
      ? File::Spec->catdir($podroot, 'lib')
        : join(":", map { tr,:\\,|/,; $_ } @podpath);
  }

  my $blibdir = join('/', File::Spec->splitdir(
    (File::Spec->splitpath(File::Spec->rel2abs($htmldir),1))[1]),''
  );

  my ($with_ActiveState, $htmltool);

  if ( $with_ActiveState = $self->_is_ActivePerl
    && eval { require ActivePerl::DocTools::Pod; 1 }
  ) {
    my $tool_v = ActiveState::DocTools::Pod->VERSION;
    $htmltool = "ActiveState::DocTools::Pod";
    $htmltool .= " $tool_v" if $tool_v && length $tool_v;
  }
  else {
      require Module::Build::PodParser;
      require Pod::Html;
    $htmltool = "Pod::Html " .  Pod::Html->VERSION;
  }
  $self->log_verbose("Converting Pod to HTML with $htmltool\n");

  my $errors = 0;

  POD:
  foreach my $pod ( keys %$pods ) {

    my ($name, $path) = File::Basename::fileparse($pods->{$pod},
      $self->file_qr('\.(?:pm|plx?|pod)$')
    );
    my @dirs = File::Spec->splitdir( File::Spec->canonpath( $path ) );
    pop( @dirs ) if scalar(@dirs) && $dirs[-1] eq File::Spec->curdir;

    my $fulldir = File::Spec->catdir($htmldir, @rootdirs, @dirs);
    my $tmpfile = File::Spec->catfile($fulldir, "${name}.tmp");
    my $outfile = File::Spec->catfile($fulldir, "${name}.html");
    my $infile  = File::Spec->abs2rel($pod);

    next if $self->up_to_date($infile, $outfile);

    unless ( -d $fulldir ){
      File::Path::mkpath($fulldir, 0, oct(755))
        or die "Couldn't mkdir $fulldir: $!";
    }

    $self->log_verbose("HTMLifying $infile -> $outfile\n");
    if ( $with_ActiveState ) {
      my $depth = @rootdirs + @dirs;
      my %opts = ( infile => $infile,
        outfile => $tmpfile,
        ( defined($podpath) ? (podpath => $podpath) : ()),
        podroot => $podroot,
        index => 1,
        depth => $depth,
      );
      eval {
        ActivePerl::DocTools::Pod::pod2html(%opts);
        1;
      } or $self->log_warn("[$htmltool] pod2html (" .
        join(", ", map { "q{$_} => q{$opts{$_}}" } (keys %opts)) . ") failed: $@");
    } else {
      my $path2root = File::Spec->catdir((File::Spec->updir) x @dirs);
      open(my $fh, '<', $infile) or die "Can't read $infile: $!";
      my $abstract = Module::Build::PodParser->new(fh => $fh)->get_abstract();

      my $title = join( '::', (@dirs, $name) );
      $title .= " - $abstract" if $abstract;

      my @opts = (
        "--title=$title",
        ( defined($podpath) ? "--podpath=$podpath" : ()),
        "--infile=$infile",
        "--outfile=$tmpfile",
        "--podroot=$podroot",
        ($path2root ? "--htmlroot=$path2root" : ()),
      );

      unless ( eval{Pod::Html->VERSION(1.12)} ) {
        push( @opts, ('--flush') ); # caching removed in 1.12
      }

      if ( eval{Pod::Html->VERSION(1.12)} ) {
        push( @opts, ('--header', '--backlink') );
      } elsif ( eval{Pod::Html->VERSION(1.03)} ) {
        push( @opts, ('--header', '--backlink=Back to Top') );
      }

      $self->log_verbose("P::H::pod2html @opts\n");
      {
        my $orig = Cwd::getcwd();
        eval { Pod::Html::pod2html(@opts); 1 }
          or $self->log_warn("[$htmltool] pod2html( " .
          join(", ", map { "q{$_}" } @opts) . ") failed: $@");
        chdir($orig);
      }
    }
    # We now have to cleanup the resulting html file
    if ( ! -r $tmpfile ) {
      $errors++;
      next POD;
    }
    open(my $fh, '<', $tmpfile) or die "Can't read $tmpfile: $!";
    my $html = join('',<$fh>);
    close $fh;
    if (!$self->_is_ActivePerl) {
      # These fixups are already done by AP::DT:P:pod2html
      # The output from pod2html is NOT XHTML!
      # IE6+ will display content that is not valid for DOCTYPE
      $html =~ s#^<!DOCTYPE .*?>#<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">#im;
      $html =~ s#<html xmlns="http://www.w3.org/1999/xhtml">#<html>#i;

      # IE6+ will not display local HTML files with strict
      # security without this comment
      $html =~ s#<head>#<head>\n<!-- saved from url=(0017)http://localhost/ -->#i;
    }
    # Fixup links that point to our temp blib
    $html =~ s/\Q$blibdir\E//g;

    open($fh, '>', $outfile) or die "Can't write $outfile: $!";
    print $fh $html;
    close $fh;
    unlink($tmpfile);
  }

  return ! $errors;

}

# Adapted from ExtUtils::MM_Unix
sub man1page_name {
  my $self = shift;
  return File::Basename::basename( shift );
}

# Adapted from ExtUtils::MM_Unix and Pod::Man
# Depending on M::B's dependency policy, it might make more sense to refactor
# Pod::Man::begin_pod() to extract a name() methods, and use them...
#    -spurkis
sub man3page_name {
  my $self = shift;
  my ($vol, $dirs, $file) = File::Spec->splitpath( shift );
  my @dirs = File::Spec->splitdir( File::Spec->canonpath($dirs) );

  # Remove known exts from the base name
  $file =~ s/\.p(?:od|m|l)\z//i;

  return join( $self->manpage_separator, @dirs, $file );
}

sub manpage_separator {
  return '::';
}

# For systems that don't have 'diff' executable, should use Algorithm::Diff
sub ACTION_diff {
  my $self = shift;
  $self->depends_on('build');
  my $local_lib = File::Spec->rel2abs('lib');
  my @myINC = grep {$_ ne $local_lib} @INC;

  # The actual install destination might not be in @INC, so check there too.
  push @myINC, map $self->install_destination($_), qw(lib arch);

  my @flags = @{$self->{args}{ARGV}};
  @flags = $self->split_like_shell($self->{args}{flags} || '') unless @flags;

  my $installmap = $self->install_map;
  delete $installmap->{read};
  delete $installmap->{write};

  my $text_suffix = $self->file_qr('\.(pm|pod)$');

  while (my $localdir = each %$installmap) {
    my @localparts = File::Spec->splitdir($localdir);
    my $files = $self->rscan_dir($localdir, sub {-f});

    foreach my $file (@$files) {
      my @parts = File::Spec->splitdir($file);
      @parts = @parts[@localparts .. $#parts]; # Get rid of blib/lib or similar

      my $installed = Module::Build::ModuleInfo->find_module_by_name(
                        join('::', @parts), \@myINC );
      if (not $installed) {
        print "Only in lib: $file\n";
        next;
      }

      my $status = File::Compare::compare($installed, $file);
      next if $status == 0;  # Files are the same
      die "Can't compare $installed and $file: $!" if $status == -1;

      if ($file =~ $text_suffix) {
        $self->do_system('diff', @flags, $installed, $file);
      } else {
        print "Binary files $file and $installed differ\n";
      }
    }
  }
}

sub ACTION_pure_install {
  shift()->depends_on('install');
}

sub ACTION_install {
  my ($self) = @_;
  require ExtUtils::Install;
  $self->depends_on('build');
  # RT#63003 suggest that odd circumstances that we might wind up
  # in a different directory than we started, so wrap with _do_in_dir to
  # ensure we get back to where we started; hope this fixes it!
  $self->_do_in_dir( ".", sub {
    ExtUtils::Install::install(
      $self->install_map, $self->verbose, 0, $self->{args}{uninst}||0
    );
  });
  if ($self->_is_ActivePerl && $self->{_completed_actions}{html}) {
    $self->log_info("Building ActivePerl Table of Contents\n");
    eval { ActivePerl::DocTools::WriteTOC(verbose => $self->verbose ? 1 : 0); 1; }
      or $self->log_warn("AP::DT:: WriteTOC() failed: $@");
  }
  if ($self->_is_ActivePPM) {
    # We touch 'lib/perllocal.pod'. There is an existing logic in subroutine _init_db()
    # of 'ActivePerl/PPM/InstallArea.pm' that says that if 'lib/perllocal.pod' has a 'date-last-touched'
    # greater than that of the PPM SQLite databases ('etc/ppm-perl-area.db' and/or
    # 'site/etc/ppm-site-area.db') then the PPM SQLite databases are rebuilt from scratch.

    # in the following line, 'perllocal.pod' this is *always* 'lib/perllocal.pod', never 'site/lib/perllocal.pod'
    my $F_perllocal = File::Spec->catfile($self->install_sets('core', 'lib'), 'perllocal.pod');
    my $dt_stamp = time;

    $self->log_info("For ActivePerl's PPM: touch '$F_perllocal'\n");

    open my $perllocal, ">>", $F_perllocal;
    close $perllocal;
    utime($dt_stamp, $dt_stamp, $F_perllocal);
  }
}

sub ACTION_fakeinstall {
  my ($self) = @_;
  require ExtUtils::Install;
  my $eui_version = ExtUtils::Install->VERSION;
  if ( $eui_version < 1.32 ) {
    $self->log_warn(
      "The 'fakeinstall' action requires Extutils::Install 1.32 or later.\n"
      . "(You only have version $eui_version)."
    );
    return;
  }
  $self->depends_on('build');
  ExtUtils::Install::install($self->install_map, !$self->quiet, 1, $self->{args}{uninst}||0);
}

sub ACTION_versioninstall {
  my ($self) = @_;

  die "You must have only.pm 0.25 or greater installed for this operation: $@\n"
    unless eval { require only; 'only'->VERSION(0.25); 1 };

  $self->depends_on('build');

  my %onlyargs = map {exists($self->{args}{$_}) ? ($_ => $self->{args}{$_}) : ()}
    qw(version versionlib);
  only::install::install(%onlyargs);
}

sub ACTION_installdeps {
  my ($self) = @_;

  # XXX include feature prerequisites as optional prereqs?

  my $info = $self->_enum_prereqs;
  if (! $info ) {
    $self->log_info( "No prerequisites detected\n" );
    return;
  }

  my $failures = $self->prereq_failures($info);
  if ( ! $failures ) {
    $self->log_info( "All prerequisites satisfied\n" );
    return;
  }

  my @install;
  while (my ($type, $prereqs) = each %$failures) {
    if($type =~ m/^(?:\w+_)?requires$/) {
      push(@install, keys %$prereqs);
      next;
    }
    $self->log_info("Checking optional dependencies:\n");
    while (my ($module, $status) = each %$prereqs) {
      push(@install, $module) if($self->y_n("Install $module?", 'y'));
    }
  }

  return unless @install;

  my ($command, @opts) = $self->split_like_shell($self->cpan_client);

  # relative command should be relative to our active Perl
  # so we need to locate that command
  if ( ! File::Spec->file_name_is_absolute( $command ) ) {
    # prefer site to vendor to core
    my @loc = ( 'site', 'vendor', '' );
    my @bindirs = File::Basename::dirname($self->perl);
    push @bindirs,
      map {
        ($self->config->{"install${_}bin"}, $self->config->{"install${_}script"})
      } @loc;
    for my $d ( @bindirs ) {
      my $abs_cmd = $self->find_command(File::Spec->catfile( $d, $command ));
      if ( defined $abs_cmd ) {
        $command = $abs_cmd;
        last;
      }
    }
  }

  $self->do_system($command, @opts, @install);
}

sub ACTION_clean {
  my ($self) = @_;
  $self->log_info("Cleaning up build files\n");
  foreach my $item (map glob($_), $self->cleanup) {
    $self->delete_filetree($item);
  }
}

sub ACTION_realclean {
  my ($self) = @_;
  $self->depends_on('clean');
  $self->log_info("Cleaning up configuration files\n");
  $self->delete_filetree(
    $self->config_dir, $self->mymetafile, $self->mymetafile2, $self->build_script
  );
}

sub ACTION_ppd {
  my ($self) = @_;

  require Module::Build::PPMMaker;
  my $ppd = Module::Build::PPMMaker->new();
  my $file = $ppd->make_ppd(%{$self->{args}}, build => $self);
  $self->add_to_cleanup($file);
}

sub ACTION_ppmdist {
  my ($self) = @_;

  $self->depends_on( 'build' );

  my $ppm = $self->ppm_name;
  $self->delete_filetree( $ppm );
  $self->log_info( "Creating $ppm\n" );
  $self->add_to_cleanup( $ppm, "$ppm.tar.gz" );

  my %types = ( # translate types/dirs to those expected by ppm
    lib     => 'lib',
    arch    => 'arch',
    bin     => 'bin',
    script  => 'script',
    bindoc  => 'man1',
    libdoc  => 'man3',
    binhtml => undef,
    libhtml => undef,
  );

  foreach my $type ($self->install_types) {
    next if exists( $types{$type} ) && !defined( $types{$type} );

    my $dir = File::Spec->catdir( $self->blib, $type );
    next unless -e $dir;

    my $files = $self->rscan_dir( $dir );
    foreach my $file ( @$files ) {
      next unless -f $file;
      my $rel_file =
        File::Spec->abs2rel( File::Spec->rel2abs( $file ),
                             File::Spec->rel2abs( $dir  ) );
      my $to_file  =
        File::Spec->catfile( $ppm, 'blib',
                            exists( $types{$type} ) ? $types{$type} : $type,
                            $rel_file );
      $self->copy_if_modified( from => $file, to => $to_file );
    }
  }

  foreach my $type ( qw(bin lib) ) {
    $self->htmlify_pods( $type, File::Spec->catdir($ppm, 'blib', 'html') );
  }

  # create a tarball;
  # the directory tar'ed must be blib so we need to do a chdir first
  my $target = File::Spec->catfile( File::Spec->updir, $ppm );
  $self->_do_in_dir( $ppm, sub { $self->make_tarball( 'blib', $target ) } );

  $self->depends_on( 'ppd' );

  $self->delete_filetree( $ppm );
}

sub ACTION_pardist {
  my ($self) = @_;

  # Need PAR::Dist
  if ( not eval { require PAR::Dist; PAR::Dist->VERSION(0.17) } ) {
    $self->log_warn(
      "In order to create .par distributions, you need to\n"
      . "install PAR::Dist first."
    );
    return();
  }

  $self->depends_on( 'build' );

  return PAR::Dist::blib_to_par(
    name => $self->dist_name,
    version => $self->dist_version,
  );
}

sub ACTION_dist {
  my ($self) = @_;

  # MUST dispatch() and not depends_ok() so we generate a clean distdir
  $self->dispatch('distdir');

  my $dist_dir = $self->dist_dir;

  $self->make_tarball($dist_dir);
  $self->delete_filetree($dist_dir);
}

sub ACTION_distcheck {
  my ($self) = @_;

  $self->_check_manifest_skip unless $self->invoked_action eq 'distclean';

  require ExtUtils::Manifest;
  local $^W; # ExtUtils::Manifest is not warnings clean.
  my ($missing, $extra) = ExtUtils::Manifest::fullcheck();

  return unless @$missing || @$extra;

  my $msg = "MANIFEST appears to be out of sync with the distribution\n";
  if ( $self->invoked_action eq 'distcheck' ) {
    die $msg;
  } else {
    warn $msg;
  }
}

sub _check_mymeta_skip {
  my $self = shift;
  my $maniskip = shift || 'MANIFEST.SKIP';

  require ExtUtils::Manifest;
  local $^W; # ExtUtils::Manifest is not warnings clean.

  # older ExtUtils::Manifest had a private _maniskip
  my $skip_factory = ExtUtils::Manifest->can('maniskip')
                  || ExtUtils::Manifest->can('_maniskip');

  my $mymetafile = $self->mymetafile;
  # we can't check it, just add it anyway to be safe
  for my $file ( $self->mymetafile, $self->mymetafile2 ) {
    unless ( $skip_factory && $skip_factory->($maniskip)->($file) ) {
      $self->log_warn("File '$maniskip' does not include '$file'. Adding it now.\n");
      my $safe = quotemeta($file);
      $self->_append_maniskip("^$safe\$", $maniskip);
    }
  }
}

sub _add_to_manifest {
  my ($self, $manifest, $lines) = @_;
  $lines = [$lines] unless ref $lines;

  my $existing_files = $self->_read_manifest($manifest);
  return unless defined( $existing_files );

  @$lines = grep {!exists $existing_files->{$_}} @$lines
    or return;

  my $mode = (stat $manifest)[2];
  chmod($mode | oct(222), $manifest) or die "Can't make $manifest writable: $!";

  open(my $fh, '<', $manifest) or die "Can't read $manifest: $!";
  my $last_line = (<$fh>)[-1] || "\n";
  my $has_newline = $last_line =~ /\n$/;
  close $fh;

  open($fh, '>>', $manifest) or die "Can't write to $manifest: $!";
  print $fh "\n" unless $has_newline;
  print $fh map "$_\n", @$lines;
  close $fh;
  chmod($mode, $manifest);

  $self->log_verbose(map "Added to $manifest: $_\n", @$lines);
}

sub _sign_dir {
  my ($self, $dir) = @_;

  unless (eval { require Module::Signature; 1 }) {
    $self->log_warn("Couldn't load Module::Signature for 'distsign' action:\n $@\n");
    return;
  }

  # Add SIGNATURE to the MANIFEST
  {
    my $manifest = File::Spec->catfile($dir, 'MANIFEST');
    die "Signing a distribution requires a MANIFEST file" unless -e $manifest;
    $self->_add_to_manifest($manifest, "SIGNATURE    Added here by Module::Build");
  }

  # Would be nice if Module::Signature took a directory argument.

  $self->_do_in_dir($dir, sub {local $Module::Signature::Quiet = 1; Module::Signature::sign()});
}

sub _do_in_dir {
  my ($self, $dir, $do) = @_;

  my $start_dir = File::Spec->rel2abs($self->cwd);
  chdir $dir or die "Can't chdir() to $dir: $!";
  eval {$do->()};
  my @err = $@ ? ($@) : ();
  chdir $start_dir or push @err, "Can't chdir() back to $start_dir: $!";
  die join "\n", @err if @err;
}

sub ACTION_distsign {
  my ($self) = @_;
  {
    local $self->{properties}{sign} = 0;  # We'll sign it ourselves
    $self->depends_on('distdir') unless -d $self->dist_dir;
  }
  $self->_sign_dir($self->dist_dir);
}

sub ACTION_skipcheck {
  my ($self) = @_;

  require ExtUtils::Manifest;
  local $^W; # ExtUtils::Manifest is not warnings clean.
  ExtUtils::Manifest::skipcheck();
}

sub ACTION_distclean {
  my ($self) = @_;

  $self->depends_on('realclean');
  $self->depends_on('distcheck');
}

sub do_create_makefile_pl {
  my $self = shift;
  require Module::Build::Compat;
  $self->log_info("Creating Makefile.PL\n");
  eval { Module::Build::Compat->create_makefile_pl($self->create_makefile_pl, $self, @_) };
  if ( $@ ) {
    1 while unlink 'Makefile.PL';
    die "$@\n";
  }
  $self->_add_to_manifest('MANIFEST', 'Makefile.PL');
}

sub do_create_license {
  my $self = shift;
  $self->log_info("Creating LICENSE file\n");

  if (  ! $self->_mb_feature('license_creation') ) {
    $self->_warn_mb_feature_deps('license_creation');
    die "Aborting.\n";
  }

  my $l = $self->license
    or die "Can't create LICENSE file: No license specified\n";

  my $license = $self->_software_license_object
    or die << "HERE";
Can't create LICENSE file: '$l' is not a valid license key
or Software::License subclass;
HERE

  $self->delete_filetree('LICENSE');

  open(my $fh, '>', 'LICENSE')
    or die "Can't write LICENSE file: $!";
  print $fh $license->fulltext;
  close $fh;

  $self->_add_to_manifest('MANIFEST', 'LICENSE');
}

sub do_create_readme {
  my $self = shift;
  $self->delete_filetree('README');

  my $docfile = $self->_main_docfile;
  unless ( $docfile ) {
    $self->log_warn(<<EOF);
Cannot create README: can't determine which file contains documentation;
Must supply either 'dist_version_from', or 'module_name' parameter.
EOF
    return;
  }

  # work around some odd Pod::Readme->new() failures in test reports by
  # confirming that new() is available
  if ( eval {require Pod::Readme; Pod::Readme->can('new') } ) {
    $self->log_info("Creating README using Pod::Readme\n");

    my $parser = Pod::Readme->new;
    $parser->parse_from_file($docfile, 'README', @_);

  } elsif ( eval {require Pod::Text; 1} ) {
    $self->log_info("Creating README using Pod::Text\n");

    if ( open(my $fh, '>', 'README') ) {
      local $^W = 0;
      no strict "refs";

      # work around bug in Pod::Text 3.01, which expects
      # Pod::Simple::parse_file to take input and output filehandles
      # when it actually only takes an input filehandle

      my $old_parse_file;
      $old_parse_file = \&{"Pod::Simple::parse_file"}
        and
      local *{"Pod::Simple::parse_file"} = sub {
        my $self = shift;
        $self->output_fh($_[1]) if $_[1];
        $self->$old_parse_file($_[0]);
      }
        if $Pod::Text::VERSION
          == 3.01; # Split line to avoid evil version-finder

      Pod::Text::pod2text( $docfile, $fh );

      close $fh;
    } else {
      $self->log_warn(
        "Cannot create 'README' file: Can't open file for writing\n" );
      return;
    }

  } else {
    $self->log_warn("Can't load Pod::Readme or Pod::Text to create README\n");
    return;
  }

  $self->_add_to_manifest('MANIFEST', 'README');
}

sub _main_docfile {
  my $self = shift;
  if ( my $pm_file = $self->dist_version_from ) {
    (my $pod_file = $pm_file) =~ s/.pm$/.pod/;
    return (-e $pod_file ? $pod_file : $pm_file);
  } else {
    return undef;
  }
}

sub do_create_bundle_inc {
  my $self = shift;
  my $dist_inc = File::Spec->catdir( $self->dist_dir, 'inc' );
  require inc::latest;
  inc::latest->write($dist_inc, @{$self->bundle_inc_preload});
  inc::latest->bundle_module($_, $dist_inc) for @{$self->bundle_inc};
  return 1;
}

sub ACTION_distdir {
  my ($self) = @_;

  if ( @{$self->bundle_inc} && ! $self->_mb_feature('inc_bundling_support') ) {
    $self->_warn_mb_feature_deps('inc_bundling_support');
    die "Aborting.\n";
  }

  $self->depends_on('distmeta');

  my $dist_files = $self->_read_manifest('MANIFEST')
    or die "Can't create distdir without a MANIFEST file - run 'manifest' action first.\n";
  delete $dist_files->{SIGNATURE};  # Don't copy, create a fresh one
  die "No files found in MANIFEST - try running 'manifest' action?\n"
    unless ($dist_files and keys %$dist_files);
  my $metafile = $self->metafile;
  $self->log_warn("*** Did you forget to add $metafile to the MANIFEST?\n")
    unless exists $dist_files->{$metafile};

  my $dist_dir = $self->dist_dir;
  $self->delete_filetree($dist_dir);
  $self->log_info("Creating $dist_dir\n");
  $self->add_to_cleanup($dist_dir);

  foreach my $file (keys %$dist_files) {
    next if $file =~ m{^MYMETA\.}; # Double check that we skip MYMETA.*
    my $new = $self->copy_if_modified(from => $file, to_dir => $dist_dir, verbose => 0);
  }

  $self->do_create_bundle_inc if @{$self->bundle_inc};

  $self->_sign_dir($dist_dir) if $self->{properties}{sign};
}

sub ACTION_disttest {
  my ($self) = @_;

  $self->depends_on('distdir');

  $self->_do_in_dir
    ( $self->dist_dir,
      sub {
        # XXX could be different names for scripts

        $self->run_perl_script('Build.PL') # XXX Should this be run w/ --nouse-rcfile
          or die "Error executing 'Build.PL' in dist directory: $!";
        $self->run_perl_script($self->build_script)
          or die "Error executing $self->build_script in dist directory: $!";
        $self->run_perl_script($self->build_script, [], ['test'])
          or die "Error executing 'Build test' in dist directory";
      });
}

sub ACTION_distinstall {
  my ($self, @args) = @_;

  $self->depends_on('distdir');

  $self->_do_in_dir ( $self->dist_dir,
    sub {
      $self->run_perl_script('Build.PL')
        or die "Error executing 'Build.PL' in dist directory: $!";
      $self->run_perl_script($self->build_script)
        or die "Error executing $self->build_script in dist directory: $!";
      $self->run_perl_script($self->build_script, [], ['install'])
        or die "Error executing 'Build install' in dist directory";
    }
  );
}

=begin private

  my $has_include = $build->_eumanifest_has_include;

Returns true if the installed version of ExtUtils::Manifest supports
#include and #include_default directives.  False otherwise.

=end private

=cut

# #!include and #!include_default were added in 1.50
sub _eumanifest_has_include {
    my $self = shift;

    require ExtUtils::Manifest;
    return eval { ExtUtils::Manifest->VERSION(1.50); 1 };
}


=begin private

  my $maniskip_file = $build->_default_maniskip;

Returns the location of the installed MANIFEST.SKIP file used by
default.

=end private

=cut

sub _default_maniskip {
    my $self = shift;

    my $default_maniskip;
    for my $dir (@INC) {
        $default_maniskip = File::Spec->catfile($dir, "ExtUtils", "MANIFEST.SKIP");
        last if -r $default_maniskip;
    }

    return $default_maniskip;
}


=begin private

  my $content = $build->_slurp($file);

Reads $file and returns the $content.

=end private

=cut

sub _slurp {
    my $self = shift;
    my $file = shift;
    my $mode = shift || "";
    open my $fh, "<$mode", $file or croak "Can't open $file for reading: $!";
    local $/;
    return <$fh>;
}

sub _spew {
    my $self = shift;
    my $file = shift;
    my $content = shift || "";
    my $mode = shift || "";
    open my $fh, ">$mode", $file or croak "Can't open $file for writing: $!";
    print {$fh} $content;
    close $fh;
}

sub _case_tolerant {
  my $self = shift;
  if ( ref $self ) {
    $self->{_case_tolerant} = File::Spec->case_tolerant
      unless defined($self->{_case_tolerant});
    return $self->{_case_tolerant};
  }
  else {
    return File::Spec->case_tolerant;
  }
}

sub _append_maniskip {
  my $self = shift;
  my $skip = shift;
  my $file = shift || 'MANIFEST.SKIP';
  return unless defined $skip && length $skip;
  open(my $fh, '>>', $file)
    or die "Can't open $file: $!";

  print $fh "$skip\n";
  close $fh;
}

sub _write_default_maniskip {
  my $self = shift;
  my $file = shift || 'MANIFEST.SKIP';
  open(my $fh, '>', $file)
    or die "Can't open $file: $!";

  my $content = $self->_eumanifest_has_include ? "#!include_default\n"
                                               : $self->_slurp( $self->_default_maniskip );

  $content .= <<'EOF';
# Avoid configuration metadata file
^MYMETA\.

# Avoid Module::Build generated and utility files.
\bBuild$
\bBuild.bat$
\b_build
\bBuild.COM$
\bBUILD.COM$
\bbuild.com$
^MANIFEST\.SKIP

# Avoid archives of this distribution
EOF

  # Skip, for example, 'Module-Build-0.27.tar.gz'
  $content .= '\b'.$self->dist_name.'-[\d\.\_]+'."\n";

  print $fh $content;
  
  close $fh;

  return;
}

sub _check_manifest_skip {
  my ($self) = @_;

  my $maniskip = 'MANIFEST.SKIP';

  if ( ! -e $maniskip ) {
    $self->log_warn("File '$maniskip' does not exist: Creating a temporary '$maniskip'\n");
    $self->_write_default_maniskip($maniskip);
    $self->_unlink_on_exit($maniskip);
  }
  else {
    # MYMETA must not be added to MANIFEST, so always confirm the skip
    $self->_check_mymeta_skip( $maniskip );
  }

  return;
}

sub ACTION_manifest {
  my ($self) = @_;

  $self->_check_manifest_skip;

  require ExtUtils::Manifest;  # ExtUtils::Manifest is not warnings clean.
  local ($^W, $ExtUtils::Manifest::Quiet) = (0,1);
  ExtUtils::Manifest::mkmanifest();
}

sub ACTION_manifest_skip {
  my ($self) = @_;

  if ( -e 'MANIFEST.SKIP' ) {
    $self->log_warn("MANIFEST.SKIP already exists.\n");
    return 0;
  }
  $self->log_info("Creating a new MANIFEST.SKIP file\n");
  return $self->_write_default_maniskip;
  return -e 'MANIFEST.SKIP'
}

# Case insensitive regex for files
sub file_qr {
    return shift->{_case_tolerant} ? qr($_[0])i : qr($_[0]);
}

sub dist_dir {
  my ($self) = @_;
  my $dir = join "-", $self->dist_name, $self->dist_version;
  $dir .= "-" . $self->dist_suffix if $self->dist_suffix;
  return $dir;
}

sub ppm_name {
  my $self = shift;
  return 'PPM-' . $self->dist_dir;
}

sub _files_in {
  my ($self, $dir) = @_;
  return unless -d $dir;

  local *DH;
  opendir DH, $dir or die "Can't read directory $dir: $!";

  my @files;
  while (defined (my $file = readdir DH)) {
    my $full_path = File::Spec->catfile($dir, $file);
    next if -d $full_path;
    push @files, $full_path;
  }
  return @files;
}

sub share_dir {
  my $self = shift;
  my $p = $self->{properties};

  $p->{share_dir} = shift if @_;

  # Always coerce to proper hash form
  if    ( ! defined $p->{share_dir} ) {
    return;
  }
  elsif ( ! ref $p->{share_dir}  ) {
    # scalar -- treat as a single 'dist' directory
    $p->{share_dir} = { dist => [ $p->{share_dir} ] };
  }
  elsif ( ref $p->{share_dir} eq 'ARRAY' ) {
    # array -- treat as a list of 'dist' directories
    $p->{share_dir} = { dist => $p->{share_dir} };
  }
  elsif ( ref $p->{share_dir} eq 'HASH' ) {
    # hash -- check structure
    my $share_dir = $p->{share_dir};
    # check dist key
    if ( defined $share_dir->{dist} ) {
      if ( ! ref $share_dir->{dist} ) {
        # scalar, so upgrade to arrayref
        $share_dir->{dist} = [ $share_dir->{dist} ];
      }
      elsif ( ref $share_dir->{dist} ne 'ARRAY' ) {
        die "'dist' key in 'share_dir' must be scalar or arrayref";
      }
    }
    # check module key
    if ( defined $share_dir->{module} ) {
      my $mod_hash = $share_dir->{module};
      if ( ref $mod_hash eq 'HASH' ) {
        for my $k ( keys %$mod_hash ) {
          if ( ! ref $mod_hash->{$k} ) {
            $mod_hash->{$k} = [ $mod_hash->{$k} ];
          }
          elsif( ref $mod_hash->{$k} ne 'ARRAY' ) {
            die "modules in 'module' key of 'share_dir' must be scalar or arrayref";
          }
        }
      }
      else {
          die "'module' key in 'share_dir' must be hashref";
      }
    }
  }
  else {
    die "'share_dir' must be hashref, arrayref or string";
  }

  return $p->{share_dir};
}

sub script_files {
  my $self = shift;

  for ($self->{properties}{script_files}) {
    $_ = shift if @_;
    next unless $_;

    # Always coerce into a hash
    return $_ if UNIVERSAL::isa($_, 'HASH');
    return $_ = { map {$_,1} @$_ } if UNIVERSAL::isa($_, 'ARRAY');

    die "'script_files' must be a hashref, arrayref, or string" if ref();

    return $_ = { map {$_,1} $self->_files_in( $_ ) } if -d $_;
    return $_ = {$_ => 1};
  }

  my %pl_files = map {
    File::Spec->canonpath( $_ ) => 1
  } keys %{ $self->PL_files || {} };

  my @bin_files = $self->_files_in('bin');

  my %bin_map = map {
    $_ => File::Spec->canonpath( $_ )
  } @bin_files;

  return $_ = { map {$_ => 1} grep !$pl_files{$bin_map{$_}}, @bin_files };
}
BEGIN { *scripts = \&script_files; }

{
  my %licenses = (
    perl         => 'Perl_5',
    apache       => 'Apache_2_0',
    apache_1_1   => 'Apache_1_1',
    artistic     => 'Artistic_1',
    artistic_2   => 'Artistic_2',
    lgpl         => 'LGPL_2_1',
    lgpl2        => 'LGPL_2_1',
    lgpl3        => 'LGPL_3_0',
    bsd          => 'BSD',
    gpl          => 'GPL_1',
    gpl2         => 'GPL_2',
    gpl3         => 'GPL_3',
    mit          => 'MIT',
    mozilla      => 'Mozilla_1_1',
    restrictive  => 'Restricted',
    open_source  => undef,
    unrestricted => undef,
    unknown      => undef,
  );

  # TODO - would be nice to not have these here, since they're more
  # properly stored only in Software::License
  my %license_urls = (
    perl         => 'http://dev.perl.org/licenses/',
    apache       => 'http://apache.org/licenses/LICENSE-2.0',
    apache_1_1   => 'http://apache.org/licenses/LICENSE-1.1',
    artistic     => 'http://opensource.org/licenses/artistic-license.php',
    artistic_2   => 'http://opensource.org/licenses/artistic-license-2.0.php',
    lgpl         => 'http://opensource.org/licenses/lgpl-license.php',
    lgpl2        => 'http://opensource.org/licenses/lgpl-2.1.php',
    lgpl3        => 'http://opensource.org/licenses/lgpl-3.0.html',
    bsd          => 'http://opensource.org/licenses/bsd-license.php',
    gpl          => 'http://opensource.org/licenses/gpl-license.php',
    gpl2         => 'http://opensource.org/licenses/gpl-2.0.php',
    gpl3         => 'http://opensource.org/licenses/gpl-3.0.html',
    mit          => 'http://opensource.org/licenses/mit-license.php',
    mozilla      => 'http://opensource.org/licenses/mozilla1.1.php',
    restrictive  => undef,
    open_source  => undef,
    unrestricted => undef,
    unknown      => undef,
  );
  sub valid_licenses {
    return \%licenses;
  }
  sub _license_url {
    return $license_urls{$_[1]};
  }
}

sub _software_license_class {
  my ($self, $license) = @_;
  if ($self->valid_licenses->{$license} && eval { require Software::LicenseUtils; Software::LicenseUtils->VERSION(0.103009) }) {
    my ($class) = Software::LicenseUtils->guess_license_from_meta_key($license, 1);
	eval "require $class";
	#die $class;
	return $class;
  }
  LICENSE: for my $l ( $self->valid_licenses->{ $license }, $license ) {
    next unless defined $l;
    my $trial = "Software::License::" . $l;
    if ( eval "require Software::License; Software::License->VERSION(0.014); require $trial; 1" ) {
      return $trial;
    }
  }
  return;
}

# use mapping or license name directly
sub _software_license_object {
  my ($self) = @_;
  return unless defined( my $license = $self->license );

  my $class = $self->_software_license_class($license) or return;

  # Software::License requires a 'holder' argument
  my $author = join( " & ", @{ $self->dist_author }) || 'unknown';
  my $sl = eval { $class->new({holder=>$author}) };
  if ( $@ ) {
    $self->log_warn( "Error getting '$class' object: $@" );
  }

  return $sl;
}

sub _hash_merge {
  my ($self, $h, $k, $v) = @_;
  if (ref $h->{$k} eq 'ARRAY') {
    push @{$h->{$k}}, ref $v ? @$v : $v;
  } elsif (ref $h->{$k} eq 'HASH') {
    $h->{$k}{$_} = $v->{$_} foreach keys %$v;
  } else {
    $h->{$k} = $v;
  }
}

sub ACTION_distmeta {
  my ($self) = @_;
  $self->do_create_makefile_pl if $self->create_makefile_pl;
  $self->do_create_readme if $self->create_readme;
  $self->do_create_license if $self->create_license;
  $self->do_create_metafile;
}

sub do_create_metafile {
  my $self = shift;
  return if $self->{wrote_metadata};

  my $p = $self->{properties};

  unless ($p->{license}) {
    $self->log_warn("No license specified, setting license = 'unknown'\n");
    $p->{license} = 'unknown';
  }

  my @metafiles = ( $self->metafile, $self->metafile2 );
  # If we're in the distdir, the metafile may exist and be non-writable.
  $self->delete_filetree($_) for @metafiles;

  # Since we're building ourself, we have to do some special stuff
  # here: the ConfigData module is found in blib/lib.
  local @INC = @INC;
  if (($self->module_name || '') eq 'Module::Build') {
    $self->depends_on('config_data');
    push @INC, File::Spec->catdir($self->blib, 'lib');
  }

  my $meta_obj = $self->_get_meta_object(
    quiet => 1, fatal => 1, auto => 1
  );
  my @created = $self->_write_meta_files( $meta_obj, 'META' );
  if ( @created ) {
    $self->{wrote_metadata} = 1;
    $self->_add_to_manifest('MANIFEST', $_) for @created;
  }
  return 1;
}

sub _write_meta_files {
  my $self = shift;
  my ($meta, $file) = @_;
  $file =~ s{\.(?:yml|json)$}{};

  my @created;
  push @created, "$file\.yml"
    if $meta && $meta->save( "$file\.yml", {version => "1.4"} );
  push @created, "$file\.json"
    if $meta && $meta->save( "$file\.json" );

  if ( @created ) {
    $self->log_info("Created " . join(" and ", @created) . "\n");
  }
  return @created;
}

sub _get_meta_object {
  my $self = shift;
  my %args = @_;
  return unless $self->try_require("CPAN::Meta", "2.110420");

  my $meta;
  eval {
    my $data = $self->get_metadata(
      fatal => $args{fatal},
      auto => $args{auto},
    );
    $data->{dynamic_config} = $args{dynamic} if defined $args{dynamic};
    $meta = CPAN::Meta->create($data);
  };
  if ($@ && ! $args{quiet}) {
    $self->log_warn(
      "Could not get valid metadata. Error is: $@\n"
    );
  }

  return $meta;
}

# We return a version 1.4 structure for backwards compatibility
sub read_metafile {
  my $self = shift;
  my ($metafile) = @_;

  return unless $self->try_require("CPAN::Meta", "2.110420");
  my $meta = CPAN::Meta->load_file($metafile);
  return $meta->as_struct( {version => "1.4"} );
}

# For legacy compatibility, we upconvert a 1.4 data structure, ensuring
# validity, and then downconvert it back to save it.
#
# generally, this code should no longer be used
sub write_metafile {
  my $self = shift;
  my ($metafile, $struct) = @_;

  return unless $self->try_require("CPAN::Meta", "2.110420");

  my $meta = CPAN::Meta->new( $struct );
  return $meta->save( $metafile, { version => "1.4" } );
}

sub normalize_version {
  my ($self, $version) = @_;
  $version = 0 unless defined $version and length $version;

  if ( $version =~ /[=<>!,]/ ) { # logic, not just version
    # take as is without modification
  }
  elsif ( ref $version eq 'version' ||
          ref $version eq 'Module::Build::Version' ) { # version objects
    $version = $version->is_qv ? $version->normal : $version->stringify;
  }
  elsif ( $version =~ /^[^v][^.]*\.[^.]+\./ ) { # no leading v, multiple dots
    # normalize string tuples without "v": "1.2.3" -> "v1.2.3"
    $version = "v$version";
  }
  else {
    # leave alone
  }
  return $version;
}

my %prereq_map = (
  requires => [ qw/runtime requires/],
  configure_requires => [qw/configure requires/],
  build_requires => [ qw/build requires/ ],
  test_requires => [ qw/test requires/ ],
  test_recommends => [ qw/test recommends/ ],
  recommends => [ qw/runtime recommends/ ],
  conflicts => [ qw/runtime conflicts/ ],
);

sub _normalize_prereqs {
  my ($self) = @_;
  my $p = $self->{properties};

  # copy prereq data structures so we can modify them before writing to META
  my %prereq_types;
  for my $type ( 'configure_requires', @{$self->prereq_action_types} ) {
    if (exists $p->{$type} and keys %{ $p->{$type} }) {
      my ($phase, $relation) = @{ $prereq_map{$type} };
      for my $mod ( keys %{ $p->{$type} } ) {
        $prereq_types{$phase}{$relation}{$mod} = $self->normalize_version($p->{$type}{$mod});
      }
    }
  }
  return \%prereq_types;
}

sub _get_license {
  my $self = shift;

  my $license = $self->license;
  my ($meta_license, $meta_license_url);

  my $valid_licenses = $self->valid_licenses();
  if ( my $sl = $self->_software_license_object ) {
    $meta_license = $sl->meta2_name;
    $meta_license_url = $sl->url;
  }
  elsif ( exists $valid_licenses->{$license} ) {
    $meta_license = $valid_licenses->{$license} ? lc $valid_licenses->{$license} : $license;
    $meta_license_url = $self->_license_url( $license );
  }
  else {
    $self->log_warn( "Can not determine license type for '" . $self->license
      . "'\nSetting META license field to 'unknown'.\n");
    $meta_license = 'unknown';
  }
  return ($meta_license, $meta_license_url);
}

my %keep = map { $_ => 1 } qw/keywords dynamic_config provides no_index name version abstract/;
my %ignore = map { $_ => 1 } qw/distribution_type/;
my %reject = map { $_ => 1 } qw/private author license requires recommends build_requires configure_requires conflicts/;

sub _upconvert_resources {
  my ($input) = @_;
  my %output;
  for my $key (keys %{$input}) {
    my $out_key = $key =~ /^\p{Lu}/ ? "x_\l$key" : $key;
    if ($key eq 'repository') {
      my $name = $input->{$key} =~ m{ \A http s? :// .* (<! \.git ) \z }xms ? 'web' : 'url';
      $output{$out_key} = { $name => $input->{$key} };
    }
    elsif ($key eq 'bugtracker') {
      $output{$out_key} = { web => $input->{$key} }
    }
    else {
      $output{$out_key} = $input->{$key};
    }
  }
  return \%output
}
my %custom = (
	resources => \&_upconvert_resources,
);

sub _upconvert_metapiece {
  my ($input, $type) = @_;
  return $input if exists $input->{'meta-spec'} && $input->{'meta-spec'}{version} == 2;

  my %ret;
  for my $key (keys %{$input}) {
    if ($keep{$key}) {
      $ret{$key} = $input->{$key};
    }
    elsif ($ignore{$key}) {
      next;
    }
    elsif ($reject{$key}) {
      croak "Can't $type $key, please use another mechanism";
    }
    elsif (my $converter = $custom{$key}) {
      $ret{$key} = $converter->($input->{$key});
    }
    else {
      my $out_key = $key =~ / \A x_ /xi ? $key : "x_$key";
      $ret{$out_key} = $input->{$key};
    }
  }
  return \%ret;
}

sub get_metadata {
  my ($self, %args) = @_;

  my $fatal = $args{fatal} || 0;
  my $p = $self->{properties};

  $self->auto_config_requires if $args{auto};

  # validate required fields
  foreach my $f (qw(dist_name dist_version dist_author dist_abstract license)) {
    my $field = $self->$f();
    unless ( defined $field and length $field ) {
      my $err = "ERROR: Missing required field '$f' for metafile\n";
      if ( $fatal ) {
        die $err;
      }
      else {
        $self->log_warn($err);
      }
    }
  }

  my %metadata = (
    name => $self->dist_name,
    version => $self->normalize_version($self->dist_version),
    author => $self->dist_author,
    abstract => $self->dist_abstract,
    generated_by => "Module::Build version $Module::Build::VERSION",
    'meta-spec' => {
      version => '2',
      url     => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
    },
    dynamic_config => exists $p->{dynamic_config} ? $p->{dynamic_config} : 1,
    release_status => $self->release_status,
  );

  my ($meta_license, $meta_license_url) = $self->_get_license;
  $metadata{license} = [ $meta_license ];
  $metadata{resources}{license} = [ $meta_license_url ] if defined $meta_license_url;

  $metadata{prereqs} = $self->_normalize_prereqs;

  if (exists $p->{no_index}) {
    $metadata{no_index} = $p->{no_index};
  } elsif (my $pkgs = eval { $self->find_dist_packages }) {
    $metadata{provides} = $pkgs if %$pkgs;
  } else {
    $self->log_warn("$@\nWARNING: Possible missing or corrupt 'MANIFEST' file.\n" .
                    "Nothing to enter for 'provides' field in metafile.\n");
  }

  my $meta_add = _upconvert_metapiece($self->meta_add, 'add');
  while (my($k, $v) = each %{$meta_add} ) {
    $metadata{$k} = $v;
  }

  my $meta_merge = _upconvert_metapiece($self->meta_merge, 'merge');
  while (my($k, $v) = each %{$meta_merge} ) {
    $self->_hash_merge(\%metadata, $k, $v);
  }

  return \%metadata;
}

# To preserve compatibility with old API, $node *must* be a hashref
# passed in to prepare_metadata.  $keys is an arrayref holding a
# list of keys -- it's use is optional and generally no longer needed
# but kept for back compatibility.  $args is an optional parameter to
# support the new 'fatal' toggle

sub prepare_metadata {
  my ($self, $node, $keys, $args) = @_;
  unless ( ref $node eq 'HASH' ) {
    croak "prepare_metadata() requires a hashref argument to hold output\n";
  }
  croak 'Keys argument to prepare_metadata is no longer supported' if $keys;
  %{$node} = %{ $self->get_meta(%{$args}) };
  return $node;
}

sub _read_manifest {
  my ($self, $file) = @_;
  return undef unless -e $file;

  require ExtUtils::Manifest;  # ExtUtils::Manifest is not warnings clean.
  local ($^W, $ExtUtils::Manifest::Quiet) = (0,1);
  return scalar ExtUtils::Manifest::maniread($file);
}

sub find_dist_packages {
  my $self = shift;

  # Only packages in .pm files are candidates for inclusion here.
  # Only include things in the MANIFEST, not things in developer's
  # private stock.

  my $manifest = $self->_read_manifest('MANIFEST')
    or die "Can't find dist packages without a MANIFEST file\nRun 'Build manifest' to generate one\n";

  # Localize
  my %dist_files = map { $self->localize_file_path($_) => $_ }
                       keys %$manifest;

  my @pm_files = grep { $_ !~ m{^t} } # skip things in t/
                   grep {exists $dist_files{$_}}
                     keys %{ $self->find_pm_files };

  return $self->find_packages_in_files(\@pm_files, \%dist_files);
}

# XXX Do not document this function; mst wrote it and now says the API is
# stupid and needs to be fixed and it shouldn't become a public API until then
sub find_packages_in_files {
  my ($self, $file_list, $filename_map) = @_;

  # First, we enumerate all packages & versions,
  # separating into primary & alternative candidates
  my( %prime, %alt );
  foreach my $file (@{$file_list}) {
    my $mapped_filename = $filename_map->{$file};
    my @path = split( /\//, $mapped_filename );
    (my $prime_package = join( '::', @path[1..$#path] )) =~ s/\.pm$//;

    my $pm_info = Module::Build::ModuleInfo->new_from_file( $file );

    foreach my $package ( $pm_info->packages_inside ) {
      next if $package eq 'main';  # main can appear numerous times, ignore
      next if $package eq 'DB';    # special debugging package, ignore
      next if grep /^_/, split( /::/, $package ); # private package, ignore

      my $version = $pm_info->version( $package );

      if ( $package eq $prime_package ) {
        if ( exists( $prime{$package} ) ) {
          # M::B::ModuleInfo will handle this conflict
          die "Unexpected conflict in '$package'; multiple versions found.\n";
        } else {
          $prime{$package}{file} = $mapped_filename;
          $prime{$package}{version} = $version if defined( $version );
        }
      } else {
        push( @{$alt{$package}}, {
                                  file    => $mapped_filename,
                                  version => $version,
                                 } );
      }
    }
  }

  # Then we iterate over all the packages found above, identifying conflicts
  # and selecting the "best" candidate for recording the file & version
  # for each package.
  foreach my $package ( keys( %alt ) ) {
    my $result = $self->_resolve_module_versions( $alt{$package} );

    if ( exists( $prime{$package} ) ) { # primary package selected

      if ( $result->{err} ) {
        # Use the selected primary package, but there are conflicting
        # errors among multiple alternative packages that need to be
        # reported
        $self->log_warn(
          "Found conflicting versions for package '$package'\n" .
          "  $prime{$package}{file} ($prime{$package}{version})\n" .
          $result->{err}
        );

      } elsif ( defined( $result->{version} ) ) {
        # There is a primary package selected, and exactly one
        # alternative package

        if ( exists( $prime{$package}{version} ) &&
             defined( $prime{$package}{version} ) ) {
          # Unless the version of the primary package agrees with the
          # version of the alternative package, report a conflict
          if ( $self->compare_versions( $prime{$package}{version}, '!=',
                                        $result->{version} ) ) {
            $self->log_warn(
              "Found conflicting versions for package '$package'\n" .
              "  $prime{$package}{file} ($prime{$package}{version})\n" .
              "  $result->{file} ($result->{version})\n"
            );
          }

        } else {
          # The prime package selected has no version so, we choose to
          # use any alternative package that does have a version
          $prime{$package}{file}    = $result->{file};
          $prime{$package}{version} = $result->{version};
        }

      } else {
        # no alt package found with a version, but we have a prime
        # package so we use it whether it has a version or not
      }

    } else { # No primary package was selected, use the best alternative

      if ( $result->{err} ) {
        $self->log_warn(
          "Found conflicting versions for package '$package'\n" .
          $result->{err}
        );
      }

      # Despite possible conflicting versions, we choose to record
      # something rather than nothing
      $prime{$package}{file}    = $result->{file};
      $prime{$package}{version} = $result->{version}
          if defined( $result->{version} );
    }
  }

  # Normalize versions or delete them if undef/0
  for my $provides ( values %prime ) {
    if ( $provides->{version} ) {
      $provides->{version} = $self->normalize_version( $provides->{version} )
    }
    else {
      delete $provides->{version};
    }
  }

  return \%prime;
}

# separate out some of the conflict resolution logic from
# $self->find_dist_packages(), above, into a helper function.
#
sub _resolve_module_versions {
  my $self = shift;

  my $packages = shift;

  my( $file, $version );
  my $err = '';
    foreach my $p ( @$packages ) {
      if ( defined( $p->{version} ) ) {
        if ( defined( $version ) ) {
          if ( $self->compare_versions( $version, '!=', $p->{version} ) ) {
            $err .= "  $p->{file} ($p->{version})\n";
          } else {
            # same version declared multiple times, ignore
          }
        } else {
          $file    = $p->{file};
          $version = $p->{version};
        }
      }
      $file ||= $p->{file} if defined( $p->{file} );
    }

  if ( $err ) {
    $err = "  $file ($version)\n" . $err;
  }

  my %result = (
    file    => $file,
    version => $version,
    err     => $err
  );

  return \%result;
}

sub make_tarball {
  my ($self, $dir, $file) = @_;
  $file ||= $dir;

  $self->log_info("Creating $file.tar.gz\n");

  if ($self->{args}{tar}) {
    my $tar_flags = $self->verbose ? 'cvf' : 'cf';
    $self->do_system($self->split_like_shell($self->{args}{tar}), $tar_flags, "$file.tar", $dir);
    $self->do_system($self->split_like_shell($self->{args}{gzip}), "$file.tar") if $self->{args}{gzip};
  } else {
    eval { require Archive::Tar && Archive::Tar->VERSION(1.09); 1 }
      or die "You must install Archive::Tar 1.09+ to make a distribution tarball\n".
             "or specify a binary tar program with the '--tar' option.\n".
             "See the documentation for the 'dist' action.\n";

    my $files = $self->rscan_dir($dir);

    # Archive::Tar versions >= 1.09 use the following to enable a compatibility
    # hack so that the resulting archive is compatible with older clients.
    # If no file path is 100 chars or longer, we disable the prefix field
    # for maximum compatibility.  If there are any long file paths then we
    # need the prefix field after all.
    $Archive::Tar::DO_NOT_USE_PREFIX =
      (grep { length($_) >= 100 } @$files) ? 0 : 1;

    my $tar   = Archive::Tar->new;
    $tar->add_files(@$files);
    for my $f ($tar->get_files) {
      $f->mode($f->mode & ~022); # chmod go-w
    }
    $tar->write("$file.tar.gz", 1);
  }
}

sub install_path {
  my $self = shift;
  my( $type, $value ) = ( @_, '<empty>' );

  Carp::croak( 'Type argument missing' )
    unless defined( $type );

  my $map = $self->{properties}{install_path};
  return $map unless @_;

  # delete existing value if $value is literal undef()
  unless ( defined( $value ) ) {
    delete( $map->{$type} );
    return undef;
  }

  # return existing value if no new $value is given
  if ( $value eq '<empty>' ) {
    return undef unless exists $map->{$type};
    return $map->{$type};
  }

  # set value if $value is a valid relative path
  return $map->{$type} = $value;
}

sub install_sets {
  # Usage: install_sets('site'), install_sets('site', 'lib'),
  #   or install_sets('site', 'lib' => $value);
  my ($self, $dirs, $key, $value) = @_;
  $dirs = $self->installdirs unless defined $dirs;
  # update property before merging with defaults
  if ( @_ == 4 && defined $dirs && defined $key) {
    # $value can be undef; will mask default
    $self->{properties}{install_sets}{$dirs}{$key} = $value;
  }
  my $map = { $self->_merge_arglist(
    $self->{properties}{install_sets},
    $self->_default_install_paths->{install_sets}
  )};
  if ( defined $dirs && defined $key ) {
    return $map->{$dirs}{$key};
  }
  elsif ( defined $dirs ) {
    return $map->{$dirs};
  }
  else {
    croak "Can't determine installdirs for install_sets()";
  }
}

sub original_prefix {
  # Usage: original_prefix(), original_prefix('lib'),
  #   or original_prefix('lib' => $value);
  my ($self, $key, $value) = @_;
  # update property before merging with defaults
  if ( @_ == 3 && defined $key) {
    # $value can be undef; will mask default
    $self->{properties}{original_prefix}{$key} = $value;
  }
  my $map = { $self->_merge_arglist(
    $self->{properties}{original_prefix},
    $self->_default_install_paths->{original_prefix}
  )};
  return $map unless defined $key;
  return $map->{$key}
}

sub install_base_relpaths {
  # Usage: install_base_relpaths(), install_base_relpaths('lib'),
  #   or install_base_relpaths('lib' => $value);
  my $self = shift;
  if ( @_ > 1 ) { # change values before merge
    $self->_set_relpaths($self->{properties}{install_base_relpaths}, @_);
  }
  my $map = { $self->_merge_arglist(
    $self->{properties}{install_base_relpaths},
    $self->_default_install_paths->{install_base_relpaths}
  )};
  return $map unless @_;
  my $relpath = $map->{$_[0]};
  return defined $relpath ? File::Spec->catdir( @$relpath ) : undef;
}

# Defaults to use in case the config install paths cannot be prefixified.
sub prefix_relpaths {
  # Usage: prefix_relpaths('site'), prefix_relpaths('site', 'lib'),
  #   or prefix_relpaths('site', 'lib' => $value);
  my $self = shift;
  my $installdirs = shift || $self->installdirs
    or croak "Can't determine installdirs for prefix_relpaths()";
  if ( @_ > 1 ) { # change values before merge
    $self->{properties}{prefix_relpaths}{$installdirs} ||= {};
    $self->_set_relpaths($self->{properties}{prefix_relpaths}{$installdirs}, @_);
  }
  my $map = {$self->_merge_arglist(
    $self->{properties}{prefix_relpaths}{$installdirs},
    $self->_default_install_paths->{prefix_relpaths}{$installdirs}
  )};
  return $map unless @_;
  my $relpath = $map->{$_[0]};
  return defined $relpath ? File::Spec->catdir( @$relpath ) : undef;
}

sub _set_relpaths {
  my $self = shift;
  my( $map, $type, $value ) = @_;

  Carp::croak( 'Type argument missing' )
    unless defined( $type );

  # set undef if $value is literal undef()
  if ( ! defined( $value ) ) {
    $map->{$type} = undef;
    return;
  }
  # set value if $value is a valid relative path
  else {
    Carp::croak( "Value must be a relative path" )
      if File::Spec::Unix->file_name_is_absolute($value);

    my @value = split( /\//, $value );
    $map->{$type} = \@value;
  }
}

# Translated from ExtUtils::MM_Any::init_INSTALL_from_PREFIX
sub prefix_relative {
  my ($self, $type) = @_;
  my $installdirs = $self->installdirs;

  my $relpath = $self->install_sets($installdirs)->{$type};

  return $self->_prefixify($relpath,
                           $self->original_prefix($installdirs),
                           $type,
                          );
}

# Translated from ExtUtils::MM_Unix::prefixify()
sub _prefixify {
  my($self, $path, $sprefix, $type) = @_;

  my $rprefix = $self->prefix;
  $rprefix .= '/' if $sprefix =~ m|/$|;

  $self->log_verbose("  prefixify $path from $sprefix to $rprefix\n")
    if defined( $path ) && length( $path );

  if( !defined( $path ) || ( length( $path ) == 0 ) ) {
    $self->log_verbose("  no path to prefixify, falling back to default.\n");
    return $self->_prefixify_default( $type, $rprefix );
  } elsif( !File::Spec->file_name_is_absolute($path) ) {
    $self->log_verbose("    path is relative, not prefixifying.\n");
  } elsif( $path !~ s{^\Q$sprefix\E\b}{}s ) {
    $self->log_verbose("    cannot prefixify, falling back to default.\n");
    return $self->_prefixify_default( $type, $rprefix );
  }

  $self->log_verbose("    now $path in $rprefix\n");

  return $path;
}

sub _prefixify_default {
  my $self = shift;
  my $type = shift;
  my $rprefix = shift;

  my $default = $self->prefix_relpaths($self->installdirs, $type);
  if( !$default ) {
    $self->log_verbose("    no default install location for type '$type', using prefix '$rprefix'.\n");
    return $rprefix;
  } else {
    return $default;
  }
}

sub install_destination {
  my ($self, $type) = @_;

  return $self->install_path($type) if $self->install_path($type);

  if ( $self->install_base ) {
    my $relpath = $self->install_base_relpaths($type);
    return $relpath ? File::Spec->catdir($self->install_base, $relpath) : undef;
  }

  if ( $self->prefix ) {
    my $relpath = $self->prefix_relative($type);
    return $relpath ? File::Spec->catdir($self->prefix, $relpath) : undef;
  }

  return $self->install_sets($self->installdirs)->{$type};
}

sub install_types {
  my $self = shift;

  my %types;
  if ( $self->install_base ) {
    %types = %{$self->install_base_relpaths};
  } elsif ( $self->prefix ) {
    %types = %{$self->prefix_relpaths};
  } else {
    %types = %{$self->install_sets($self->installdirs)};
  }

  %types = (%types, %{$self->install_path});

  return sort keys %types;
}

sub install_map {
  my ($self, $blib) = @_;
  $blib ||= $self->blib;

  my( %map, @skipping );
  foreach my $type ($self->install_types) {
    my $localdir = File::Spec->catdir( $blib, $type );
    next unless -e $localdir;

    # the line "...next if (($type eq 'bindoc'..." was one of many changes introduced for
    # improving HTML generation on ActivePerl, see https://rt.cpan.org/Public/Bug/Display.html?id=53478
    # Most changes were ok, but this particular line caused test failures in t/manifypods.t on windows,
    # therefore it is commented out.

    # ********* next if (($type eq 'bindoc' || $type eq 'libdoc') && not $self->is_unixish);

    if (my $dest = $self->install_destination($type)) {
      $map{$localdir} = $dest;
    } else {
      push( @skipping, $type );
    }
  }

  $self->log_warn(
    "WARNING: Can't figure out install path for types: @skipping\n" .
    "Files will not be installed.\n"
  ) if @skipping;

  # Write the packlist into the same place as ExtUtils::MakeMaker.
  if ($self->create_packlist and my $module_name = $self->module_name) {
    my $archdir = $self->install_destination('arch');
    my @ext = split /::/, $module_name;
    $map{write} = File::Spec->catfile($archdir, 'auto', @ext, '.packlist');
  }

  # Handle destdir
  if (length(my $destdir = $self->destdir || '')) {
    foreach (keys %map) {
      # Need to remove volume from $map{$_} using splitpath, or else
      # we'll create something crazy like C:\Foo\Bar\E:\Baz\Quux
      # VMS will always have the file separate than the path.
      my ($volume, $path, $file) = File::Spec->splitpath( $map{$_}, 0 );

      # catdir needs a list of directories, or it will create something
      # crazy like volume:[Foo.Bar.volume.Baz.Quux]
      my @dirs = File::Spec->splitdir($path);

      # First merge the directories
      $path = File::Spec->catdir($destdir, @dirs);

      # Then put the file back on if there is one.
      if ($file ne '') {
          $map{$_} = File::Spec->catfile($path, $file)
      } else {
          $map{$_} = $path;
      }
    }
  }

  $map{read} = '';  # To keep ExtUtils::Install quiet

  return \%map;
}

sub depends_on {
  my $self = shift;
  foreach my $action (@_) {
    $self->_call_action($action);
  }
}

sub rscan_dir {
  my ($self, $dir, $pattern) = @_;
  my @result;
  local $_; # find() can overwrite $_, so protect ourselves
  my $subr = !$pattern ? sub {push @result, $File::Find::name} :
             !ref($pattern) || (ref $pattern eq 'Regexp') ? sub {push @result, $File::Find::name if /$pattern/} :
             ref($pattern) eq 'CODE' ? sub {push @result, $File::Find::name if $pattern->()} :
             die "Unknown pattern type";

  File::Find::find({wanted => $subr, no_chdir => 1}, $dir);
  return \@result;
}

sub delete_filetree {
  my $self = shift;
  my $deleted = 0;
  foreach (@_) {
    next unless -e $_;
    $self->log_verbose("Deleting $_\n");
    File::Path::rmtree($_, 0, 0);
    die "Couldn't remove '$_': $!\n" if -e $_;
    $deleted++;
  }
  return $deleted;
}

sub autosplit_file {
  my ($self, $file, $to) = @_;
  require AutoSplit;
  my $dir = File::Spec->catdir($to, 'lib', 'auto');
  AutoSplit::autosplit($file, $dir);
}

sub cbuilder {
  # Returns a CBuilder object

  my $self = shift;
  my $s = $self->{stash};
  return $s->{_cbuilder} if $s->{_cbuilder};

  require ExtUtils::CBuilder;
  return $s->{_cbuilder} = ExtUtils::CBuilder->new(
    config => $self->config,
    ($self->quiet ? (quiet => 1 ) : ()),
  );
}

sub have_c_compiler {
  my ($self) = @_;

  my $p = $self->{properties};
  return $p->{_have_c_compiler} if defined $p->{_have_c_compiler};

  $self->log_verbose("Checking if compiler tools configured... ");
  my $b = $self->cbuilder;
  my $have = $b && eval { $b->have_compiler };
  $self->log_verbose($have ? "ok.\n" : "failed.\n");
  return $p->{_have_c_compiler} = $have;
}

sub compile_c {
  my ($self, $file, %args) = @_;

  if ( ! $self->have_c_compiler ) {
    die "Error: no compiler detected to compile '$file'.  Aborting\n";
  }

  my $b = $self->cbuilder;
  my $obj_file = $b->object_file($file);
  $self->add_to_cleanup($obj_file);
  return $obj_file if $self->up_to_date($file, $obj_file);

  $b->compile(source => $file,
              defines => $args{defines},
              object_file => $obj_file,
              include_dirs => $self->include_dirs,
              extra_compiler_flags => $self->extra_compiler_flags,
             );

  return $obj_file;
}

sub link_c {
  my ($self, $spec) = @_;
  my $p = $self->{properties}; # For convenience

  $self->add_to_cleanup($spec->{lib_file});

  my $objects = $p->{objects} || [];

  return $spec->{lib_file}
    if $self->up_to_date([$spec->{obj_file}, @$objects],
                         $spec->{lib_file});

  my $module_name = $spec->{module_name} || $self->module_name;

  $self->cbuilder->link(
    module_name => $module_name,
    objects     => [$spec->{obj_file}, @$objects],
    lib_file    => $spec->{lib_file},
    extra_linker_flags => $p->{extra_linker_flags} );

  return $spec->{lib_file};
}

sub compile_xs {
  my ($self, $file, %args) = @_;

  $self->log_verbose("$file -> $args{outfile}\n");

  if (eval {require ExtUtils::ParseXS; 1}) {

    ExtUtils::ParseXS::process_file(
                                    filename => $file,
                                    prototypes => 0,
                                    output => $args{outfile},
                                   );
  } else {
    # Ok, I give up.  Just use backticks.

    my $xsubpp = Module::Build::ModuleInfo->find_module_by_name('ExtUtils::xsubpp')
      or die "Can't find ExtUtils::xsubpp in INC (@INC)";

    my @typemaps;
    push @typemaps, Module::Build::ModuleInfo->find_module_by_name(
        'ExtUtils::typemap', \@INC
    );
    my $lib_typemap = Module::Build::ModuleInfo->find_module_by_name(
        'typemap', [File::Basename::dirname($file), File::Spec->rel2abs('.')]
    );
    push @typemaps, $lib_typemap if $lib_typemap;
    @typemaps = map {+'-typemap', $_} @typemaps;

    my $cf = $self->{config};
    my $perl = $self->{properties}{perl};

    my @command = ($perl, "-I".$cf->get('installarchlib'), "-I".$cf->get('installprivlib'), $xsubpp, '-noprototypes',
                   @typemaps, $file);

    $self->log_info("@command\n");
    open(my $fh, '>', $args{outfile}) or die "Couldn't write $args{outfile}: $!";
    print {$fh} $self->_backticks(@command);
    close $fh;
  }
}

sub split_like_shell {
  my ($self, $string) = @_;

  return () unless defined($string);
  return @$string if UNIVERSAL::isa($string, 'ARRAY');
  $string =~ s/^\s+|\s+$//g;
  return () unless length($string);

  return Text::ParseWords::shellwords($string);
}

sub oneliner {
  # Returns a string that the shell can evaluate as a perl command.
  # This should be avoided whenever possible, since "the shell" really
  # means zillions of shells on zillions of platforms and it's really
  # hard to get it right all the time.

  # Some of this code is stolen with permission from ExtUtils::MakeMaker.

  my($self, $cmd, $switches, $args) = @_;
  $switches = [] unless defined $switches;
  $args = [] unless defined $args;

  # Strip leading and trailing newlines
  $cmd =~ s{^\n+}{};
  $cmd =~ s{\n+$}{};

  my $perl = ref($self) ? $self->perl : $self->find_perl_interpreter;
  return $self->_quote_args($perl, @$switches, '-e', $cmd, @$args);
}

sub run_perl_script {
  my ($self, $script, $preargs, $postargs) = @_;
  foreach ($preargs, $postargs) {
    $_ = [ $self->split_like_shell($_) ] unless ref();
  }
  return $self->run_perl_command([@$preargs, $script, @$postargs]);
}

sub run_perl_command {
  # XXX Maybe we should accept @args instead of $args?  Must resolve
  # this before documenting.
  my ($self, $args) = @_;
  $args = [ $self->split_like_shell($args) ] unless ref($args);
  my $perl = ref($self) ? $self->perl : $self->find_perl_interpreter;

  # Make sure our local additions to @INC are propagated to the subprocess
  local $ENV{PERL5LIB} = join $self->config('path_sep'), $self->_added_to_INC;

  return $self->do_system($perl, @$args);
}

# Infer various data from the path of the input filename
# that is needed to create output files.
# The input filename is expected to be of the form:
#   lib/Module/Name.ext or Module/Name.ext
sub _infer_xs_spec {
  my $self = shift;
  my $file = shift;

  my $cf = $self->{config};

  my %spec;

  my( $v, $d, $f ) = File::Spec->splitpath( $file );
  my @d = File::Spec->splitdir( $d );
  (my $file_base = $f) =~ s/\.[^.]+$//i;

  $spec{base_name} = $file_base;

  $spec{src_dir} = File::Spec->catpath( $v, $d, '' );

  # the module name
  shift( @d ) while @d && ($d[0] eq 'lib' || $d[0] eq '');
  pop( @d ) while @d && $d[-1] eq '';
  $spec{module_name} = join( '::', (@d, $file_base) );

  $spec{archdir} = File::Spec->catdir($self->blib, 'arch', 'auto',
                                      @d, $file_base);

  $spec{c_file} = File::Spec->catfile( $spec{src_dir},
                                       "${file_base}.c" );

  $spec{obj_file} = File::Spec->catfile( $spec{src_dir},
                                         "${file_base}".$cf->get('obj_ext') );

  require DynaLoader;
  my $modfname = defined &DynaLoader::mod2fname ? DynaLoader::mod2fname([@d, $file_base]) : $file_base;

  $spec{bs_file} = File::Spec->catfile($spec{archdir}, "$modfname.bs");

  $spec{lib_file} = File::Spec->catfile($spec{archdir}, "$modfname.".$cf->get('dlext'));

  return \%spec;
}

sub process_xs {
  my ($self, $file) = @_;

  my $spec = $self->_infer_xs_spec($file);

  # File name, minus the suffix
  (my $file_base = $file) =~ s/\.[^.]+$//;

  # .xs -> .c
  $self->add_to_cleanup($spec->{c_file});

  unless ($self->up_to_date($file, $spec->{c_file})) {
    $self->compile_xs($file, outfile => $spec->{c_file});
  }

  # .c -> .o
  my $v = $self->dist_version;
  $self->compile_c($spec->{c_file},
                   defines => {VERSION => qq{"$v"}, XS_VERSION => qq{"$v"}});

  # archdir
  File::Path::mkpath($spec->{archdir}, 0, oct(777)) unless -d $spec->{archdir};

  # .xs -> .bs
  $self->add_to_cleanup($spec->{bs_file});
  unless ($self->up_to_date($file, $spec->{bs_file})) {
    require ExtUtils::Mkbootstrap;
    $self->log_info("ExtUtils::Mkbootstrap::Mkbootstrap('$spec->{bs_file}')\n");
    ExtUtils::Mkbootstrap::Mkbootstrap($spec->{bs_file});  # Original had $BSLOADLIBS - what's that?
    open(my $fh, '>>', $spec->{bs_file});  # create
    utime((time)x2, $spec->{bs_file});  # touch
  }

  # .o -> .(a|bundle)
  $self->link_c($spec);
}

sub do_system {
  my ($self, @cmd) = @_;
  $self->log_verbose("@cmd\n");

  # Some systems proliferate huge PERL5LIBs, try to ameliorate:
  my %seen;
  my $sep = $self->config('path_sep');
  local $ENV{PERL5LIB} =
    ( !exists($ENV{PERL5LIB}) ? '' :
      length($ENV{PERL5LIB}) < 500
      ? $ENV{PERL5LIB}
      : join $sep, grep { ! $seen{$_}++ and -d $_ } split($sep, $ENV{PERL5LIB})
    );

  my $status = system(@cmd);
  if ($status and $! =~ /Argument list too long/i) {
    my $env_entries = '';
    foreach (sort keys %ENV) { $env_entries .= "$_=>".length($ENV{$_})."; " }
    warn "'Argument list' was 'too long', env lengths are $env_entries";
  }
  return !$status;
}

sub copy_if_modified {
  my $self = shift;
  my %args = (@_ > 3
              ? ( @_ )
              : ( from => shift, to_dir => shift, flatten => shift )
             );
  $args{verbose} = !$self->quiet
    unless exists $args{verbose};

  my $file = $args{from};
  unless (defined $file and length $file) {
    die "No 'from' parameter given to copy_if_modified";
  }

  # makes no sense to replicate an absolute path, so assume flatten
  $args{flatten} = 1 if File::Spec->file_name_is_absolute( $file );

  my $to_path;
  if (defined $args{to} and length $args{to}) {
    $to_path = $args{to};
  } elsif (defined $args{to_dir} and length $args{to_dir}) {
    $to_path = File::Spec->catfile( $args{to_dir}, $args{flatten}
                                    ? File::Basename::basename($file)
                                    : $file );
  } else {
    die "No 'to' or 'to_dir' parameter given to copy_if_modified";
  }

  return if $self->up_to_date($file, $to_path); # Already fresh

  {
    local $self->{properties}{quiet} = 1;
    $self->delete_filetree($to_path); # delete destination if exists
  }

  # Create parent directories
  File::Path::mkpath(File::Basename::dirname($to_path), 0, oct(777));

  $self->log_verbose("Copying $file -> $to_path\n");

  if ($^O eq 'os2') {# copy will not overwrite; 0x1 = overwrite
    chmod 0666, $to_path;
    File::Copy::syscopy($file, $to_path, 0x1) or die "Can't copy('$file', '$to_path'): $!";
  } else {
    File::Copy::copy($file, $to_path) or die "Can't copy('$file', '$to_path'): $!";
  }

  # mode is read-only + (executable if source is executable)
  my $mode = oct(444) | ( $self->is_executable($file) ? oct(111) : 0 );
  chmod( $mode, $to_path );

  return $to_path;
}

sub up_to_date {
  my ($self, $source, $derived) = @_;
  $source  = [$source]  unless ref $source;
  $derived = [$derived] unless ref $derived;

  # empty $derived means $source should always run
  return 0 if @$source && !@$derived || grep {not -e} @$derived;

  my $most_recent_source = time / (24*60*60);
  foreach my $file (@$source) {
    unless (-e $file) {
      $self->log_warn("Can't find source file $file for up-to-date check");
      next;
    }
    $most_recent_source = -M _ if -M _ < $most_recent_source;
  }

  foreach my $derived (@$derived) {
    return 0 if -M $derived > $most_recent_source;
  }
  return 1;
}

sub dir_contains {
  my ($self, $first, $second) = @_;
  # File::Spec doesn't have an easy way to check whether one directory
  # is inside another, unfortunately.

  ($first, $second) = map File::Spec->canonpath($_), ($first, $second);
  my @first_dirs = File::Spec->splitdir($first);
  my @second_dirs = File::Spec->splitdir($second);

  return 0 if @second_dirs < @first_dirs;

  my $is_same = ( $self->_case_tolerant
                  ? sub {lc(shift()) eq lc(shift())}
                  : sub {shift() eq shift()} );

  while (@first_dirs) {
    return 0 unless $is_same->(shift @first_dirs, shift @second_dirs);
  }

  return 1;
}

1;
__END__


=head1 NAME

Module::Build::Base - Default methods for Module::Build

=head1 SYNOPSIS

  Please see the Module::Build documentation.

=head1 DESCRIPTION

The C<Module::Build::Base> module defines the core functionality of
C<Module::Build>.  Its methods may be overridden by any of the
platform-dependent modules in the C<Module::Build::Platform::>
namespace, but the intention here is to make this base module as
platform-neutral as possible.  Nicely enough, Perl has several core
tools available in the C<File::> namespace for doing this, so the task
isn't very difficult.

Please see the C<Module::Build> documentation for more details.

=head1 AUTHOR

Ken Williams <kwilliams@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2001-2006 Ken Williams.  All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

perl(1), Module::Build(3)

=cut
