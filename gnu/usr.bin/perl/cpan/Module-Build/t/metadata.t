#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 14;

blib_load('Module::Build');
blib_load('Module::Build::ConfigData');

my $tmp = MBTest->tmpdir;

my %metadata =
  (
   module_name   => 'Simple',
   dist_version  => '3.14159265',
   dist_author   => [ 'Simple Simon <ss\@somewhere.priv>' ],
   dist_abstract => 'Something interesting',
   test_requires => {
       'Test::More' => 0.49,
   },
   license       => 'perl',
   meta_add => {
		keywords  => [qw(super duper something)],
		resources => {homepage => 'http://foo.example.com'},
	       },
  );


use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->change_build_pl( \%metadata );
$dist->regen;

my $simple_file = 'lib/Simple.pm';
my $simple2_file = 'lib/Simple2.pm';

   # Traditional VMS will return the file in in lower case, and is_deeply
   # does exact case comparisons.
   # When ODS-5 support is active for preserved case file names we do not
   # change the case.
   if ($^O eq 'VMS') {
       my $lower_case_expect = 1;
       my $vms_efs_case = 0;
       if (eval 'require VMS::Feature') {
           $vms_efs_case = VMS::Feature::current("efs_case_preserve");
       } else {
           my $efs_case = $ENV{'DECC$EFS_CASE_PRESERVE'} || '';
           $vms_efs_case = $efs_case =~ /^[ET1]/i;
       }
       $lower_case_expect = 0 if $vms_efs_case;
       if ($lower_case_expect) {
           $simple_file = lc($simple_file);
           $simple2_file = lc($simple2_file);
       }
   }


$dist->chdir_in;

my $mb = Module::Build->new_from_context;

##################################################
#
# Test for valid META.yml

{
  my $mb_prereq = { 'Module::Build' => $Module::Build::VERSION };
  my $mb_config_req = {
    'Module::Build' => sprintf '%2.2f', int($Module::Build::VERSION * 100)/100
  };
  my $node;
  my $output = stdout_stderr_of( sub {
    $node = $mb->get_metadata( auto => 1 );
  });
  like( $output, qr/Module::Build was not found in configure_requires/,
    "saw warning about M::B not in configure_requires"
  );

  # exists() doesn't seem to work here
  is $node->{name}, $metadata{module_name};
  is $node->{version}, $metadata{dist_version};
  is $node->{abstract}, $metadata{dist_abstract};
  is_deeply $node->{author}, $metadata{dist_author};
  is_deeply $node->{license}, [ 'perl_5' ];
  is_deeply $node->{prereqs}{configure}{requires}, $mb_config_req, 'Add M::B to configure_requires';
  is_deeply $node->{prereqs}{test}{requires}, {
      'Test::More' => '0.49',
  }, 'Test::More was required by ->new';
  like $node->{generated_by}, qr{Module::Build};
  ok defined( $node->{'meta-spec'}{version} ),
      "'meta-spec' -> 'version' field present in META.yml";
  ok defined( $node->{'meta-spec'}{url} ),
      "'meta-spec' -> 'url' field present in META.yml";
  is_deeply $node->{keywords}, $metadata{meta_add}{keywords};
  is_deeply $node->{resources}, $metadata{meta_add}{resources};
}

{
  my $mb_prereq = { 'Module::Build' => 0 };
  $mb->configure_requires( $mb_prereq );
  my $node = $mb->get_metadata( auto => 1 );


  # exists() doesn't seem to work here
  is_deeply $node->{prereqs}{configure}{requires}, $mb_prereq, 'Add M::B to configure_requires';
}

$dist->clean;

