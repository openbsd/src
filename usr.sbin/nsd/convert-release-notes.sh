#!/usr/bin/env bash

set -e

SCRIPTNAME=$0
# SCRIPTPATH=$(dirname "$0")
VERSION=""

RELNOTES_FILE=doc/RELNOTES
PRINT_EMAIL=false
PRINT_YAML=false
RC=""

usage() {
    cat >&2 <<EOF
Usage: $SCRIPTNAME [options] [<version>]

Convert the RELNOTES file format to the different text formats used in
e-mail/GitHub/git-tags/website.

    <version>       Specify the version to extract from RELNOTES. Defaults to
                    the top-most version in the file.

    -e, --email     Generate an email header before the release notes.

    -r <num>, --rc <num>      Use this release candidate in generated text.

    -y, --yaml      Convert to yaml output for website nsd_releases.yml file.

    -f, --file      Specify the file to read. Defaults to "$RELNOTES_FILE".

Examples:

Latest release: $SCRIPTNAME
Specific release: $SCRIPTNAME 4.12.0
Release candidate: $SCRIPTNAME -r 1 4.12.0
EOF
}

error_unknown_option() {
    echo "Unknown option: $1" >&2
    usage && exit 1
}

error_too_many_args() {
    echo "Too many arguments found: $1" >&2
    usage && exit 1
}

extract_release_notes() {
    # Find latest version if VERSION was not specified
    if [[ -z "$VERSION" ]]; then
        VERSION=$(grep -E -m1 -B1 "^=====+$" <"$RELNOTES_FILE" | head -n1)
    fi

    sed -E -e "/^$VERSION/{ n; n; bfound }; d; bend" \
        -e ':found; /\n=====+$/bstrip_trailer; N; bfound' \
        -e ':strip_trailer; s/\n+([0-9]+\.[0-9]+\.[0-9]+)\n=====+$//' \
        -e ':delete_tabs; s/\t+//g' \
        -e ':end' "$RELNOTES_FILE"

    # grep -En -B1 '^=====+$' "$RELNOTES_FILE" | grep -A3 "$VERSION" | \
    #     grep -E "^[0-9]+-" | cut -d- -f1 | {
    #         read -r start
    #         read -r end
    #         head -n"$((end-1))" <"$RELNOTES_FILE" | tail -n+"$start" | tr -d "\t"
    #     }
}

convert_rel_notes_to_yaml() {
    local features=()
    local bugs=()
    local state=none
    local -n array_to_add # nameref ("pointer") to features or bugs array
    local tmp_item

    while read -r; do
        local line=$REPLY
        # echo ">>> $line"

        if grep -qE "^FEATURES:" <<<"$line"; then
            state=features
        elif grep -qE "^BUG FIXES:" <<<"$line"; then
            state=bugs
        elif grep -qE "^- " <<<"$line"; then
            state=item_start
        elif grep -qE "^  " <<<"$line"; then
            state=item_continue
        else
            # skip empty/unknown lines
            continue
        fi

        case "$state" in
            features)
                # Starting a new section, add last item to previous section
                if [[ -R array_to_add && -n "$tmp_item" ]]; then
                    array_to_add+=("$tmp_item")
                    tmp_item=""
                fi
                # shellcheck disable=2178
                declare -n array_to_add=features
                ;;
            bugs)
                # Starting a new section, add last item to previous section
                if [[ -R array_to_add && -n "$tmp_item" ]]; then
                    array_to_add+=("$tmp_item")
                    tmp_item=""
                fi
                # shellcheck disable=2178
                declare -n array_to_add=bugs
                ;;
            item_start)
                [[ -n "$tmp_item" ]] && array_to_add+=("$tmp_item")
                tmp_item=${line#- }
                ;;
            item_continue)
                tmp_item+=$'\n'"${line}"
                ;;
        esac
    done < <(tail -n+2 <<<"$(extract_release_notes)")
    # Finished reading, add completed item to active section
    [[ -n "$tmp_item" ]] && array_to_add+=("$tmp_item")

    cat <<EOF
---
version: $VERSION
date: $(date "+%d %B, %Y")
EOF

    if [[ "${#features[@]}" == 0 ]]; then
        echo "features: []"
    else
        echo "features:"
        for f in "${features[@]}"; do
            echo "- >-"
            echo "  $f"
        done
    fi

    if [[ "${#bugs[@]}" == 0 ]]; then
        echo "bugs: []"
    else
        echo "bugs:"
        for f in "${bugs[@]}"; do
            echo "- >-"
            echo "  $f"
        done
    fi
}

#### PARSE ARGUMENTS ####

if [[ "$1" =~ ^-h|--help$ ]]; then
    usage && exit
else
    until [[ -z "$1" ]]; do
        case "$1" in
            -e|--email)
                PRINT_EMAIL=true
                ;;
            -r|--rc)
                RC=rc$2
                shift
                ;;
            -y|--yaml)
                PRINT_YAML=true
                ;;
            -f|--file)
                RELNOTES_FILE=$2
                shift
                ;;
            -*)
                error_unknown_option "$1"
                ;;
            *)
                if [[ -z "$VERSION" ]]; then
                    VERSION=$1
                else
                    error_too_many_args "$1"
                fi
                ;;
        esac
        shift
    done
fi

if ! [[ -f "$RELNOTES_FILE" ]]; then
    echo "$0: $RELNOTES_FILE: No such file or directory" >&2
    exit 3
fi

#### Generate text ####

if [[ "$PRINT_EMAIL" == true ]]; then
    SHA256=""
    if [[ -f "nsd-$VERSION$RC.tar.gz.sha256" ]]; then
        SHA256=$(cat "nsd-$VERSION$RC.tar.gz.sha256")
    fi

cat <<EOF
Dear all,

NSD $VERSION ${RC:+pre-}release is available:
https://nlnetlabs.nl/downloads/nsd/nsd-$VERSION$RC.tar.gz
sha256 $SHA256
pgp https://nlnetlabs.nl/downloads/nsd/nsd-$VERSION$RC.tar.gz.asc

EOF
fi # END PRINT_EMAIL

if [[ "$PRINT_YAML" == true ]]; then
    convert_rel_notes_to_yaml
else
    extract_release_notes
fi

# vim: set ts=4 et sw=4:
