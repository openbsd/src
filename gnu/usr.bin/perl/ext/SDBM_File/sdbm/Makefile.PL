use ExtUtils::MakeMaker;
WriteMakefile(
    'NAME'	=> 'SDBM_File',
    'LINKTYPE'  => 'static',
    'DEFINE'	=> '-DSDBM -DDUFF',
    'SKIP'	=> [qw(static static_lib dynamic dynamic_lib)],
    'clean'
	=> {'FILES' => 'dbu libsdbm.a dbd dba dbe x-dbu *.dir *.pag'},
    'H' => [qw(tune.h sdbm.h pair.h $(PERL_INC)/config.h)],
    'C' => [qw(sdbm.c pair.c hash.c)]
);


sub MY::top_targets {
	'
all :: static

static ::	libsdbm$(LIB_EXT)

config ::

libsdbm$(LIB_EXT): $(O_FILES)
	$(AR) cr libsdbm$(LIB_EXT) $(O_FILES)
	$(RANLIB) libsdbm$(LIB_EXT)

lint:
	lint -abchx $(LIBSRCS)
';
}
