#!/bin/sh
# tests/importers.sh -- the importer in C (bible --import) against the
# shell importer it replaced, byte for byte: each translation added by
# both, from the same copy of the source, into two EBaC folders, then the
# folders compared (every file, but the date in license and version) and
# what each said. See "The importers in C" in readme.architecture.
#
#   sh tests/importers.sh thiagobodruk SOURCE_FOLDER [NAME ...]
#   sh tests/importers.sh scrollmapper SOURCE_FOLDER [NAME ...]
#   sh tests/importers.sh gutenberg TEXTS_FOLDER [NUMBER ...]
#
# SOURCE_FOLDER: a copy of thiagobodruk/bible (json/index.json and xml/),
# or of scrollmapper/bible_databases (docs/main_readme/translation_list.md,
# sources/*/*/README.md and formats/txt/), or of Project Gutenberg's texts
# (NUMBER.txt, and its page, NUMBER.html, if there is one: each added
# with its page, then without it). With no NAME, every translation in its
# list. EBAC_NAMES_SOURCE: a copy of Unicode's names of languages
# (cldr-localenames-full/main), for that list, if not from the Internet.
# Run from EBaC's folder, after make (bible and index). The shell
# importers, retired since, are taken from git, as they were last
# (IMPORTERS_FROM: another commit to take them from).
set -u
# (the last commit with the shell importers)
from=${IMPORTERS_FROM:-72ace7e3033f7f25bdd28ae3b97ec1fbcf015e8c}
which=$1
source=$(cd "$2" && pwd)
shift 2
repo=$(pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ebac-importers.XXXXXX")
trap 'rm -rf "$work"' EXIT

case $which in
thiagobodruk) script=add-thiagobodruk.sh ;;
scrollmapper) script=add-scrollmapper.sh ;;
gutenberg) script=add-gutenberg.sh ;;
*) echo "importers.sh: no such source: $which" >&2; exit 2 ;;
esac

# the shell importer's folder (the scripts, index, the AV) and the C one's
mkdir "$work/sh" "$work/c"
for d in "$work/sh" "$work/c"; do
        grep '^AV ' "$repo/trans" > "$d/trans"
        ln -s "$repo/AV" "$d/AV"
done
for f in "$script" importer.sh; do
        git -C "$repo" show "$from:$f" > "$work/sh/$f" ||
                { echo "importers.sh: $f isn't in commit $from" >&2; exit 2; }
done
cp "$repo/index" "$work/sh/"

# (each folder compared: every file, but the date in license and version)
compare_folder() {
        for f in $( (cd "$work/sh" && find "$1" -type f 2>/dev/null; cd "$work/c" && find "$1" -type f 2>/dev/null) | sort -u); do
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
}

if [ "$which" = gutenberg ]; then
        [ $# -gt 0 ] || set -- $(sh "$work/sh/$script" --catalog 2>/dev/null |
                                 awk -F '\t' '$1 == "translation" { print $2 }')
        pass=0
        fail=0
        for number in "$@"; do
                names=$(sh "$work/sh/$script" --catalog 2>/dev/null |
                        awk -F '\t' -v n="$number" '$1 == "translation" && $2 == n { print $6 }')
                for how in page nopage; do
                        page=""
                        [ $how = page ] && [ -f "$source/$number.html" ] && page="--page $source/$number.html"
                        [ $how = page ] && [ -z "$page" ] && continue
                        for n in $names; do rm -rf "$work/sh/$n" "$work/c/$n"; done
                        grep '^AV ' "$repo/trans" > "$work/sh/trans"
                        grep '^AV ' "$repo/trans" > "$work/c/trans"
                        COLUMNS=80 sh "$work/sh/$script" --agree --at-end --file "$source/$number.txt" \
                                $page "$number" > "$work/sh.out" 2>&1 < /dev/null
                        echo "exit $?" >> "$work/sh.out"
                        COLUMNS=80 BIBLEROOT="$work/c" "$repo/bible" --import gutenberg --agree --at-end \
                                --file "$source/$number.txt" $page "$number" > "$work/c.out" 2>&1 < /dev/null
                        echo "exit $?" >> "$work/c.out"
                        problems=""
                        cmp -s "$work/sh.out" "$work/c.out" || problems="$problems
     what they said differs:
$(diff "$work/sh.out" "$work/c.out" | head -n 8 | sed 's/^/       /')"
                        for n in $names; do compare_folder "$n"; done
                        cmp -s "$work/sh/trans" "$work/c/trans" || problems="$problems
     trans differs"
                        if [ -z "$problems" ]; then
                                pass=$((pass + 1))
                                echo "ok   $number ($how): $names"
                        else
                                fail=$((fail + 1))
                                echo "FAIL $number ($how)$problems"
                        fi
                done
        done
        echo
        echo "$pass of $((pass + fail)) texts the same from both importers"
        [ $fail -eq 0 ]
        exit
fi

if [ $# -eq 0 ]; then
        set -- $(sh "$work/sh/$script" --source "$source" \
                ${EBAC_NAMES_SOURCE:+--names-source "$EBAC_NAMES_SOURCE"} --catalog 2>/dev/null |
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
