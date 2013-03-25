package CPANPLUS::Internals::Source::Memory;

use base 'CPANPLUS::Internals::Source';

use strict;

use CPANPLUS::Error;
use CPANPLUS::Module;
use CPANPLUS::Module::Fake;
use CPANPLUS::Module::Author;
use CPANPLUS::Internals::Constants;

use File::Fetch;
use Archive::Extract;

use IPC::Cmd                    qw[can_run];
use File::Temp                  qw[tempdir];
use File::Basename              qw[dirname];
use Params::Check               qw[allow check];
use Module::Load::Conditional   qw[can_load];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

$Params::Check::VERBOSE = 1;

=head1 NAME

CPANPLUS::Internals::Source::Memory - In memory implementation

=cut

### flag to show if init_trees got its' data from storable. This allows
### us to not write an existing stored file back to disk
{   my $from_storable;

    sub _init_trees {
        my $self = shift;
        my $conf = $self->configure_object;
        my %hash = @_;

        my($path,$uptodate,$verbose,$use_stored);
        my $tmpl = {
            path        => { default => $conf->get_conf('base'), store => \$path },
            verbose     => { default => $conf->get_conf('verbose'), store => \$verbose },
            uptodate    => { required => 1, store => \$uptodate },
            use_stored  => { default  => 1, store => \$use_stored },
        };

        check( $tmpl, \%hash ) or return;

        ### retrieve the stored source files ###
        my $stored      = $self->__memory_retrieve_source(
                                path        => $path,
                                uptodate    => $uptodate && $use_stored,
                                verbose     => $verbose,
                            ) || {};

        ### we got this from storable if $stored has keys..
        $from_storable = keys %$stored ? 1 : 0;

        ### set up the trees
        $self->_atree( $stored->{_atree} || {} );
        $self->_mtree( $stored->{_mtree} || {} );

        return 1;
    }

    sub _standard_trees_completed { return $from_storable }
    sub _custom_trees_completed   { return $from_storable }

    sub _finalize_trees {
        my $self = shift;
        my $conf = $self->configure_object;
        my %hash = @_;

        my($path,$uptodate,$verbose);
        my $tmpl = {
            path        => { default => $conf->get_conf('base'), store => \$path },
            verbose     => { default => $conf->get_conf('verbose'), store => \$verbose },
            uptodate    => { required => 1, store => \$uptodate },
        };

        {   local $Params::Check::ALLOW_UNKNOWN = 1;
            check( $tmpl, \%hash ) or return;
        }

        ### write the stored files to disk, so we can keep using them
        ### from now on, till they become invalid
        ### write them if the original sources weren't uptodate, or
        ### we didn't just load storable files
        $self->__memory_save_source() if !$uptodate or not $from_storable;

        return 1;
    }

    ### saves current memory state
    sub _save_state {
        my $self = shift;
        return $self->_finalize_trees( @_, uptodate => 0 );
    }
}

sub _add_author_object {
    my $self = shift;
    my %hash = @_;

    my $class;
    my $tmpl = {
        class   => { default => 'CPANPLUS::Module::Author', store => \$class },
        map { $_ => { required => 1 } }
            qw[ author cpanid email ]
    };

    my $href = do {
        local $Params::Check::NO_DUPLICATES = 1;
        check( $tmpl, \%hash ) or return;
    };

    my $obj = $class->new( %$href, _id => $self->_id );

    $self->author_tree->{ $href->{'cpanid'} } = $obj or return;

    return $obj;
}

sub _add_module_object {
    my $self = shift;
    my %hash = @_;

    my $class;
    my $tmpl = {
        class   => { default => 'CPANPLUS::Module', store => \$class },
        map { $_ => { required => 1 } }
            qw[ module version path comment author package description dslip mtime ]
    };

    my $href = do {
        local $Params::Check::NO_DUPLICATES = 1;
        check( $tmpl, \%hash ) or return;
    };

    my $obj = $class->new( %$href, _id => $self->_id );

    ### Every module get's stored as a module object ###
    $self->module_tree->{ $href->{module} } = $obj or return;

    return $obj;
}

{   my %map = (
        _source_search_module_tree  => [ module_tree => 'CPANPLUS::Module' ],
        _source_search_author_tree  => [ author_tree => 'CPANPLUS::Module::Author' ],
    );

    while( my($sub, $aref) = each %map ) {
        no strict 'refs';

        my($meth, $class) = @$aref;

        *$sub = sub {
            my $self = shift;
            my $conf = $self->configure_object;
            my %hash = @_;

            my($authors,$list,$verbose,$type);
            my $tmpl = {
                data    => { default    => [],
                             strict_type=> 1, store     => \$authors },
                allow   => { required   => 1, default   => [ ], strict_type => 1,
                             store      => \$list },
                verbose => { default    => $conf->get_conf('verbose'),
                             store      => \$verbose },
                type    => { required   => 1, allow => [$class->accessors()],
                             store      => \$type },
            };

            my $args = check( $tmpl, \%hash ) or return;

            my @rv;
            for my $obj ( values %{ $self->$meth } ) {
                #push @rv, $auth if check(
                #                        { $type => { allow => $list } },
                #                        { $type => $auth->$type }
                #                    );
                push @rv, $obj if allow( $obj->$type() => $list );
            }

            return @rv;
        }
    }
}

=pod

=head2 $cb->__memory_retrieve_source(name => $name, [path => $path, uptodate => BOOL, verbose => BOOL])

This method retrieves a I<storable>d tree identified by C<$name>.

It takes the following arguments:

=over 4

=item name

The internal name for the source file to retrieve.

=item uptodate

A flag indicating whether the file-cache is up-to-date or not.

=item path

The absolute path to the directory holding the source files.

=item verbose

A boolean flag indicating whether or not to be verbose.

=back

Will get information from the config file by default.

Returns a tree on success, false on failure.

=cut

sub __memory_retrieve_source {
    my $self = shift;
    my %hash = @_;
    my $conf = $self->configure_object;

    my $tmpl = {
        path     => { default => $conf->get_conf('base') },
        verbose  => { default => $conf->get_conf('verbose') },
        uptodate => { default => 0 },
    };

    my $args = check( $tmpl, \%hash ) or return;

    ### check if we can retrieve a frozen data structure with storable ###
    my $storable = can_load( modules => {'Storable' => '0.0'} )
                        if $conf->get_conf('storable');

    return unless $storable;

    ### $stored is the name of the frozen data structure ###
    my $stored = $self->__memory_storable_file( $args->{path} );

    if ($storable && -e $stored && -s _ && $args->{'uptodate'}) {
        msg( loc("Retrieving %1", $stored), $args->{'verbose'} );

        my $href = Storable::retrieve($stored);
        return $href;
    } else {
        return;
    }
}

=pod

=head2 $cb->__memory_save_source([verbose => BOOL, path => $path])

This method saves all the parsed trees in I<storable>d format if
C<Storable> is available.

It takes the following arguments:

=over 4

=item path

The absolute path to the directory holding the source files.

=item verbose

A boolean flag indicating whether or not to be verbose.

=back

Will get information from the config file by default.

Returns true on success, false on failure.

=cut

sub __memory_save_source {
    my $self = shift;
    my %hash = @_;
    my $conf = $self->configure_object;


    my $tmpl = {
        path     => { default => $conf->get_conf('base'), allow => DIR_EXISTS },
        verbose  => { default => $conf->get_conf('verbose') },
        force    => { default => 1 },
    };

    my $args = check( $tmpl, \%hash ) or return;

    my $aref = [qw[_mtree _atree]];

    ### check if we can retrieve a frozen data structure with storable ###
    my $storable;
    $storable = can_load( modules => {'Storable' => '0.0'} )
                    if $conf->get_conf('storable');
    return unless $storable;

    my $to_write = {};
    foreach my $key ( @$aref ) {
        next unless ref( $self->$key );
        $to_write->{$key} = $self->$key;
    }

    return unless keys %$to_write;

    ### $stored is the name of the frozen data structure ###
    my $stored = $self->__memory_storable_file( $args->{path} );

    if (-e $stored && not -w $stored) {
        msg( loc("%1 not writable; skipped.", $stored), $args->{'verbose'} );
        return;
    }

    msg( loc("Writing compiled source information to disk. This might take a little while."),
	    $args->{'verbose'} );

    my $flag;
    unless( Storable::nstore( $to_write, $stored ) ) {
        error( loc("could not store %1!", $stored) );
        $flag++;
    }

    return $flag ? 0 : 1;
}

sub __memory_storable_file {
    my $self = shift;
    my $conf = $self->configure_object;
    my $path = shift or return;

    ### check if we can retrieve a frozen data structure with storable ###
    my $storable = $conf->get_conf('storable')
                        ? can_load( modules => {'Storable' => '0.0'} )
                        : 0;

    return unless $storable;

    ### $stored is the name of the frozen data structure ###
    ### changed to use File::Spec->catfile -jmb
    my $stored = File::Spec->rel2abs(
        File::Spec->catfile(
            $path,                          #base dir
            $conf->_get_source('stored')    #file
            . '.s' .
            $Storable::VERSION              #the version of storable
            . '.c' .
            $self->VERSION                  #the version of CPANPLUS
            . STORABLE_EXT                  #append a suffix
        )
    );

    return $stored;
}




# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

1;
