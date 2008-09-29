#!perl -w
use strict;
use Module::CoreList;
use Test::More tests => 13;

BEGIN { require_ok('Module::CoreList'); }

ok($Module::CoreList::version{5.00503},    "5.00503");

ok(!exists $Module::CoreList::version{5.00503}{attributes},
   "attributes weren't in 5.00503");

ok($Module::CoreList::version{5.006001},    "5.006001");

ok(exists $Module::CoreList::version{'5.006001'}{attributes},
   "attributes were in 5.6.1");

ok($Module::CoreList::version{5.007003},    "5.007003");

ok(exists $Module::CoreList::version{5.007003}{'Attribute::Handlers'},
   "Attribute::Handlers were bundled with 5.7.3");

is(Module::CoreList->first_release_by_date('File::Spec'), 5.005,
   "File::Spec was first bundled in 5.005");

is(Module::CoreList->first_release('File::Spec'), 5.00405,
   "File::Spec was released in perl with lowest version number 5.00405");

is(Module::CoreList->first_release('File::Spec', 0.82), 5.006_001,
   "File::Spec reached 0.82 with 5.006_001");

is_deeply([ sort keys %Module::CoreList::released ],
          [ sort keys %Module::CoreList::version ],
          "have a note of everythings release");

is_deeply( [ map {
    exists $Module::CoreList::version{ $_ }{FindExt} ? $_ : ()
} keys %Module::CoreList::version ],
           [], "FindExt shouldn't get included rt#6922" );


my $consistent = 1;
for my $family (values %Module::CoreList::families) {
    my $first = shift @$family;
    my $has = join " ", sort keys %{ $Module::CoreList::versions{ $first } };
    for my $member (@$family) {
        $has eq join " ", sort keys %{ $Module::CoreList::versions{ $member } }
          or do { diag "$first -> $member family"; $consistent = 0 };
    }
}
is( $consistent, 1,
    "families seem consistent (descendants have same modules as ancestors)" );
