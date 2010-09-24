package CPANPLUS::Internals::Source::SQLite;

use strict;
use warnings;

use base 'CPANPLUS::Internals::Source';

use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;
use CPANPLUS::Internals::Source::SQLite::Tie;

use Data::Dumper;
use DBIx::Simple;
use DBD::SQLite;

use Params::Check               qw[allow check];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

use constant TXN_COMMIT => 1000;

=head1 NAME 

CPANPLUS::Internals::Source::SQLite - SQLite implementation

=cut

{   my $Dbh;
    my $DbFile;

    sub __sqlite_file { 
        return $DbFile if $DbFile;

        my $self = shift;
        my $conf = $self->configure_object;

        $DbFile = File::Spec->catdir( 
                        $conf->get_conf('base'),
                        SOURCE_SQLITE_DB
            );
    
        return $DbFile;
    };

    sub __sqlite_dbh { 
        return $Dbh if $Dbh;
        
        my $self = shift;
        $Dbh     = DBIx::Simple->connect(
                        "dbi:SQLite:dbname=" . $self->__sqlite_file,
                        '', '',
                        { AutoCommit => 1 }
                    );
        #$Dbh->dbh->trace(1);

        return $Dbh;        
    };
}

{   my $used_old_copy = 0;

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

        ### if it's not uptodate, or the file doesn't exist, we need to create
        ### a new sqlite db
        if( not $uptodate or not -e $self->__sqlite_file ) {        
            $used_old_copy = 0;

            ### chuck the file
            1 while unlink $self->__sqlite_file;
        
            ### and create a new one
            $self->__sqlite_create_db or do {
                error(loc("Could not create new SQLite DB"));
                return;    
            }            
        } else {
            $used_old_copy = 1;
        }            
    
        ### set up the author tree
        {   my %at;
            tie %at, 'CPANPLUS::Internals::Source::SQLite::Tie',
                dbh => $self->__sqlite_dbh, table => 'author', 
                key => 'cpanid',            cb => $self;
                
            $self->_atree( \%at  );
        }

        ### set up the author tree
        {   my %mt;
            tie %mt, 'CPANPLUS::Internals::Source::SQLite::Tie',
                dbh => $self->__sqlite_dbh, table => 'module', 
                key => 'module',            cb => $self;

            $self->_mtree( \%mt  );
        }
        
        ### start a transaction
        $self->__sqlite_dbh->query('BEGIN');
        
        return 1;        
        
    }
    
    sub _standard_trees_completed   { return $used_old_copy }
    sub _custom_trees_completed     { return }
    ### finish transaction
    sub _finalize_trees             { $_[0]->__sqlite_dbh->query('COMMIT'); return 1 }

    ### saves current memory state, but not implemented in sqlite
    sub _save_state                 { 
        error(loc("%1 has not implemented writing state to disk", __PACKAGE__)); 
        return;
    }
}

{   my $txn_count = 0;

    ### XXX move this outside the sub, so we only compute it once
    my $class;
    my @keys    = qw[ author cpanid email ];
    my $tmpl    = {
        class   => { default => 'CPANPLUS::Module::Author', store => \$class },
        map { $_ => { required => 1 } } @keys
     };
    
    ### dbix::simple's expansion of (??) is REALLY expensive, so do it manually
    my $ph      = join ',', map { '?' } @keys;


    sub _add_author_object {
        my $self = shift;
        my %hash = @_;
        my $dbh  = $self->__sqlite_dbh;
    
        my $href = do {
            local $Params::Check::NO_DUPLICATES         = 1;            
            local $Params::Check::SANITY_CHECK_TEMPLATE = 0;
            check( $tmpl, \%hash ) or return;
        };

        ### keep counting how many we inserted
        unless( ++$txn_count % TXN_COMMIT ) {
            #warn "Committing transaction $txn_count";
            $dbh->query('COMMIT') or error( $dbh->error ); # commit previous transaction
            $dbh->query('BEGIN')  or error( $dbh->error ); # and start a new one
        }
        
        $dbh->query( 
            "INSERT INTO author (". join(',',keys(%$href)) .") VALUES ($ph)",
            values %$href
        ) or do {
            error( $dbh->error );
            return;
        };
        
        return 1;
     }
}

{   my $txn_count = 0;

    ### XXX move this outside the sub, so we only compute it once
    my $class;    
    my @keys = qw[ module version path comment author package description dslip mtime ];
    my $tmpl = {
        class   => { default => 'CPANPLUS::Module', store => \$class },
        map { $_ => { required => 1 } } @keys
    };
    
    ### dbix::simple's expansion of (??) is REALLY expensive, so do it manually
    my $ph      = join ',', map { '?' } @keys;

    sub _add_module_object {
        my $self = shift;
        my %hash = @_;
        my $dbh  = $self->__sqlite_dbh;
    
        my $href = do {
            local $Params::Check::NO_DUPLICATES         = 1;
            local $Params::Check::SANITY_CHECK_TEMPLATE = 0;
            check( $tmpl, \%hash ) or return;
        };
        
        ### fix up author to be 'plain' string
        $href->{'author'} = $href->{'author'}->cpanid;

        ### keep counting how many we inserted
        unless( ++$txn_count % TXN_COMMIT ) {
            #warn "Committing transaction $txn_count";
            $dbh->query('COMMIT') or error( $dbh->error ); # commit previous transaction
            $dbh->query('BEGIN')  or error( $dbh->error ); # and start a new one
        }
        
        $dbh->query( 
            "INSERT INTO module (". join(',',keys(%$href)) .") VALUES ($ph)", 
            values %$href
        ) or do {
            error( $dbh->error );
            return;
        };
        
        return 1;
    }
}

{   my %map = (
        _source_search_module_tree  
            => [ module => module => 'CPANPLUS::Module' ],
        _source_search_author_tree  
            => [ author => cpanid => 'CPANPLUS::Module::Author' ],
    );        

    while( my($sub, $aref) = each %map ) {
        no strict 'refs';
        
        my($table, $key, $class) = @$aref;
        *$sub = sub {
            my $self = shift;
            my %hash = @_;
            my $dbh  = $self->__sqlite_dbh;
            
            my($list,$type);
            my $tmpl = {
                allow   => { required   => 1, default   => [ ], strict_type => 1,
                             store      => \$list },
                type    => { required   => 1, allow => [$class->accessors()],
                             store      => \$type },
            };
        
            check( $tmpl, \%hash ) or return;
        
        
            ### we aliased 'module' to 'name', so change that here too
            $type = 'module' if $type eq 'name';
        
            my $res = $dbh->query( "SELECT * from $table" );
            
            my $meth = $table .'_tree';
            my @rv = map  { $self->$meth( $_->{$key} ) } 
                     grep { allow( $_->{$type} => $list ) } $res->hashes;
        
            return @rv;
        }
    }
}



sub __sqlite_create_db {
    my $self = shift;
    my $dbh  = $self->__sqlite_dbh;
    
    ### we can ignore the result/error; not all sqlite implemantation
    ### support this    
    $dbh->query( qq[
        DROP TABLE IF EXISTS author;
        \n]
     ) or do {
        msg( $dbh->error );
    }; 
    $dbh->query( qq[
        DROP TABLE IF EXISTS module;
        \n]
     ) or do {
        msg( $dbh->error );
    }; 


    
    $dbh->query( qq[
        /* the author information */
        CREATE TABLE author (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            
            author  varchar(255),
            email   varchar(255),
            cpanid  varchar(255)
        );
        \n]

    ) or do {
        error( $dbh->error );
        return;
    };

    $dbh->query( qq[
        /* the module information */
        CREATE TABLE module (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            
            module      varchar(255),
            version     varchar(255),
            path        varchar(255),
            comment     varchar(255),
            author      varchar(255),
            package     varchar(255),
            description varchar(255),
            dslip       varchar(255),
            mtime       varchar(255)
        );
        
        \n]

    ) or do {
        error( $dbh->error );
        return;
    };        
        
    return 1;    
}

1;
