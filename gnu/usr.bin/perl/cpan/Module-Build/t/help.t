#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 23;

blib_load('Module::Build');

use DistGen;

my $dist = DistGen->new;
$dist->regen;
$dist->chdir_in;

my $restart = sub {
  # we're redefining the same package as we go, so...
  delete($::{'MyModuleBuilder::'});
  delete($INC{'MyModuleBuilder.pm'});
  $dist->regen( clean => 1 );
};

########################################################################
{ # check the =item style
my $mb = Module::Build->subclass(
  code => join "\n", map {s/^ {4}//; $_} split /\n/, <<'  ---',
    =head1 ACTIONS

    =over

    =item foo

    Does the foo thing.

    =item bar

    Does the bar thing.

    =item help

    Does the help thing.

    You should probably not be seeing this.  That is, we haven't
    overridden the help action, but we're able to override just the
    docs?  That almost seems reasonable, but is probably wrong.

    =back

    =cut

    sub ACTION_foo { die "fooey" }
    sub ACTION_bar { die "barey" }
    sub ACTION_baz { die "bazey" }

    # guess we can have extra pod later

    =over

    =item baz

    Does the baz thing.

    =back

    =cut

  ---
  )->new(
      module_name => $dist->name,
  );

ok $mb;
can_ok($mb, 'ACTION_foo');

foreach my $action (qw(foo bar baz)) { # typical usage
  my $doc = $mb->get_action_docs($action);
  ok($doc, "got doc for '$action'");
  like($doc, qr/^=\w+ $action\n\nDoes the $action thing\./s,
    'got the right doc');
}

{ # user typo'd the action name
  ok( ! eval {$mb->get_action_docs('batz'); 1}, 'slap');
  like($@, qr/No known action 'batz'/, 'informative error');
}

{ # XXX this one needs some thought
  my $action = 'help';
  my $doc = $mb->get_action_docs($action);
  ok($doc, "got doc for '$action'");
  0 and warn "help doc >\n$doc<\n";
  TODO: {
    local $TODO = 'Do we allow overrides on just docs?';
    unlike($doc, qr/^=\w+ $action\n\nDoes the $action thing\./s,
      'got the right doc');
  }
}
} # end =item style
$restart->();
########################################################################
if(0) { # the =item style without spanning =head1 sections
my $mb = Module::Build->subclass(
  code => join "\n", map {s/^ {4}//; $_} split /\n/, <<'  ---',
    =head1 ACTIONS

    =over

    =item foo

    Does the foo thing.

    =item bar

    Does the bar thing.

    =back

    =head1 thbbt

    =over

    =item baz

    Should not see this.

    =back

    =cut

    sub ACTION_foo { die "fooey" }
    sub ACTION_bar { die "barey" }
    sub ACTION_baz { die "bazey" }

  ---
  )->new(
      module_name => $dist->name,
  );

ok $mb;
can_ok($mb, 'ACTION_foo');

foreach my $action (qw(foo bar)) { # typical usage
  my $doc = $mb->get_action_docs($action);
  ok($doc, "got doc for '$action'");
  like($doc, qr/^=\w+ $action\n\nDoes the $action thing\./s,
    'got the right doc');
}
is($mb->get_action_docs('baz'), undef, 'no jumping =head1 sections');

} # end =item style without spanning =head1's
$restart->();
########################################################################
TODO: { # the =item style with 'Actions' not 'ACTIONS'
local $TODO = 'Support capitalized Actions section';
my $mb = Module::Build->subclass(
  code => join "\n", map {s/^ {4}//; $_} split /\n/, <<'  ---',
    =head1 Actions

    =over

    =item foo

    Does the foo thing.

    =item bar

    Does the bar thing.

    =back

    =cut

    sub ACTION_foo { die "fooey" }
    sub ACTION_bar { die "barey" }

  ---
  )->new(
      module_name => $dist->name,
  );

foreach my $action (qw(foo bar)) { # typical usage
  my $doc = $mb->get_action_docs($action);
  ok($doc, "got doc for '$action'");
  like($doc || 'undef', qr/^=\w+ $action\n\nDoes the $action thing\./s,
    'got the right doc');
}

} # end =item style with Actions
$restart->();
########################################################################
{ # check the =head2 style
my $mb = Module::Build->subclass(
  code => join "\n", map {s/^ {4}//; $_} split /\n/, <<'  ---',
    =head1 ACTIONS

    =head2 foo

    Does the foo thing.

    =head2 bar

    Does the bar thing.

    =head3 bears

    Be careful with bears.

    =cut

    sub ACTION_foo { die "fooey" }
    sub ACTION_bar { die "barey" }
    sub ACTION_baz { die "bazey" }
    sub ACTION_batz { die "batzey" }

    # guess we can have extra pod later
    # Though, I do wonder whether we should allow them to mix...
    # maybe everything should have to be head2?

    =head2 baz

    Does the baz thing.

    =head4 What's a baz?

    =head1 not this part

    This is level 1, so the stuff about baz is done.

    =head1 Thing

    =head2 batz

    This is not an action doc.

    =cut

  ---
  )->new(
      module_name => $dist->name,
  );

my %also = (
  foo => '',
  bar => "\n=head3 bears\n\nBe careful with bears.\n",
  baz => "\n=head4 What's a baz\\?\n",
);

foreach my $action (qw(foo bar baz)) {
  my $doc = $mb->get_action_docs($action);
  ok($doc, "got doc for '$action'");
  my $and = $also{$action};
  like($doc || 'undef',
    qr/^=\w+ $action\n\nDoes the $action thing\.\n$and\n$/s,
    'got the right doc');
}
is($mb->get_action_docs('batz'), undef, 'nothing after uplevel');

} # end =head2 style
########################################################################

# cleanup
$dist->clean();

# vim:ts=2:sw=2:et:sta
