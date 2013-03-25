#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest;
use File::Spec::Functions qw/catdir catfile/;

#--------------------------------------------------------------------------#
# Begin testing
#--------------------------------------------------------------------------#

plan tests => 23;

blib_load('Module::Build');

#--------------------------------------------------------------------------#
# Create test distribution
#--------------------------------------------------------------------------#

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp, name => 'Simple::Share' );
$dist->regen;
$dist->chdir_in;

#--------------------------------------------------------------------------#
# Test setting 'share_dir'
#--------------------------------------------------------------------------#

my $mb = $dist->new_from_context;

# Test without a 'share' dir
ok( $mb, "Created Module::Build object" );
is( $mb->share_dir, undef,
  "default share_dir undef if no 'share' dir exists"
);
ok( ! exists $mb->{properties}{requires}{'File::ShareDir'},
  "File::ShareDir not added to 'requires'"
);

# Add 'share' dir and an 'other' dir and content
$dist->add_file('share/foo.txt',<< '---');
This is foo.txt
---
$dist->add_file('share/subdir/share/anotherbar.txt',<< '---');
This is anotherbar.txt in a subdir - test for a bug in M::B 0.38 when full path contains 'share/.../*share/...' subdir
---
$dist->add_file('share/subdir/whatever/anotherfoo.txt',<< '---');
This is anotherfoo.txt in a subdir - this shoud work on M::B 0.38
---
$dist->add_file('other/share/bar.txt',<< '---');
This is bar.txt
---
$dist->regen;
ok( -e catfile(qw/share foo.txt/), "Created 'share' directory" );
ok( -d catfile(qw/share subdir share/), "Created 'share/subdir/share' directory" );
ok( -d catfile(qw/share subdir whatever/), "Created 'share/subdir/whatever' directory" );
ok( -e catfile(qw/other share bar.txt/), "Created 'other/share' directory" );

# Check default when share_dir is not given
stdout_stderr_of( sub { $mb = $dist->new_from_context });
is( $mb->share_dir, undef,
  "Default share_dir is undef even if 'share' exists"
);
ok( ! exists $mb->{properties}{requires}{'File::ShareDir'},
  "File::ShareDir not added to 'requires'"
);


# share_dir set to scalar
$dist->change_build_pl(
  {
    module_name         => $dist->name,
    license             => 'perl',
    share_dir           => 'share',
  }
);
$dist->regen;
stdout_stderr_of( sub { $mb = $dist->new_from_context });
is_deeply( $mb->share_dir, { dist => [ 'share' ] },
  "Scalar share_dir set as dist-type share"
);

# share_dir set to arrayref
$dist->change_build_pl(
  {
    module_name         => $dist->name,
    license             => 'perl',
    share_dir           => [ 'share' ],
  }
);
$dist->regen;
stdout_stderr_of( sub { $mb = $dist->new_from_context });
is_deeply( $mb->share_dir, { dist => [ 'share' ] },
  "Arrayref share_dir set as dist-type share"
);

# share_dir set to hashref w scalar
$dist->change_build_pl(
  {
    module_name         => $dist->name,
    license             => 'perl',
    share_dir           => { dist => 'share' },
  }
);
$dist->regen;
stdout_stderr_of( sub { $mb = $dist->new_from_context });
is_deeply( $mb->share_dir, { dist => [ 'share' ] },
  "Hashref share_dir w/ scalar dist set as dist-type share"
);

# share_dir set to hashref w array
$dist->change_build_pl(
  {
    module_name         => $dist->name,
    license             => 'perl',
    share_dir           => { dist => [ 'share' ] },
  }
);
$dist->regen;
stdout_stderr_of( sub { $mb = $dist->new_from_context });
is_deeply( $mb->share_dir, { dist => [ 'share' ] },
  "Hashref share_dir w/ arrayref dist set as dist-type share"
);

# Generate a module sharedir (scalar)
$dist->change_build_pl(
  {
    module_name         => $dist->name,
    license             => 'perl',
    share_dir           => {
      dist => 'share',
      module => { $dist->name =>  'other/share'  },
    },
  }
);
$dist->regen;
stdout_stderr_of( sub { $mb = $dist->new_from_context });
is_deeply( $mb->share_dir,
  { dist => [ 'share' ],
    module => { $dist->name => ['other/share']  },
  },
  "Hashref share_dir w/ both dist and module shares (scalar-form)"
);

# Generate a module sharedir (array)
$dist->change_build_pl(
  {
    module_name         => $dist->name,
    license             => 'perl',
    share_dir           => {
      dist => [ 'share' ],
      module => { $dist->name =>  ['other/share']  },
    },
  }
);
$dist->regen;
stdout_stderr_of( sub { $mb = $dist->new_from_context });
is_deeply( $mb->share_dir,
  { dist => [ 'share' ],
    module => { $dist->name => ['other/share']  },
  },
  "Hashref share_dir w/ both dist and module shares (array-form)"
);

#--------------------------------------------------------------------------#
# test constructing to/from mapping
#--------------------------------------------------------------------------#

is_deeply( $mb->_find_share_dir_files,
  {
    "share/foo.txt" => "dist/Simple-Share/foo.txt",
    "share/subdir/share/anotherbar.txt" => "dist/Simple-Share/subdir/share/anotherbar.txt",
    "share/subdir/whatever/anotherfoo.txt" => "dist/Simple-Share/subdir/whatever/anotherfoo.txt",
    "other/share/bar.txt" => "module/Simple-Share/bar.txt",
  },
  "share_dir filemap for copying to lib complete"
);

#--------------------------------------------------------------------------#
# test moving files to blib
#--------------------------------------------------------------------------#

$mb->dispatch('build');

ok( -d 'blib', "Build ran and blib exists" );
ok( -d 'blib/lib/auto/share', "blib/lib/auto/share exists" );

my $share_list = Module::Build->rscan_dir('blib/lib/auto/share', sub {-f});

SKIP:
{

skip 'filename case not necessarily preserved', 1 if $^O eq 'VMS';

is_deeply(
  [ sort @$share_list ], [
    'blib/lib/auto/share/dist/Simple-Share/foo.txt',
    'blib/lib/auto/share/dist/Simple-Share/subdir/share/anotherbar.txt',
    'blib/lib/auto/share/dist/Simple-Share/subdir/whatever/anotherfoo.txt',
    'blib/lib/auto/share/module/Simple-Share/bar.txt',
  ],
  "share_dir files copied to blib"
);

}

#--------------------------------------------------------------------------#
# test installing
#--------------------------------------------------------------------------#

my $temp_install = 'temp_install';
mkdir $temp_install;
ok( -d $temp_install, "temp install dir created" );

$mb->install_base($temp_install);
stdout_of( sub { $mb->dispatch('install') } );

$share_list = Module::Build->rscan_dir(
  "$temp_install/lib/perl5/auto/share", sub {-f}
);

SKIP:
{

skip 'filename case not necessarily preserved', 1 if $^O eq 'VMS';

is_deeply(
  [ sort @$share_list ], [
    "$temp_install/lib/perl5/auto/share/dist/Simple-Share/foo.txt",
    "$temp_install/lib/perl5/auto/share/dist/Simple-Share/subdir/share/anotherbar.txt",
    "$temp_install/lib/perl5/auto/share/dist/Simple-Share/subdir/whatever/anotherfoo.txt",
    "$temp_install/lib/perl5/auto/share/module/Simple-Share/bar.txt",
  ],
  "share_dir files correctly installed"
);

}

#--------------------------------------------------------------------------#
# test with File::ShareDir
#--------------------------------------------------------------------------#

SKIP: {
  eval { require File::ShareDir; File::ShareDir->VERSION(1.00) };
  skip "needs File::ShareDir 1.00", 2 if $@;

  unshift @INC, File::Spec->catdir($temp_install, qw/lib perl5/);
  require Simple::Share;

  eval {File::ShareDir::dist_file('Simple-Share','foo.txt') };
  is( $@, q{}, "Found shared dist file" );

  eval {File::ShareDir::module_file('Simple::Share','bar.txt') };
  is( $@, q{}, "Found shared module file" );
}
