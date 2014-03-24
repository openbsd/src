package B::Lint::Debug;
use if $] > 5.017, 'deprecate';

our $VERSION = '1.17';

=head1 NAME

B::Lint::Debug - Adds debugging stringification to B::

=head1 DESCRIPTION

This module injects stringification to a B::OP*/B::SPECIAL. This
should not be loaded unless you're debugging.

=cut

package # hide from PAUSE
    B::SPECIAL;
use overload '""' => sub {
    my $self = shift @_;
    "SPECIAL($$self)";
};

package # hide from PAUSE
    B::OP;
use overload '""' => sub {
    my $self  = shift @_;
    my $class = ref $self;
    $class =~ s/\AB:://xms;
    my $name = $self->name;
    "$class($name)";
};

package # hide from PAUSE
    B::SVOP;
use overload '""' => sub {
    my $self  = shift @_;
    my $class = ref $self;
    $class =~ s/\AB:://xms;
    my $name = $self->name;
    "$class($name," . $self->sv . "," . $self->gv . ")";
};

package # hide from PAUSE
    B::SPECIAL;
sub DESTROY { }
our $AUTOLOAD;

sub AUTOLOAD {
    my $cx = 0;
    print "AUTOLOAD $AUTOLOAD\n";

    package # hide from PAUSE
        DB;
    while ( my @stuff = caller $cx ) {

        print "$cx: [@DB::args] [@stuff]\n";
        if ( ref $DB::args[0] ) {
            if ( $DB::args[0]->can('padix') ) {
                print "    PADIX: " . $DB::args[0]->padix . "\n";
            }
            if ( $DB::args[0]->can('targ') ) {
                print "    TARG: " . $DB::args[0]->targ . "\n";
                for ( B::Lint::cv()->PADLIST->ARRAY ) {
                    print +( $_->ARRAY )[ $DB::args[0]->targ ] . "\n";
                }
            }
        }
        ++$cx;
    }
}

1;
