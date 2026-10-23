#!/bin/sh
# translations.sh -- read the 'trans' file, for the makefile.
#
#   sh translations.sh list            print the translation folders in trans
#   sh translations.sh check           check trans against the folders here
#   sh translations.sh check --strict  the same, but unlisted folders are
#                                      errors too (used by "make test")
#
# Each line of trans is a translation's folder name, then a description:
#     AV  Authorized Version, (King James Version)
# Blank lines are skipped, and so are lines starting with '#'. A comment
# whose first word is a folder name notes a folder that is not a
# translation, so that every folder is accounted for:
#     # tests  side-by-side tests (not a translation)
#
# "check" reports, and exits with status 1:
#   - a translation listed in trans whose folder or booklist is missing
#     (always an error: it can't be indexed), and
#   - a folder that trans doesn't mention at all (a warning, or an error
#     with --strict), so a new translation isn't forgotten.
# Folders whose names start with '.' (such as .git) are ignored.

cd "$(dirname "$0")" || exit 1

listed() {      # translation folders: first word of each non-comment line
    awk '!/^[ \t]*#/ && NF { print $1 }' trans
}

noted() {       # folders named in comments: first word after the '#'
    awk '/^[ \t]*#/ { sub(/^[ \t]*#[ \t]*/, ""); if (NF) print $1 }' trans
}

case "$1" in
list)
    listed
    exit 0
    ;;
check)
    ;;
*)
    echo "usage: sh translations.sh list | check [--strict]" >&2
    exit 2
    ;;
esac

status=0

for t in $(listed); do
    if [ ! -d "$t" ]; then
        echo "Error: 'trans' lists translation $t, but there is no folder $t." >&2
        status=1
    elif [ ! -f "$t/booklist" ]; then
        echo "Error: translation $t has no booklist (the list of its books)." >&2
        status=1
    fi
done

if [ "$2" = "--strict" ]; then
    level=Error
else
    level=Warning
fi
for d in */; do
    [ -d "$d" ] || continue         # no folders at all
    d=${d%/}
    if ! { listed; noted; } | grep -qx -- "$d"; then
        echo "$level: folder $d is not mentioned in 'trans'. If it is a translation," >&2
        echo "  add a line for it; if not, add a comment line:  # $d  (what it is)" >&2
        [ "$level" = Error ] && status=1
    fi
done

exit $status
