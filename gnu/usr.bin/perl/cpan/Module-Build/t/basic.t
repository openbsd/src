#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 58;

blib_load('Module::Build');

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

$dist->chdir_in;

#########################


# Test object creation
{
  my $mb = Module::Build->new( module_name => $dist->name );
  ok $mb;
  is $mb->module_name, $dist->name;
  is $mb->build_class, 'Module::Build';
  is $mb->dist_name, $dist->name;

  $mb = Module::Build->new( dist_name => $dist->name, dist_version => 7 );
  ok $mb;
  ok $mb->module_name;  # Set via heuristics
  is $mb->dist_name, $dist->name;
}

# Make sure actions are defined, and known_actions works as class method
{
  my %actions = map {$_, 1} Module::Build->known_actions;
  ok $actions{clean};
  ok $actions{distdir};
}

# Test prerequisite checking
{
  local @INC = (File::Spec->catdir( $dist->dirname, 'lib' ), @INC);
  my $flagged = 0;
  local $SIG{__WARN__} = sub { $flagged = 1 if $_[0] =~ /@{[$dist->name]}/};
  my $mb = Module::Build->new(
    module_name => $dist->name,
    requires    => {$dist->name => 0},
  );
  ok ! $flagged;
  ok ! $mb->prereq_failures;
  $mb->dispatch('realclean');
  $dist->clean;

  $flagged = 0;
  $mb = Module::Build->new(
    module_name => $dist->name,
    requires    => {$dist->name => 3.14159265},
  );
  ok $flagged;
  ok $mb->prereq_failures;
  ok $mb->prereq_failures->{requires}{$dist->name};
  is $mb->prereq_failures->{requires}{$dist->name}{have}, "0.01";
  is $mb->prereq_failures->{requires}{$dist->name}{need}, "3.14159265";

  $mb->dispatch('realclean');
  $dist->clean;

  # Make sure check_installed_status() works as a class method
  my $info = Module::Build->check_installed_status('File::Spec', 0);
  ok $info->{ok};
  is $info->{have}, $File::Spec::VERSION;

  # Make sure check_installed_status() works with an advanced spec
  $info = Module::Build->check_installed_status('File::Spec', '> 0');
  ok $info->{ok};

  # Use 2 lines for this, to avoid a "used only once" warning
  local $Foo::Module::VERSION;
  $Foo::Module::VERSION = '1.01_02';

  $info = Module::Build->check_installed_status('Foo::Module', '1.01_02');
  ok $info->{ok} or diag($info->{message});
}

{
  # Make sure the correct warning message is generated when an
  # optional prereq isn't installed
  my $flagged = 0;
  local $SIG{__WARN__} = sub { $flagged = 1 if $_[0] =~ /ModuleBuildNonExistent is not installed/};

  my $mb = Module::Build->new(
    module_name => $dist->name,
    recommends  => {ModuleBuildNonExistent => 3},
  );
  ok $flagged;
  $dist->clean;
}

# Test verbosity
{
  my $mb = Module::Build->new(module_name => $dist->name);

  $mb->add_to_cleanup('save_out');
  # Use uc() so we don't confuse the current test output
  like uc(stdout_of( sub {$mb->dispatch('test', verbose => 1)} )), qr/^OK \d/m;
  like uc(stdout_of( sub {$mb->dispatch('test', verbose => 0)} )), qr/\.\. ?OK/;

  $mb->dispatch('realclean');
  $dist->clean;
}

# Make sure 'config' entries are respected on the command line, and that
# Getopt::Long specs work as expected.
{
  use Config;
  $dist->change_build_pl
    ({
      module_name => @{[$dist->name]},
      license     => 'perl',
      get_options => { foo => {},
		       bar => { type    => '+'  },
		       bat => { type    => '=s' },
		       dee => { type    => '=s',
				default => 'goo'
			      },
		     }
     });

  $dist->regen;
  eval {Module::Build->run_perl_script('Build.PL', [], ['--nouse-rcfile', '--config', "foocakes=barcakes", '--foo', '--bar', '--bar', '-bat=hello', 'gee=whiz', '--any', 'hey', '--destdir', 'yo', '--verbose', '1'])};
  is $@, '';

  my $mb = Module::Build->resume;
  ok $mb->valid_property('config');

  is $mb->config('cc'), $Config{cc};
  is $mb->config('foocakes'), 'barcakes';

  # Test args().
  is $mb->args('foo'), 1;
  is $mb->args('bar'), 2, 'bar';
  is $mb->args('bat'), 'hello', 'bat';
  is $mb->args('gee'), 'whiz';
  is $mb->args('any'), 'hey';
  is $mb->args('dee'), 'goo';
  is $mb->destdir, 'yo';
  my %runtime = $mb->runtime_params;
  is_deeply \%runtime,
    {
     verbose => 1,
     destdir => 'yo',
     use_rcfile => 0,
     config => { foocakes => 'barcakes' },
    };

  ok my $argsref = $mb->args;
  is $argsref->{foo}, 1;
  $argsref->{doo} = 'hee';
  is $mb->args('doo'), 'hee';
  ok my %args = $mb->args;
  is $args{foo}, 1;

  # revert test distribution to pristine state because we modified a file
  $dist->regen( clean => 1 );
}

# Test author stuff
{
  my $mb = Module::Build->new(
    module_name => $dist->name,
    dist_author => 'Foo Meister <foo@example.com>',
    build_class => 'My::Big::Fat::Builder',
  );
  ok $mb;
  ok ref($mb->dist_author), 'dist_author converted to array if simple string';
  is $mb->dist_author->[0], 'Foo Meister <foo@example.com>';
  is $mb->build_class, 'My::Big::Fat::Builder';
}

# Test conversion of shell strings
{
  my $mb = Module::Build->new(
    module_name => $dist->name,
    dist_author => 'Foo Meister <foo@example.com>',
    extra_compiler_flags => '-I/foo -I/bar',
    extra_linker_flags => '-L/foo -L/bar',
  );
  ok $mb;
  is_deeply $mb->extra_compiler_flags, ['-I/foo', '-I/bar'], "Should split shell string into list";
  is_deeply $mb->extra_linker_flags,   ['-L/foo', '-L/bar'], "Should split shell string into list";

  # Try again with command-line args
  eval {Module::Build->run_perl_script('Build.PL', [], ['--extra_compiler_flags', '-I/foo -I/bar',
							'--extra_linker_flags', '-L/foo -L/bar'])};
  $mb = Module::Build->resume;
  ok $mb;
  is_deeply $mb->extra_compiler_flags, ['-I/foo', '-I/bar'], "Should split shell string into list";
  is_deeply $mb->extra_linker_flags,   ['-L/foo', '-L/bar'], "Should split shell string into list";
}

# Test include_dirs.
{
  ok my $mb = Module::Build->new(
    module_name => $dist->name,
    include_dirs => [qw(/foo /bar)],
  );
  is_deeply $mb->include_dirs, ['/foo', '/bar'], 'Should have include dirs';

  # Try a string.
  ok $mb = Module::Build->new(
    module_name => $dist->name,
    include_dirs => '/foo',
  );
  is_deeply $mb->include_dirs, ['/foo'], 'Should have string include dir';

  # Try again with command-line args
  eval { Module::Build->run_perl_script(
      'Build.PL', [],
      ['--include_dirs', '/foo', '--include_dirs', '/bar' ],
  ) };

  ok $mb = Module::Build->resume;
  is_deeply $mb->include_dirs, ['/foo', '/bar'], 'Should have include dirs';

  eval { Module::Build->run_perl_script(
      'Build.PL', [],
      ['--include_dirs', '/foo' ],
  ) };

  ok $mb = Module::Build->resume;
  is_deeply $mb->include_dirs, ['/foo'], 'Should have single include dir';
}

