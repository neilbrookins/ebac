#!/bin/sh
# add-thiagobodruk.sh -- add a Bible translation from thiagobodruk/bible on
#                        GitHub to EBaC (or remove one added so).
#
#   sh add-thiagobodruk.sh             list the languages, then a language's
#                                      translations, and ask which to add or
#                                      remove
#   sh add-thiagobodruk.sh NIV-TB      add it (by short name)
#   sh add-thiagobodruk.sh --agree NIV-TB  add it, agreeing to its license
#                                      (for scripts: no one to answer)
#   sh add-thiagobodruk.sh --remove NIV-TB  remove it
#   sh add-thiagobodruk.sh --list      just list them
#   --source PLACE                     read them from PLACE instead: a web
#                                      address, or a folder with a copy of
#                                      the repository (for testing)
#
# The translations come from https://github.com/thiagobodruk/bible: some
# 140, in some 85 languages, each one file of XML (and one of JSON, the same
# text), made by its author's crawler from Bible sites. Its list,
# json/index.json, gives each one's title and short name, as "en_kjva": the
# language's code, then the translation's. A translation is
#
#   <bible><b id="gn" n="Genesis"><c n="1"><v n="1">In the beginning
#   ...</v>...</c>...</b>...</bible>
#
# a b (book: its name, as the translation has it, in n), its c (chapters)
# and their v (verses), numbered; the whole file on one line. Each is read
# into the plain-text layout of scrollmapper/bible_databases ("### Genesis", then "[1:1]
# In the beginning ..."), and from there is added as add-scrollmapper.sh
# adds those from scrollmapper/bible_databases: the same lists, license question, books, index and
# 'trans' (importer.sh, which both read). So the same text comes out the
# same from either source; to check, the King James Version from each can
# be read side by side.
#
# Short names. EBaC knows each by the translation's own short name, in
# capitals, with "-TB" after it (for Thiago Bodruk, whose collection it is):
# en_kjva is KJVA-TB, en_niv NIV-TB, so that one from scrollmapper/bible_databases (KJVA),
# from Project Gutenberg (DRC-PG) and from here (KJVA-TB) can be kept side by
# side. The folder of each it adds has a file, thiagobodruk, saying so: this
# script removes only those, and the other importers leave them alone.
#
# Licenses. The collection's code and files are MIT-licensed, but it says of
# the texts: "All the Bible versions are property of their respective
# owners. All rights reserved to the owners." Some are in the public domain
# (the King James Version); others are not (the New International Version,
# the New Living Translation): what may be done with them is their owners'
# to say. So each is added only by someone who agrees, asked every time, as
# for scrollmapper/bible_databases.
#
# Versions: the XML file's ETag (or, from a folder, its checksum), kept in
# the translation's folder as add-scrollmapper.sh keeps it: the same version
# again is refused.
#
# EBaC runs this itself for T)hiagobodruk/bible under A)dd on its main menu,
# as "sh add-thiagobodruk.sh --from-ebac PROGRAM NAME", and is started again
# when it is done, as for add-scrollmapper.sh.

LC_ALL=C
export LC_ALL
set -f                  # no file-name patterns: a typed * stays a *

SOURCE=https://raw.githubusercontent.com/thiagobodruk/bible/master
LIST_PATH=json/index.json
PROGRAM=add-thiagobodruk
OWN_MARK=thiagobodruk
SUFFIX=-TB
LICENSE_PAGE=https://github.com/thiagobodruk/bible#what-about-the-license-and-the-copyrights

here=$(cd "$(dirname "$0")" && pwd)
if [ ! -f "$here/importer.sh" ]; then
        echo "add-thiagobodruk: importer.sh is missing (it belongs beside this program)." >&2
        exit 1
fi
. "$here/importer.sh"

# Every translation, one per line in $tmp/all: short name (KJVA-TB), tab,
# its language's code (en), tab, title, tab, its name in the source
# (en_kjva). The list is JSON, read by its quoted strings, whatever its
# lines: a string followed by ":" is a key, the next string its value; a
# version (an object with "name" and "abbreviation") ends at its "}".
all_translations() {
        fetch "$LIST_PATH" "$tmp/list.json" || return 1
        $AWK -v suffix="$SUFFIX" "$PLAIN"'
        BEGIN { RS = "\"" }
        { token[NR] = $0 }
        END {
                # the strings are the even tokens (between quotes), the
                # rest what is between them
                for (i = 2; i <= NR; i += 2) {
                        if (token[i + 1] ~ /^[ \t\r\n]*:[ \t\r\n]*$/) {
                                value[token[i]] = token[i + 2]
                                i += 2
                        }
                        if (index(token[i + 1], "}") && value["abbreviation"] != "") {
                                abbr = value["abbreviation"]
                                code = tolower(substr(abbr, 1, index(abbr, "_") - 1))
                                name = toupper(substr(abbr, index(abbr, "_") + 1)) suffix
                                print name "\t" code "\t" plain(value["name"]) "\t" abbr
                                delete value
                        }
                }
        }' "$tmp/list.json" > "$tmp/all"
        if [ ! -s "$tmp/all" ]; then
                problem="found no translations in the list at $source"
                return 1
        fi
        return 0
}

# The translation's name in the source (en_kjva), from its short name.
source_name() {
        $AWK -F '\t' -v n="$1" '$1 == n { print $4; exit }' "$tmp/all"
}

# license_of NAME: what the source says of its texts' licenses (the same for
# all of them), into license_line, and its page into details_line.
license_of() {
        license_line="the source says: property of its owner, all rights reserved"
        details_line=$LICENSE_PAGE
}

# version_of NAME: the fingerprint of its XML file in the source, without
# downloading it: the ETag a web server gives it (the last, after any
# redirects), or a folder's file's checksum. "" if it can't be had.
version_of() {
        path="xml/$(source_name "$1").xml"
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

# fetch_text NAME FILE: its XML, downloaded and read into the plain-text
# layout of scrollmapper/bible_databases in FILE: "### Book<tab>id" for each b (its n, and its
# id, the collection's short name for the book: gn, ex, 1sm, made from the
# Portuguese names in almost every translation, so typeable in any), then
# "[C:V]
# text" for each v, in the order the file has them. Read a tag at a time
# (the whole file is one line): each record is what follows a "<", a tag
# and then the text after it. XML's five named characters (&amp; and the
# rest) and numbered ones (&#8217;) become the characters themselves; the
# rest of the text is left as it is (importer.sh makes typographic
# punctuation plain, as for scrollmapper/bible_databases). A file with anything else in
# it is refused, rather than read wrongly.
fetch_text() {
        fetch "xml/$(source_name "$1").xml" "$tmp/xml" || return 1
        $AWK '
        # the UTF-8 bytes of character number n (as every awk can: printf
        # "%c" of a number is a byte in the C locale)
        function utf8(n) {
                if (n < 128)
                        return sprintf("%c", n)
                if (n < 2048)
                        return sprintf("%c%c", 192 + int(n / 64), 128 + n % 64)
                if (n < 65536)
                        return sprintf("%c%c%c", 224 + int(n / 4096),
                                       128 + int(n / 64) % 64, 128 + n % 64)
                return sprintf("%c%c%c%c", 240 + int(n / 262144), 128 + int(n / 4096) % 64,
                               128 + int(n / 64) % 64, 128 + n % 64)
        }
        function hex(s,    i, n) {
                n = 0
                s = tolower(s)
                for (i = 1; i <= length(s); i++)
                        n = n * 16 + index("0123456789abcdef", substr(s, i, 1)) - 1
                return n
        }
        function unescape(s,    out, i, j, ent) {
                if (!index(s, "&"))
                        return s
                out = ""
                while ((i = index(s, "&")) > 0) {
                        out = out substr(s, 1, i - 1)
                        s = substr(s, i)
                        j = index(s, ";")
                        ent = (j > 0) ? substr(s, 2, j - 2) : ""
                        if (ent == "amp") out = out "&"
                        else if (ent == "lt") out = out "<"
                        else if (ent == "gt") out = out ">"
                        else if (ent == "quot") out = out "\""
                        else if (ent == "apos") out = out "\047"
                        else if (ent ~ /^#[0-9]+$/) out = out utf8(substr(ent, 2) + 0)
                        else if (ent ~ /^#[xX][0-9a-fA-F]+$/) out = out utf8(hex(substr(ent, 3)))
                        else { out = out "&"; s = substr(s, 2); continue }
                        s = substr(s, j + 1)
                }
                return out s
        }
        # an attribute of a tag: n="..."
        function attr(tag, a,    p) {
                if (!match(tag, " " a "=\"[^\"]*\""))
                        return ""
                p = substr(tag, RSTART + length(a) + 3, RLENGTH - length(a) - 4)
                return unescape(p)
        }
        function fail(message) {
                print "problem " message
                failed = 1
                exit
        }
        BEGIN { RS = "<" }
        NR == 1 { next }        # (before the first tag: nothing)
        {
                close_at = index($0, ">")
                if (close_at == 0)
                        fail("a tag without its end, at tag " NR)
                tag = substr($0, 1, close_at - 1)
                text = substr($0, close_at + 1)
                name = tag              # (b, /b; a self-closing one, as b/)
                sub(/[ \t\r\n].*/, "", name)
                sub(/.\/$/, substr(name, 1, 1), name)
                if (name == "b") {
                        # (its name, and after a tab its short name in the
                        # source, its id: "gn", kept in abbreviations)
                        print "### " attr(tag, "n") "\t" attr(tag, "id")
                        chapter = ""
                } else if (name == "c")
                        chapter = attr(tag, "n")
                else if (name == "v") {
                        verse = attr(tag, "n")
                        if (chapter !~ /^[0-9]+$/ || verse !~ /^[0-9]+$/)
                                fail("a verse numbered \"" chapter ":" verse "\", not chapter:verse")
                        gsub(/[\r\n\t]/, " ", text)
                        text = unescape(text)
                        print "[" chapter ":" verse "]" ((text != "") ? " " text : "")
                } else if (name !~ /^(\?xml|bible|\/b|\/c|\/v|\/bible|!--.*)$/)
                        fail("a tag it doesn\047t know: <" substr(tag, 1, 30) ">")
                else if (name !~ /^\// && text ~ /[^ \t\r\n]/)
                        fail("text outside a verse: " substr(text, 1, 40))
        }' "$tmp/xml" > "$2" || { problem="couldn't read its XML"; return 1; }
        if grep '^problem ' "$2" > /dev/null; then
                problem="its XML: $(sed -n 's/^problem //p' "$2" | head -n 1)"
                return 1
        fi
        return 0
}

usage() {
        echo "usage: sh add-thiagobodruk.sh [--list] [--agree] [--source PLACE] [NAME ...] [--remove NAME ...]"
        echo "Downloads Bible translations from thiagobodruk/bible and adds them to EBaC, or"
        echo "removes them."
        echo "  NAME            short names of translations to add, such as NIV-TB"
        echo "  --remove NAME   remove this translation (added before)"
        echo "  --agree         agree to the license of each translation added (when"
        echo "                  run with no one to answer, as from a script)"
        echo "  --list          only list the translations"
        echo "  --catalog, --license NAME, --check NAME, --at-end, --agreed-how TEXT:"
        echo "                  for programs (as add-scrollmapper.sh's)"
        echo "  --source PLACE  where the translations are: a web address, or a folder"
        echo "                  with a copy of thiagobodruk/bible (default: $SOURCE)"
        echo "  --names-source PLACE  where the names of languages are (Unicode's CLDR)"
}

importer_main "$@"
