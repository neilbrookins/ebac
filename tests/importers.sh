#!/bin/sh
# tests/importers.sh -- the importer in C (bible --import) against the
# shell importer it replaces, byte for byte: each translation added by
# both, from the same copy of the source, into two EBaC folders, then the
# folders compared (every file, but the date in license and version) and
# what each said. See "The importers in C" in readme.architecture.
#
#   sh tests/importers.sh thiagobodruk SOURCE_FOLDER [NAME ...]
#
# SOURCE_FOLDER: a copy of thiagobodruk/bible (json/index.json and xml/).
# With no NAME, every translation in its list. Run from EBaC's folder,
# after make (bible and index).
set -u
which=$1
source=$(cd "$2" && pwd)
shift 2
repo=$(pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ebac-importers.XXXXXX")
trap 'rm -rf "$work"' EXIT

case $which in
thiagobodruk) script=add-thiagobodruk.sh ;;
*) echo "importers.sh: no such source: $which" >&2; exit 2 ;;
esac

# the shell importer's folder (the scripts, index, the AV) and the C one's
mkdir "$work/sh" "$work/c"
for d in "$work/sh" "$work/c"; do
        grep '^AV ' "$repo/trans" > "$d/trans"
        ln -s "$repo/AV" "$d/AV"
done
cp "$repo/$script" "$repo/importer.sh" "$repo/index" "$work/sh/"

if [ $# -eq 0 ]; then
        set -- $(sh "$work/sh/$script" --source "$source" --catalog 2>/dev/null |
                 awk -F '\t' '$1 == "translation" { print $2 }')
fi

pass=0
fail=0
for name in "$@"; do
        COLUMNS=80 sh "$work/sh/$script" --source "$source" --agree --at-end "$name" \
                > "$work/sh.out" 2>&1 < /dev/null
        COLUMNS=80 BIBLEROOT="$work/c" "$repo/bible" --import "$which" --source "$source" \
                --agree --at-end "$name" > "$work/c.out" 2>&1 < /dev/null
        problems=""
        if ! cmp -s "$work/sh.out" "$work/c.out"; then
                problems="$problems
     what they said differs:
$(diff "$work/sh.out" "$work/c.out" | head -n 8 | sed 's/^/       /')"
        fi
        for f in $( (cd "$work/sh" && find "$name" -type f 2>/dev/null; cd "$work/c" && find "$name" -type f 2>/dev/null) | sort -u); do
                case $f in */license|*/version)
                        a=$(sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]/DATE/' "$work/sh/$f" 2>/dev/null)
                        b=$(sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]/DATE/' "$work/c/$f" 2>/dev/null)
                        [ "$a" = "$b" ] || problems="$problems
     $f differs" ;;
                *)
                        cmp -s "$work/sh/$f" "$work/c/$f" || problems="$problems
     $f differs" ;;
                esac
        done
        cmp -s "$work/sh/trans" "$work/c/trans" || problems="$problems
     trans differs"
        if [ -z "$problems" ]; then
                pass=$((pass + 1))
                echo "ok   $name ($(sed -n 's/.*: writing \([0-9]*\) books.*/\1 books/p' "$work/c.out"))"
        else
                fail=$((fail + 1))
                echo "FAIL $name$problems"
        fi
done
echo
echo "$pass of $((pass + fail)) translations the same from both importers"
[ $fail -eq 0 ]
