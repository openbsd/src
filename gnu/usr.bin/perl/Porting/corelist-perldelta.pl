#!perl
use 5.010;
use strict;
use warnings;
use lib 'Porting';
use Maintainers qw/%Modules/;
use Module::CoreList;

my $deprecated;

#--------------------------------------------------------------------------#

sub added {
  my ($mod, $old_v, $new_v) = @_;
  say "=item C<$mod>\n";
  say "Version $new_v has been added to the Perl core.\n";
}

sub updated {
  my ($mod, $old_v, $new_v) = @_;
  say "=item C<$mod>\n";
  say "Upgraded from version $old_v to $new_v.\n";
  if ( $deprecated->{$mod} ) {
    say "NOTE: C<$mod> is deprecated and may be removed from a future version of Perl.\n";
  }
}

sub removed {
  my ($mod, $old_v, $new_v) = @_;
  say "=item C<$mod>\n";
  say "Removed from the Perl core.  Prior version was $old_v.\n";
}

sub generate_section {
  my ($title, $item_sub, @mods ) = @_;
  return unless @mods;

  say "=head2 $title\n";
  say "=over 4\n";

  for my $tuple ( sort { lc($a->[0]) cmp lc($b->[0]) } @mods ) {
    my ($mod,$old_v,$new_v) = @$tuple;
    $old_v //= q('undef');
    $new_v //= q('undef');
    $item_sub->($mod, $old_v, $new_v);
  }

  say "=back\n";
}

#--------------------------------------------------------------------------#

my $corelist = \%Module::CoreList::version;
my @versions = sort keys %$corelist;

# by default, compare latest two version in CoreList;
my ($old, $new) = @ARGV;
$old ||= $versions[-2];
$new ||= $versions[-1];
$deprecated = $Module::CoreList::deprecated{$new};

my (@new,@deprecated,@removed,@pragmas,@modules);

# %Modules defines what is currently in core
for my $k ( keys %Modules ) {
    warn "Considering $k";
  next unless exists $corelist->{$new}{$k};
  my $old_ver = $corelist->{$old}{$k};
  my $new_ver = $corelist->{$new}{$k};
  # in core but not in last corelist
  if ( ! exists $corelist->{$old}{$k} ) {
    push @new, [$k, undef, $new_ver];
  }
  # otherwise just pragmas or modules
  else {
    my $old_ver = $corelist->{$old}{$k};
    my $new_ver = $corelist->{$new}{$k};
    next unless defined $old_ver && defined $new_ver && $old_ver ne $new_ver;
    my $tuple = [ $k, $old_ver, $new_ver ];
    if ( $k eq lc $k ) {
      push @pragmas, $tuple;
    }
    else {
      push @modules, $tuple;
    }
  }
}

# in old corelist, but not this one => removed
# N.B. This is exhaustive -- not just what's in %Modules, so modules removed from
# distributions will show up here, too.  Some person will have to review to see what's
# important. That's the best we can do without a historical Maintainers.pl
for my $k ( keys %{ $corelist->{$old} } ) {
  if ( ! exists $corelist->{$new}{$k} ) {
    push @removed, [$k, $corelist->{$old}{$k}, undef];
  }
}

generate_section("New Modules and Pragmata", \&added, @new);
generate_section("Pragmata Changes", \&updated, @pragmas);
generate_section("Updated Modules", \&updated, @modules);
generate_section("Removed Modules and Pragmata", \&removed, @removed);

