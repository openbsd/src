BEGIN {   # this is a variant of arnolds original example
        ARGV[1] = "/dev/null"
        ARGV[2] = "glotch"  # file open must skipped deleted argv
        ARGV[3] = "/dev/null"
        ARGC = 4
        delete ARGV[2]
}
# note that input is read here
END {
        for (i in ARGV)
                printf("ARGV[%d] is %s\n", i, ARGV[i])
}
