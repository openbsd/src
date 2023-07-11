#! /bin/sh

set -u
set -e

# Library of functions.
# Intended to be sourced by scripts (or interactive shells if you want).

genfile_stdout_16m ()
{
    seq -f%015g 1048576
}
genfile_stdout_1m ()
{
    seq -f%015g 65536
}
genfile ()
{
    #touch "$1"
    genfile_stdout_1m > "$1"
}

# makes a directory path and optionally a file in it.
# if you want the last element to be a directory, add / at the end
mkdirfile ()
{
    case "$1" in
        '') error that cannot work;;
        */) mkdir -p "$1";;
        */*) mkdir -p "${1%/*}"; genfile "$1";;
        *) genfile "$1";;
    esac
}

mkdirsymlink ()
{
    (
        mkdir -p "$1"
        cd "$1"
        ln -sf "$2" "$3"
    )
}

# make a first interesting tree
generate_tree_1 ()
{
    mkdirfile foo/bar/baz/one.txt
    mkdirfile foo/bar/baz/one2.txt
    mkdirfile 'foo/bar/baz/  two.txt'
    mkdirfile 'foo/bar/baz/two  2.txt'
    mkdirfile 'foo/bar/baz/two3.txt  '
    mkdirsymlink foo/baz/ ../bar/baz/one.txt three.txt
    mkdirfile one/two/three/four.txt
    mkdirfile foo/five/one/two/five/blah.txt
    mkdirfile foo/one/two/five/blah.txt
}

# a frontend for find
# first argument is a dir to chdir to
findme ()
{
    if [ $# -lt 2 ] ; then
        echo usage: different 1>&2
        return 1
    fi
    (
        cd "$1" ; shift
        # Remove unstable fields:
        #    1: inode
        #    2: size in blocks
        # 8-10: last modification time
        find "$@" -ls |
        sed -e 's/^[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g' |
        cut -d ' ' -f 3-7,11- |
        sort
    )
}

# compare two trees.  This will later be modular to pick between:
# - diff
# - find . -print0 | sort --zero-terminated | xargs -0 tar fc foo.tar
# - mtree
compare_trees ()
{
    if [ $# -ne 2 ] ; then
        echo usage: different 1>&2
        return 1
    fi
    # files_and_permissions
    findme "$1" . > find1
    findme "$2" . > find2
    diff -u find[12]
    # file contents
    diff -ru "$1" "$2"
}
