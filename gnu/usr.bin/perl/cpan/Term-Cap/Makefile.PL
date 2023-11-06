use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'		=> 'Term::Cap',
    'VERSION_FROM'	=> 'Cap.pm',
    'INSTALLDIRS'       => 'perl',
    'PREREQ_PM'		=> {Test::More => 0.33 },
    'LICENSE'      => 'perl',
    META_MERGE => {
        'meta-spec' => { version => 2 },
        resources => {
            repository => {
                type => 'git',
                url  => 'https://github.com/jonathanstowe/Term-Cap.git',
                web  => 'https://github.com/jonathanstowe/Term-Cap',
            },
        },
    },
    ($] >= 5.005 ?   
      (ABSTRACT_FROM => 'Cap.pm',
       AUTHOR     => 'Jonathan Stowe <jns@gellyfish.co.uk>') : ()),
);
