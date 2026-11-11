#!/bin/sh
# add-scrollmapper.sh -- download a Bible translation and add it to EBaC
#                        (or remove one added before).
#
#   sh add-scrollmapper.sh             list the languages, then a language's
#                                     translations, and ask which to add or
#                                     remove
#   sh add-scrollmapper.sh BSB-SM LEB-SM  add these (by short name)
#   sh add-scrollmapper.sh --agree BSB-SM  add it, agreeing to its license
#                                     (for scripts: see below)
#   sh add-scrollmapper.sh --remove BSB-SM  remove this one
#   sh add-scrollmapper.sh --list      just list them
#
# For programs (ebac-admin, which adds and removes translations for the
# EBaC integration's voice commands), plain lines separated by tabs:
#
#   --catalog         every language ("language", code, English name, own
#                     name, number of translations) and translation
#                     ("translation", name, language code, title, 1 if
#                     added), one per line
#   --license NAME    its title, license and details page, a line each
#                     ("Title:", "License:", "Details:")
#   --check NAME      "added" or "not added"; then, if added and the source
#                     has the same version, "same version", and the date
#                     downloaded
#   --at-end          adding: the translation goes at the end of 'trans',
#                     not the top (so that EBaC doesn't start with it: the
#                     integration's voice users don't change the app's)
#   --agreed-how TEXT with --agree: how it was agreed to, for its license
#                     file ("by voice, from Kitchen")
#
# The translations come from the plain-text Bibles of the bible_databases
# project (https://github.com/scrollmapper/bible_databases), made from
# CrossWire's SWORD modules. Each is one file: a "### Book" heading for each
# book, then one verse per line, "[1:1] In the beginning ...". For each
# translation chosen, this:
#
#   1. shows its license and asks whether you agree to it (if not, it is
#      skipped);
#   2. downloads it;
#   3. splits it into a file per book, in a folder named after it (such as
#      BSB-SM: its name in the source, and -SM, for scrollmapper, as
#      those from thiagobodruk/bible end in -TB, and those from Project
#      Gutenberg in -PG), and writes the folder's booklist: the books, in order (and
#      booknames: their full names, as the text has them; and license:
#      the license agreed to, and when);
#   4. builds its concordance index (runs EBaC's index program in it), and
#      checks that the index has every verse;
#   5. adds it to the top of 'trans', so that EBaC offers it under C)hange
#      translation, and starts with it from now on.
#
# Licenses. The translations are not all free to copy: most are public
# domain, but some are free only for non-commercial use, or may not be
# changed. So each is added only by someone who agrees to its license,
# asked every time, a fresh copy too (a license can change). The license
# is the line in the translation's README.md in the source ("**License:**
# GPL"); the details are on its page at CrossWire, whose modules the texts
# are made from. With no one to answer (standard input not a terminal, as
# in a script), a translation is added only with --agree, by which whoever
# runs this agrees to the license of each one named.
#
# Choosing a translation already added does the same again, a fresh copy
# at the top of 'trans', but only if the source's text has changed since:
# the same version is refused, saying so (remove it first, r and its
# number, to download it again). Each translation's folder keeps a file,
# version, with the text's fingerprint when it was downloaded: the ETag
# the web server gives the file (it changes when, and only when, the text
# does; asked for alone, without the text), or for a folder as the source,
# its checksum (cksum). Removing one deletes its folder and its line
# in 'trans'. In the list, a number adds that translation and r and a
# number (r6) removes it; several can be given at once ("r10 11 13"), and
# are done in that order.
#
# Book names are the translation's own, with spaces and punctuation left out
# (EBaC's book prompt, its @book searches and the makefile take one word),
# and cut short to fit the book column of EBaC's list of search results (15
# characters; 14 for one such as "1Samuel", shown as "1 Samuel"). Books with
# no text (the Old Testament of a New Testament translation) are left out.
# Typographic punctuation becomes plain ASCII (curly quotes, dashes), as in
# EBaC's own text, so that searches find words next to it; other characters
# stay as they are. Verse numbers keep their brackets: EBaC reads [1:1] as
# well as 1:1.
#
# Every translation there is offered, in every language, those with the
# Apocrypha too (before EBaC 2.1, they had more verses than it could number:
# 32,767). The list of them is read each time, so that new languages and
# translations appear as they are added there. First the languages are
# listed, each by its own name and its name in English ("Espanol, Spanish",
# with its accent), English first, then by the English names; then the
# translations of the language chosen. The list gives each translation's
# language only as a code (es), so the names come from Unicode (see
# NAMES_SOURCE and language_names()).
#
# EBaC runs this itself when A)dd a translation is chosen from its main menu,
# as "sh add-scrollmapper.sh --from-ebac PROGRAM NAME": PROGRAM is how to
# start EBaC again, and NAME the translation it was showing. EBaC is
# replaced by this program, and this one is replaced by a new EBaC when it
# is done, which starts with the translation just added (or else NAME). So
# it seems like one program, and the new EBaC reads 'trans' again, listing
# what was added.
#
# --source PLACE reads the translations from PLACE instead: a web address,
# or a folder with a copy of bible_databases.
#
# Needs only a POSIX shell and awk, and curl or wget to download (as on
# Linux, macOS, WSL, Solaris, and in the Home Assistant app). The text is
# handled as bytes (LC_ALL=C), so every awk treats it the same way. If
# EBaC's index program hasn't been built yet, it is built with make.

LC_ALL=C
export LC_ALL
set -f                  # no file-name patterns: a typed * stays a *

SOURCE=https://raw.githubusercontent.com/scrollmapper/bible_databases/master
LIST_PATH=docs/main_readme/translation_list.md
PROGRAM=add-scrollmapper
OWN_MARK=""             # (those from scrollmapper/bible_databases have no mark of their own)
SUFFIX=-SM

here=$(cd "$(dirname "$0")" && pwd)
if [ ! -f "$here/importer.sh" ]; then
        echo "add-scrollmapper: importer.sh is missing (it belongs beside this program)." >&2
        exit 1
fi
. "$here/importer.sh"

# Every translation, one per line in $tmp/all: name, tab, its language's
# code (as en, es, grc), tab, title.
all_translations() {
        fetch "$LIST_PATH" "$tmp/list.md" || return 1
        # Each short name ends in -SM, as those from thiagobodruk/bible end
        # in -TB, and those from Project Gutenberg in -PG: which source a
        # translation came from
        # (November 10, 2026, the designer: the same name, KJV, is in more
        # than one source). The name in the source (KJV) is the fourth
        # column, for fetching it.
        $AWK -v suffix="$SUFFIX" "$PLAIN"'
        # - **SpaRV (es)**: SpaRV: La Santa Biblia Reina-Valera (1909)
        /^- \*\*[^ \t]+ \([A-Za-z0-9-]+\)\*\*: / {
                name = substr($0, 5)
                name = substr(name, 1, index(name, " ") - 1)
                code = substr($0, index($0, " (") + 2)
                code = tolower(substr(code, 1, index(code, ")") - 1))
                title = substr($0, index($0, "**: ") + 4)
                sub(/^[^ \t]+: /, "", title)        # the short name again
                print name suffix "\t" code "\t" plain(title) "\t" name
        }' "$tmp/list.md" > "$tmp/all"
        if [ ! -s "$tmp/all" ]; then
                problem="found no translations in the list at $source"
                return 1
        fi
        # One added before names had -SM (WLC, not WLC-SM) is listed too,
        # under its old name, so it shows as added and r removes it (it is
        # then added again as WLC-SM): a folder of that name, without
        # another importer's mark, not one of EBaC's own (a link).
        $AWK -F '\t' '{ print $4 "\t" $2 "\t" $3 " (old name)\t" $4 }' \
                "$tmp/all" > "$tmp/old"
        while IFS='	' read -r old code title src; do
                [ -f "$here/$old/booklist" ] && [ ! -L "$here/$old" ] &&
                        ! others_text "$here/$old" &&
                        printf '%s\t%s\t%s\t%s\n' "$old" "$code" "$title" "$src" >> "$tmp/all"
        done < "$tmp/old"
        problem=""              # (others_text() says whose, of one not listed)
        return 0
}

# The translation's name in the source (KJV), from its short name (KJV-SM).
source_name() {
        $AWK -F '\t' -v n="$1" '$1 == n { print $4; exit }' "$tmp/all"
}

# license_of NAME: the license of a translation, from its README.md in the
# source, into license_line ("GPL"; "not stated" if it says "Null" or
# nothing), and its page at CrossWire into details_line. The README is "# NAME: TITLE", then "**License:** X" (or
# "**License**: X", or as LEB has it, the license alone on the next line).
CROSSWIRE=https://crosswire.org/sword/modules/
license_of() {
        code=$($AWK -F '\t' -v n="$1" '$1 == n { print $2; exit }' "$tmp/all")
        src=$(source_name "$1")
        license_line=""
        if fetch "sources/$code/$src/README.md" "$tmp/readme" 2> /dev/null; then
                license_line=$($AWK "$PLAIN"'
                        /^\*\*License:?\*\*:?/ {
                                sub(/^\*\*License:?\*\*:?/, "")
                                print plain($0); found = 1; exit
                        }
                        title && plain($0) != "" && !next_line { next_line = plain($0) }
                        /^# / { title = 1 }
                        END { if (!found && next_line ~ /^[A-Z]/) print next_line }' "$tmp/readme")
        fi
        case $license_line in
        ""|Null|null) license_line="not stated" ;;
        esac
        details_line="${CROSSWIRE}ModInfo.jsp?modName=$src"
}

# version_of NAME: the fingerprint of the translation's text in the source,
# without downloading it: the ETag a web server gives it (the last, after
# any redirects), or a folder's file's checksum. "" if it can't be had.
version_of() {
        path="formats/txt/$(source_name "$1").txt"
        case $source in
        http://* | https://*)
                url=${source%/}/$path
                if command -v curl > /dev/null 2>&1; then
                        curl -sSIL --max-time 60 "$url" 2> /dev/null
                elif command -v wget > /dev/null 2>&1; then
                        wget -q --spider --server-response "$url" 2>&1
                fi | $AWK 'tolower($1) == "etag:" { v = $2 } END { print v }' |
                        tr -d '\r'
                ;;
        *)
                [ -f "$source/$path" ] && cksum < "$source/$path" |
                        $AWK '{ print "cksum " $1 " " $2 }'
                ;;
        esac
}

# fetch_text NAME FILE: the text, as the source has it (already in the
# layout importer.sh reads)
fetch_text() {
        fetch "formats/txt/$(source_name "$1").txt" "$2"
}

usage() {
        echo "usage: sh add-scrollmapper.sh [--list] [--agree] [--source PLACE] [NAME ...] [--remove NAME ...]"
        echo "Downloads Bible translations and adds them to EBaC, or removes them."
        echo "  NAME            short names of translations to add, such as BSB-SM"
        echo "  --remove NAME   remove this translation (added before)"
        echo "  --agree         agree to the license of each translation added (when"
        echo "                  run with no one to answer, as from a script)"
        echo "  --list          only list the translations"
        echo "  --catalog, --license NAME, --check NAME, --at-end, --agreed-how TEXT:"
        echo "                  for programs (see the comments at the top)"
        echo "  --source PLACE  where the translations are: a web address, or a folder"
        echo "                  with a copy of bible_databases (default: $SOURCE)"
        echo "  --names-source PLACE  where the names of languages are (Unicode's CLDR)"
}

importer_main "$@"
