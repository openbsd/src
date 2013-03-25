package CPANPLUS::Internals::Source::SQLite::Tie;

use strict;
use warnings;

use CPANPLUS::Error;
use CPANPLUS::Module;
use CPANPLUS::Module::Fake;
use CPANPLUS::Module::Author::Fake;
use CPANPLUS::Internals::Constants;


use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';


use Data::Dumper;
$Data::Dumper::Indent = 1;

require Tie::Hash;
use vars qw[@ISA];
push @ISA, 'Tie::StdHash';


sub TIEHASH {
    my $class = shift;
    my %hash  = @_;

    my $tmpl = {
        dbh     => { required => 1 },
        table   => { required => 1 },
        key     => { required => 1 },
        cb      => { required => 1 },
        offset  => { default  => 0 },
    };

    my $args = check( $tmpl, \%hash ) or return;
    my $obj  = bless { %$args, store => {} } , $class;

    return $obj;
}

sub FETCH {
    my $self    = shift;
    my $key     = shift or return;
    my $dbh     = $self->{dbh};
    my $cb      = $self->{cb};
    my $table   = $self->{table};


    ### did we look this one up before?
    if( my $obj = $self->{store}->{$key} ) {
        return $obj;
    }

    my $res  = $dbh->query(
                    "SELECT * from $table where $self->{key} = ?", $key
                ) or do {
                    error( $dbh->error );
                    return;
                };

    my $href = $res->hash;

    ### get rid of the primary key
    delete $href->{'id'};

    ### no results?
    return unless keys %$href;

    ### expand author if needed
    ### XXX no longer generic :(
    if( $table eq 'module' ) {
        $href->{author} = $cb->author_tree( $href->{author } ) or return;
    }

    my $class = {
        module  => 'CPANPLUS::Module',
        author  => 'CPANPLUS::Module::Author',
    }->{ $table };

    my $obj = $self->{store}->{$key} = $class->new( %$href, _id => $cb->_id );

    return $obj;
}

sub STORE {
    my $self = shift;
    my $key  = shift;
    my $val  = shift;

    $self->{store}->{$key} = $val;
}

1;

sub FIRSTKEY {
    my $self = shift;
    my $dbh  = $self->{'dbh'};

    my $res  = $dbh->query(
                    "select $self->{key} from $self->{table} order by $self->{key} limit 1"
               );

    $self->{offset} = 0;

    my $key = $res->flat->[0];

    return $key;
}

sub NEXTKEY {
    my $self = shift;
    my $dbh  = $self->{'dbh'};

    my $res  = $dbh->query(
                    "select $self->{key} from $self->{table} ".
                    "order by $self->{key} limit 1 offset $self->{offset}"
               );

    $self->{offset} +=1;

    my $key = $res->flat->[0];
    my $val = $self->FETCH( $key );

    ### use each() semantics
    return wantarray ? ( $key, $val ) : $key;
}

sub EXISTS   { !!$_[0]->FETCH( $_[1] ) }

sub SCALAR   {
    my $self = shift;
    my $dbh  = $self->{'dbh'};

    my $res  = $dbh->query( "select count(*) from $self->{table}" );

    return $res->flat;
}

### intentionally left blank
sub DELETE   {  }
sub CLEAR    {  }

