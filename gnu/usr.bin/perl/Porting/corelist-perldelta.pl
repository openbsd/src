#!perl
use 5.010;
use strict;
use warnings;
use lib 'Porting';
use Maintainers qw/%Modules/;
use Module::CoreList;
use Getopt::Long;

=head1 USAGE

  # generate the module changes for the Perl you are currently building
  ./perl Porting/corelist-perldelta.pl
  
  # generate a diff between the corelist sections of two perldelta* files:
  perl Porting/corelist-perldelta.pl --mode=check 5.17.1 5.17.2 <perl5172delta.pod

=head1 ABOUT

corelist-perldelta.pl is a bit schizophrenic. The part to generate the
new Perldelta text does not need Algorithm::Diff, but wants to be
run with the freshly built Perl.

The part to check the diff wants to be run with a Perl that has an up-to-date
L<Module::CoreList>, but needs the outside L<Algorithm::Diff>.

Ideally, the program will be split into two separate programs, one
to generate the text and one to show the diff between the 
corelist sections of the last perldelta and the next perldelta.

=cut

my %sections = (
  new     => qr/New Modules and Pragma(ta)?/,
  updated => qr/Updated Modules and Pragma(ta)?/,
  removed => qr/Removed Modules and Pragma(ta)?/,
);

my %titles = (
  new     => 'New Modules and Pragmata',
  updated => 'Updated Modules and Pragmata',
  removed => 'Removed Modules and Pragmata',
);

my $deprecated;

#--------------------------------------------------------------------------#

sub added {
  my ($mod, $old_v, $new_v) = @_;
  say "=item *\n";
  say "C<$mod> $new_v has been added to the Perl core.\n";
}

sub updated {
  my ($mod, $old_v, $new_v) = @_;
  say "=item *\n";
  say "C<$mod> has been upgraded from version $old_v to $new_v.\n";
  if ( $deprecated->{$mod} ) {
    say "NOTE: C<$mod> is deprecated and may be removed from a future version of Perl.\n";
  }
}

sub removed {
  my ($mod, $old_v, $new_v) = @_;
  say "=item *\n";
  say "C<$mod> has been removed from the Perl core.  Prior version was $old_v.\n";
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

sub run {
  my %opt = (mode => 'generate');

  GetOptions(\%opt,
    'mode|m:s', # 'generate', 'check'
  );

  # by default, compare latest two version in CoreList;
  my @versions = sort keys %Module::CoreList::version;
  my ($old, $new) = (shift @ARGV, shift @ARGV);
  $old ||= $versions[-2];
  $new ||= $versions[-1];

  if ( $opt{mode} eq 'generate' ) {
    do_generate($old => $new);
  }
  elsif ( $opt{mode} eq 'check' ) {
    do_check(\*ARGV, $old => $new);
  }
  else {
    die "Unrecognized mode '$opt{mode}'\n";
  }

  exit 0;
}

sub corelist_delta {
  my ($old, $new) = @_;
  my $corelist = \%Module::CoreList::version;

  $deprecated = $Module::CoreList::deprecated{$new};

  my (@new,@deprecated,@removed,@pragmas,@modules);

  # %Modules defines what is currently in core
  for my $k ( keys %Modules ) {
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

  return (\@new, \@removed, \@pragmas, \@modules);
}

sub do_generate {
  my ($old, $new) = @_;
  my ($added, $removed, $pragmas, $modules) = corelist_delta($old => $new);

  generate_section($titles{new}, \&added, @{ $added });
  generate_section($titles{updated}, \&updated, @{ $pragmas }, @{ $modules });
  generate_section($titles{removed}, \&removed, @{ $removed });
}

sub do_check {
  my ($in, $old, $new) = @_;

  my $delta = DeltaParser->new($in);
  my ($added, $removed, $pragmas, $modules) = corelist_delta($old => $new);

  for my $ck (['new',     $delta->new_modules, $added],
              ['removed', $delta->removed_modules, $removed],
              ['updated', $delta->updated_modules, [@{ $modules }, @{ $pragmas }]]) {
    my @delta = @{ $ck->[1] };
    my @corelist = sort { lc $a->[0] cmp lc $b->[0] } @{ $ck->[2] };

    printf $ck->[0] . ":\n";

    require Algorithm::Diff;
    my $diff = Algorithm::Diff->new(map {
      [map { join q{ } => grep defined, @{ $_ } } @{ $_ }]
    } \@delta, \@corelist);

    while ($diff->Next) {
      next if $diff->Same;
      my $sep = '';
      if (!$diff->Items(2)) {
        printf "%d,%dd%d\n", $diff->Get(qw( Min1 Max1 Max2 ));
      } elsif(!$diff->Items(1)) {
        printf "%da%d,%d\n", $diff->Get(qw( Max1 Min2 Max2 ));
      } else {
        $sep = "---\n";
        printf "%d,%dc%d,%d\n", $diff->Get(qw( Min1 Max1 Min2 Max2 ));
      }
      print "< $_\n" for $diff->Items(1);
      print $sep;
      print "> $_\n" for $diff->Items(2);
    }

    print "\n";
  }
}

{
  package DeltaParser;
  use Pod::Simple::SimpleTree;

  sub new {
    my ($class, $input) = @_;

    my $self = bless {} => $class;

    my $parsed_pod = Pod::Simple::SimpleTree->new->parse_file($input)->root;
    splice @{ $parsed_pod }, 0, 2; # we don't care about the document structure,
                                   # just the nodes within it

    $self->_parse_delta($parsed_pod);

    return $self;
  }

  for my $k (keys %sections) {
    no strict 'refs';
    my $m = "${k}_modules";
    *$m = sub { $_[0]->{$m} };
  }

  sub _parse_delta {
    my ($self, $pod) = @_;

    map {
        my ($t, $s) = @{ $_ };
        
        # Keep the section title if it has one:
        if( $s->[0]->[0] eq 'head2' ) {
          #warn "Keeping section title '$s->[0]->[2]'";
          $titles{ $t } = $s->[0]->[2]
              if $s->[0]->[2];
        };

        $self->${\"_parse_${t}_section"}($s)
    } map {
        my $s = $self->_look_for_section($pod => $sections{$_})
            or die "failed to parse $_ section";
        [$_, $s];
    } keys %sections;

    for my $s (keys %sections) {
      my $m = "${s}_modules";

      $self->{$m} = [sort {
        lc $a->[0] cmp lc $b->[0]
      } @{ $self->{$m} }];
    }

    return;
  }

  sub _parse_new_section {
    my ($self, $section) = @_;

    $self->{new_modules} = $self->_parse_section($section => sub {
      my ($el) = @_;

      my ($first, $second) = @{ $el }[2, 3];
      my ($ver) = $second =~ /(\d[^\s]+)\s+has\s+been/;

      return [ $first->[2], undef, $ver ];
    });

    return;
  }

  sub _parse_updated_section {
    my ($self, $section) = @_;

    $self->{updated_modules} = $self->_parse_section($section => sub {
      my ($el) = @_;

      my ($first, $second) = @{ $el }[2, 3];
      my $module = $first->[2];
      my ($old, $new) = $second =~
          /from\s+(?:version\s+)?(\d[^\s]+)\s+to\s+(\d[^\s]+?)\.?$/;

      warn "Unable to extract old or new version of $module from perldelta"
        if !defined $old || !defined $new;

      return [ $module, $old, $new ];
    });

    return;
  }

  sub _parse_removed_section {
    my ($self, $section) = @_;
    $self->{removed_modules} = $self->_parse_section($section => sub {
      my ($el) = @_;

      my ($first, $second) = @{ $el }[2, 3];
      my ($old) = $second =~ /was\s+(\d[^\s]+?)\.?$/;

      return [ $first->[2], $old, undef ];
    });

    return;
  }

  sub _parse_section {
    my ($self, $section, $parser) = @_;

    my $items = $self->_look_down($section => sub {
      my ($el) = @_;
      return unless ref $el && $el->[0] =~ /^item-/
          && @{ $el } > 2 && ref $el->[2];
      return unless $el->[2]->[0] eq 'C';

      return 1;
    });

    return [map { $parser->($_) } @{ $items }];
  }

  sub _look_down {
    my ($self, $pod, $predicate) = @_;
    my @pod = @{ $pod };

    my @l;
    while (my $el = shift @pod) {
      push @l, $el if $predicate->($el);
      if (ref $el) {
        my @el = @{ $el };
        splice @el, 0, 2;
        unshift @pod, @el if @el;
      }
    }

    return @l ? \@l : undef;
  }

  sub _look_for_section {
    my ($self, $pod, $section) = @_;

    my $level;
    $self->_look_for_range($pod,
      sub {
        my ($el) = @_;
        my ($heading) = $el->[0] =~ /^head(\d)$/;
        my $f = $heading && $el->[2] =~ /^$section/;        
        $level = $heading if $f && !$level;
        return $f;
      },
      sub {
        my ($el) = @_;
        $el->[0] =~ /^head(\d)$/ && $1 <= $level;
      },
    );
  }

  sub _look_for_range {
    my ($self, $pod, $start_predicate, $stop_predicate) = @_;

    my @l;
    for my $el (@{ $pod }) {
      if (@l) {
        return \@l if $stop_predicate->($el);
      }
      else {
        next unless $start_predicate->($el);
      }
      push @l, $el;
    }

    return;
  }
}

run;
