BEGIN {
    unshift @INC, 't/lib';
}

use strict;
use Test::More tests => 31;

use Data::Dumper;
use File::Temp;
use Cwd;
use Parse::CPAN::Meta;

require ExtUtils::MM_Any;

sub in_dir(&;$) {
    my $code = shift;
    my $dir = shift || File::Temp->newdir;

    # chdir to the new directory
    my $orig_dir = cwd();
    chdir $dir or die "Can't chdir to $dir: $!";

    # Run the code, but trap the error so we can chdir back
    my $return;
    my $ok = eval { $return = $code->(); 1; };
    my $err = $@;

    # chdir back
    chdir $orig_dir or die "Can't chdir to $orig_dir: $!";

    # rethrow if necessary
    die $err unless $ok;

    return $return;
}

sub mymeta_ok {
    my($have, $want, $name) = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    my $have_gen = delete $have->{generated_by};
    my $want_gen = delete $want->{generated_by};
    my $have_url = delete $have->{'meta-spec'}->{url};
    my $want_url = delete $want->{'meta-spec'}->{url};

    is_deeply $have, $want, $name;
    like $have_gen, qr{CPAN::Meta}, "CPAN::Meta mentioned in the generated_by";
    like $have_url, qr{CPAN::Meta::Spec}, "CPAN::Meta::Spec mentioned in meta-spec URL";

    return;
}

my $new_mm = sub {
    return bless { ARGS => {@_}, @_ }, 'ExtUtils::MM_Any';
};

{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
    );

    is_deeply {$mm->metafile_data}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },
        build_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    };


    is_deeply {$mm->metafile_data({}, { no_index => { directory => [qw(foo)] } })}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },
        build_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },

        no_index        => {
            directory           => [qw(t inc foo)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    }, 'rt.cpan.org 39348';
}


{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        AUTHOR          => ['Some Guy'],
        PREREQ_PM       => {
            Foo                 => 2.34,
            Bar                 => 4.56,
        },
    );

    is_deeply {$mm->metafile_data(
        {
            configure_requires => {
                Stuff   => 2.34
            },
            wobble      => 42
        },
        {
            no_index    => {
                package => "Thing"
            },
            wibble      => 23
        },
    )},
    {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => ['Some Guy'],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'script',

        configure_requires      => {
            Stuff       => 2.34,
        },
        build_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },

        requires       => {
            Foo                 => 2.34,
            Bar                 => 4.56,
        },

        no_index        => {
            directory           => [qw(t inc)],
            package             => 'Thing',
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },

        wibble  => 23,
        wobble  => 42,
    };
}


# Test MIN_PERL_VERSION meta-spec 1.4
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
        MIN_PERL_VERSION => 5.006,
    );

    is_deeply {$mm->metafile_data}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },
        build_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },

        requires        => {
            perl        => '5.006',
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    }, 'MIN_PERL_VERSION meta-spec 1.4';
}

# Test MIN_PERL_VERSION meta-spec 2.0
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
        MIN_PERL_VERSION => 5.006,
    );

    is_deeply {
        $mm->metafile_data(
                {}, {
                'meta-spec' => {
                url     => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
                version => 2
                } } )
    }, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        prereqs => {
            configure       => {
                requires    => {
                    'ExtUtils::MakeMaker'   => 0,
                },
            },
            build           => {
                requires    => {
                    'ExtUtils::MakeMaker'   => 0,
                },
            },
            runtime         => {
                requires    => {
                    'perl'  => '5.006',
                },
            },
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {

            url     => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
            version => 2
        },
    }, 'MIN_PERL_VERSION meta-spec 2.0';
}

# Test MIN_PERL_VERSION meta-spec 1.4
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
        MIN_PERL_VERSION => 5.006,
        PREREQ_PM => {
            'Foo::Bar'  => 1.23,
        },
    );

    is_deeply {$mm->metafile_data}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },
        build_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },

        requires        => {
            perl        => '5.006',
            'Foo::Bar'  => 1.23,
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    }, 'MIN_PERL_VERSION and PREREQ_PM meta-spec 1.4';
}

# Test CONFIGURE_REQUIRES meta-spec 1.4
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        CONFIGURE_REQUIRES => {
            "Fake::Module1" => 1.01,
        },
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
    );

    is_deeply {$mm->metafile_data}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => {
            'Fake::Module1'     => 1.01,
        },
        build_requires      => {
            'ExtUtils::MakeMaker'   => 0,
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    },'CONFIGURE_REQUIRES meta-spec 1.4';
}

# Test CONFIGURE_REQUIRES meta-spec 2.0
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        CONFIGURE_REQUIRES => {
            "Fake::Module1" => 1.01,
        },
        PM              => {
            "Foo::Bar"      => 'lib/Foo/Bar.pm',
        },
    );

    is_deeply {
        $mm->metafile_data(
                {}, {
                'meta-spec' => {
                url     => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
                version => 2
                } } )
    }, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        prereqs => {
            configure       => {
                requires    => {
                    'Fake::Module1'         => 1.01,
                },
            },
            build           => {
                requires    => {
                    'ExtUtils::MakeMaker'   => 0,
                },
            },
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
            version     => 2
        },
    },'CONFIGURE_REQUIRES meta-spec 2.0';
}


# Test BUILD_REQUIRES meta-spec 1.4
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        BUILD_REQUIRES => {
            "Fake::Module1" => 1.01,
        },
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
    );

    is_deeply {$mm->metafile_data}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => {
            'ExtUtils::MakeMaker'   => 0,
        },
        build_requires      => {
            'Fake::Module1'         => 1.01,
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    },'BUILD_REQUIRES meta-spec 1.4';
}

# Test BUILD_REQUIRES meta-spec 2.0
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        BUILD_REQUIRES => {
            "Fake::Module1" => 1.01,
        },
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
        META_MERGE => { "meta-spec" => { version => 2 }},
    );

    is_deeply {
        $mm->metafile_data(
                {}, {
                'meta-spec' => {
                url     => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
                version => 2
                } } )
    }, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        prereqs => {
            configure       => {
                requires    => {
                    'ExtUtils::MakeMaker'   => 0,
                },
            },
            build           => {
                requires    => {
                    'Fake::Module1'         => 1.01,
                },
            },
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
            version     => 2
        },
    },'BUILD_REQUIRES meta-spec 2.0';
}

# Test TEST_REQUIRES meta-spec 1.4
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        TEST_REQUIRES => {
            "Fake::Module1"     => 1.01,
        },
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
    );

    is_deeply {$mm->metafile_data}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => {
            'ExtUtils::MakeMaker'       => 0,
        },
        build_requires      => {
            'ExtUtils::MakeMaker'       => 0,
            'Fake::Module1'             => 1.01,
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    },'TEST_REQUIRES meta-spec 1.4';
}

# Test TEST_REQUIRES meta-spec 2.0
{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        TEST_REQUIRES => {
            "Fake::Module1"     => 1.01,
        },
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
        META_MERGE => { "meta-spec" => { version => 2 }},
    );

    is_deeply {
        $mm->metafile_data(
                {}, {
                'meta-spec' => {
                url     => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
                version => 2
                } } )
    }, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        prereqs => {
            configure       => {
                requires    => {
                    'ExtUtils::MakeMaker'   => 0,
                },
            },
            build           => {
                requires    => {
                    'ExtUtils::MakeMaker'   => 0,
                },
            },
            test            => {
                requires    => {
                    "Fake::Module1"         => 1.01,
                },
            },
        },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
            version     => 2
        },
    },'TEST_REQUIRES meta-spec 2.0';
}


# Test _REQUIRES key priority over META_ADD

{
    my $mm = $new_mm->(
        DISTNAME        => 'Foo-Bar',
        VERSION         => 1.23,
        BUILD_REQUIRES => {
            "Fake::Module1" => 1.01,
        },
        META_ADD => (my $meta_add = { build_requires => {}, configure_requires => {} }),
        PM              => {
            "Foo::Bar"          => 'lib/Foo/Bar.pm',
        },
    );

    is_deeply {$mm->metafile_data($meta_add)}, {
        name            => 'Foo-Bar',
        version         => 1.23,
        abstract        => 'unknown',
        author          => [],
        license         => 'unknown',
        dynamic_config  => 1,
        distribution_type       => 'module',

        configure_requires      => { },
        build_requires          => { },

        no_index        => {
            directory           => [qw(t inc)],
        },

        generated_by => "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        'meta-spec'  => {
            url         => 'http://module-build.sourceforge.net/META-spec-v1.4.html',
            version     => 1.4
        },
    },'META.yml data (META_ADD wins)';


    # Yes, this is all hard coded.
    require CPAN::Meta;
    my $want_mymeta = {
        name            => 'ExtUtils-MakeMaker',
        version         => '6.57_07',
        abstract        => 'Create a module Makefile',
        author          => ['Michael G Schwern <schwern@pobox.com>'],
        license         => ['perl_5'],
        dynamic_config  => 0,

        prereqs => {
            runtime => {
                requires => {
                    "DirHandle"         => 0,
                    "File::Basename"    => 0,
                    "File::Spec"        => "0.8",
                    "Pod::Man"          => 0,
                    "perl"              => "5.006",
                },
            },
            configure => {
                requires => {
                    'ExtUtils::MakeMaker'   => 0,
                },
            },
            build    => {
                requires => {
                    'Fake::Module1'       => 1.01,
                },
            },
        },

        release_status => 'testing',
        resources => {
            license     =>  [ 'http://dev.perl.org/licenses/' ],
            homepage    =>  'http://makemaker.org',
            bugtracker  =>  { web => 'http://rt.cpan.org/NoAuth/Bugs.html?Dist=ExtUtils-MakeMaker' },
            repository  =>  { url => 'http://github.com/Perl-Toolchain-Gang/ExtUtils-MakeMaker' },
            x_MailingList => 'makemaker@perl.org',
        },

        no_index        => {
            directory           => [qw(t inc)],
            package             => ["DynaLoader", "in"],
        },

        generated_by => "ExtUtils::MakeMaker version 6.5707, CPAN::Meta::Converter version 2.110580",
        'meta-spec'  => {
            url         => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
            version     => 2,
        },
    };

    mymeta_ok $mm->mymeta("t/META_for_testing.json"),
              $want_mymeta,
              'MYMETA JSON data (BUILD_REQUIRES wins)';

    mymeta_ok $mm->mymeta("t/META_for_testing.yml"),
              $want_mymeta,
              'MYMETA YAML data (BUILD_REQUIRES wins)';
}

{
    my $mm = $new_mm->(
        DISTNAME            => 'Foo-Bar',
        VERSION             => 1.23,
        CONFIGURE_REQUIRES  => { "Fake::Module0" => 0.99 },
        BUILD_REQUIRES      => { "Fake::Module1" => 1.01 },
        TEST_REQUIRES       => { "Fake::Module2" => 1.23 },
    );

    my $meta = $mm->mymeta('t/META_for_testing.json');
    is($meta->{configure_requires}, undef, "no configure_requires in v2 META");
    is($meta->{build_requires}, undef, "no build_requires in v2 META");
    is_deeply(
        $meta->{prereqs}{configure}{requires},
        { "Fake::Module0" => 0.99 },
        "configure requires are one thing in META v2...",
    );

    is_deeply(
        $meta->{prereqs}{build}{requires},
        { "Fake::Module1" => 1.01 },
        "build requires are one thing in META v2...",
    );

    is_deeply(
        $meta->{prereqs}{test}{requires},
        { "Fake::Module2" => 1.23 },
        "...and test requires are another",
    );
}

note "CPAN::Meta bug using the module version instead of the meta spec version"; {
    my $mm = $new_mm->(
        NAME      => 'GD::Barcode::Code93',
        AUTHOR    => 'Chris DiMartino',
        ABSTRACT  => 'Code 93 implementation of GD::Barcode family',
        PREREQ_PM => {
            'GD::Barcode' => 0,
            'GD'          => 0
        },
        VERSION   => '1.4',
    );

    my $meta = $mm->mymeta("t/META_for_testing_tricky_version.yml");
    is $meta->{'meta-spec'}{version}, 2, "internally, our MYMETA struct is v2";

    in_dir {
        $mm->write_mymeta($meta);
        ok -e "MYMETA.yml";
        ok -e "MYMETA.json";

        my $meta_yml = Parse::CPAN::Meta->load_file("MYMETA.yml");
        is $meta_yml->{'meta-spec'}{version}, 1.4, "MYMETA.yml correctly downgraded to 1.4";

        my $meta_json = Parse::CPAN::Meta->load_file("MYMETA.json");
        cmp_ok $meta_json->{'meta-spec'}{version}, ">=", 2, "MYMETA.json at 2 or better";
    };
}


note "A bad license string"; {
    my $mm = $new_mm->(
        DISTNAME  => 'Foo::Bar',
        VERSION   => '1.4',
        LICENSE   => 'death and retribution',
    );

    in_dir {
        my $meta = $mm->mymeta;
        $mm->write_mymeta($meta);

        my $meta_yml = Parse::CPAN::Meta->load_file("MYMETA.yml");
        is $meta_yml->{license}, "unknown", "in yaml";

        my $meta_json = Parse::CPAN::Meta->load_file("MYMETA.json");
        is_deeply $meta_json->{license}, ["unknown"], "in json";
    };
}
