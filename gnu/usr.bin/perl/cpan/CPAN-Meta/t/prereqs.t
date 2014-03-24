use strict;
use warnings;
use Test::More 0.88;

use CPAN::Meta::Prereqs;

delete $ENV{$_} for qw/PERL_JSON_BACKEND PERL_YAML_BACKEND/; # use defaults

my $prereq_struct = {
  runtime => {
    requires => {
      'Config' => 0,
      'Cwd'    => 0,
      'Data::Dumper' => 0,
      'ExtUtils::Install' => 0,
      'File::Basename' => 0,
      'File::Compare'  => 0,
      'File::Copy' => 0,
      'File::Find' => 0,
      'File::Path' => 0,
      'File::Spec' => 0,
      'IO::File'   => 0,
      'perl'       => '5.005_03',
    },
    recommends => {
      'Archive::Tar' => '1.00',
      'ExtUtils::Install' => 0.3,
      'ExtUtils::ParseXS' => 2.02,
      'Pod::Text' => 0,
      'YAML' => 0.35,
    },
  },
  build => {
    requires => {
      'Test' => 0,
    },
  }
};

my $prereq = CPAN::Meta::Prereqs->new($prereq_struct);

isa_ok($prereq, 'CPAN::Meta::Prereqs');

is_deeply($prereq->as_string_hash, $prereq_struct, "round-trip okay");

{
  my $req = $prereq->requirements_for(qw(runtime requires));
  my @req_mod = $req->required_modules;

  ok(
    (grep { 'Cwd' eq $_ } @req_mod),
    "we got the runtime requirements",
  );

  ok(
    (! grep { 'YAML' eq $_ } @req_mod),
    "...but not the runtime recommendations",
  );

  ok(
    (! grep { 'Test' eq $_ } @req_mod),
    "...nor the build requirements",
  );
}

{
  my $req = $prereq->requirements_for(qw(runtime requires));
  my $rec = $prereq->requirements_for(qw(runtime recommends));

  my $merged = $req->clone->add_requirements($rec);

  my @req_mod = $merged->required_modules;

  ok(
    (grep { 'Cwd' eq $_ } @req_mod),
    "we got the runtime requirements",
  );

  ok(
    (grep { 'YAML' eq $_ } @req_mod),
    "...and the runtime recommendations",
  );

  ok(
    (! grep { 'Test' eq $_ } @req_mod),
    "...but not the build requirements",
  );

}

{
  my $req = $prereq->requirements_for(qw(runtime suggests));
  my @req_mod = $req->required_modules;

  is(@req_mod, 0, "empty set of runtime/suggests requirements");
}

{
  my $req = $prereq->requirements_for(qw(develop suggests));
  my @req_mod = $req->required_modules;

  is(@req_mod, 0, "empty set of develop/suggests requirements");
}

{
  my $new_prereq = CPAN::Meta::Prereqs->new;

  $new_prereq
    ->requirements_for(qw(runtime requires))
    ->add_minimum(Foo => '1.000');

  $new_prereq
    ->requirements_for(qw(runtime requires))
    ->add_minimum(Bar => '2.976');

  is_deeply(
    $new_prereq->as_string_hash,
    { runtime => { requires => { Foo => '1.000', Bar => '2.976' } } },
    'we can accumulate new requirements on a prereq object',
  );
}

done_testing;

