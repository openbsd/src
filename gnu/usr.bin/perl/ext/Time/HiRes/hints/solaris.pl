use POSIX qw(uname);
# 2.6 has nanosleep in -lposix4, after that it's in -lrt
if (substr((uname())[2], 2) <= 6) {
    $self->{LIBS} = ['-lposix4'];
} else {
    $self->{LIBS} = ['-lrt'];
}


