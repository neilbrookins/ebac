#!/bin/sh
# add-translation.sh -- download a Bible translation and add it to EBaC
#                        (or remove one added before).
#
#   sh add-translation.sh             list the languages, then a language's
#                                     translations, and ask which to add or
#                                     remove
#   sh add-translation.sh BSB LEB     add these (by short name)
#   sh add-translation.sh --agree BSB  add it, agreeing to its license
#                                     (for scripts: see below)
#   sh add-translation.sh --remove BSB  remove this one
#   sh add-translation.sh --list      just list them
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
#      BSB), and writes the folder's booklist: the books, in order (and
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
# Choosing a translation already added does the same again: a fresh copy,
# and at the top of 'trans'. Removing one deletes its folder and its line
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
# as "sh add-translation.sh --from-ebac PROGRAM NAME": PROGRAM is how to
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
# The names of languages, in English and in each language itself, from
# Unicode's Common Locale Data Repository (CLDR): the list of translations
# gives each one's language only as a code (such as es).
NAMES_SOURCE=https://raw.githubusercontent.com/unicode-org/cldr-json/main/cldr-json/cldr-localenames-full/main
# For a code Unicode has no names for (hbo, ancient Hebrew; tsg, Tausug):
# its English name from IANA's registry of language codes (BCP 47), which
# names every code.
REGISTRY_SOURCE=https://www.iana.org/assignments/language-subtag-registry
CACHE_FORMAT="# languages.cache: code, English name, own name, code named (EBaC 2.4.1)"

here=$(cd "$(dirname "$0")" && pwd)

# Solaris's /usr/bin/awk is the original awk of the 1970s, without
# functions; its newer awk is called nawk. Use nawk wherever there is one.
if command -v nawk > /dev/null 2>&1; then
        AWK=nawk
else
        AWK=awk
fi

# Functions for the awk programs, on the text's UTF-8 bytes (each written
# in octal, as every awk reads them):
#   plain(s)   typographic punctuation as plain ASCII, and white space as
#              single spaces, with none at either end (spaces(s));
#   spaces(s)  white space, as Unicode counts it, as single spaces, with
#              none at either end; "" for a blank line.
# (Each replacement is tried only when a quick index() finds its first
# bytes, as most lines have none of these: some awks are slow at gsub().)
PLAIN='
function plain(s) {
        if (index(s, "\342\200")) {
                gsub(/\342\200\230|\342\200\231/, "\047", s)     # curly single quotes
                gsub(/\342\200\234|\342\200\235/, "\"", s)       # curly double quotes
                gsub(/\342\200\224|\342\200\225/, "--", s)       # em dash, horizontal bar
                gsub(/\342\200\223/, "-", s)                     # en dash
                gsub(/\342\200\246/, "...", s)                   # ellipsis
        }
        if (index(s, "\302\266"))
                gsub(/\302\266/, "", s)                          # pilcrow (paragraph mark)
        return spaces(s)
}
function spaces(s) {
        # spaces other than the ASCII one: no-break, U+0085, U+1680,
        # U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F and U+3000
        if (index(s, "\302") || index(s, "\341") || index(s, "\342") || index(s, "\343")) {
                gsub(/\302\240|\302\205|\341\232\200|\343\200\200|\342\201\237/, " ", s)
                gsub(/\342\200\200|\342\200\201|\342\200\202|\342\200\203|\342\200\204/, " ", s)
                gsub(/\342\200\205|\342\200\206|\342\200\207|\342\200\210|\342\200\211/, " ", s)
                gsub(/\342\200\212|\342\200\250|\342\200\251|\342\200\257/, " ", s)
        }
        if (s ~ /[\t\r\v\f\034\035\036\037]/)
                gsub(/[\t\r\v\f\034\035\036\037]/, " ", s)
        if (index(s, "  "))
                gsub(/  +/, " ", s)
        sub(/^ /, "", s)
        sub(/ $/, "", s)
        return s
}'

tmp=""                  # a folder for downloads, removed at the end
adding=""               # the translation folder being made, if any
was_listed=no           # whether trans listed it before
from_ebac=""            # how to start EBaC again, when run from EBaC
start_with=""           # the translation the new EBaC starts with
failed=0
problem=""

cleanup() {
        [ -n "$tmp" ] && rm -rf "$tmp"
        tmp=""
}

# Start a new EBaC in this program's place (when run from EBaC).
back_to_ebac() {
        cleanup
        trap - EXIT INT
        exec "$from_ebac" "$start_with"
        printf 'add-translation: couldn\047t start EBaC again (%s).\n' "$from_ebac" >&2
        echo "Start it as usual, and the new translations will be there." >&2
        exit 1
}

# CTRL-C: leave no half-made translation, and go back to EBaC if run from it.
interrupted() {
        if [ -n "$adding" ] && [ "$was_listed" = no ] && [ -d "$adding" ]; then
                rm -rf "$adding"
        fi
        echo
        if [ -n "$from_ebac" ]; then
                echo "Stopped."
                back_to_ebac
        fi
        cleanup
        exit 1
}

# Functions for the awk programs, on the characters of UTF-8 text (in the C
# locale, awk sees bytes; a character beyond ASCII is two to four bytes, the
# first not a continuation byte, \200 to \277, each written in octal):
#   char_len(s, i)    the bytes of the character at byte i of s
#   char_cols(c)      the columns character c takes: 2 for Chinese, Japanese
#                     and Korean, 0 for a mark on the letter before it (an
#                     accent, a Hebrew vowel point), 1 for the rest
#   cut_cols(s, n)    as much of s as fits in n columns
#   title_first(s)    s with its first letter a capital (Unicode gives many
#                     languages' names in small letters; a list shows them
#                     with a capital): ASCII, Latin with accents, Greek,
#                     Cyrillic and Armenian
#   no_spaces(s)      whether s is in a script written without spaces
#                     between words: Chinese, Japanese, Thai, Lao, Burmese,
#                     Khmer (EBaC's search, word by word, works poorly in it)
CHARS='
function char_len(s, i,    n) {
        if (CONT == "")
                CONT = "\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
        n = 1
        while (i + n <= length(s) && index(CONT, substr(s, i + n, 1)))
                n++
        return n
}
function char_cols(c,    b, b2) {
        if (CONT == "")
                char_len("", 1)
        b = substr(c, 1, 1)
        b2 = substr(c, 2, 1)
        if (index("\343\344\345\346\347\350\351\352\353\354\355", b) ||
            (b == "\357" && (b2 == "\274" || b2 == "\275")))
                return 2
        if (b == "\314" || (b == "\315" && index(CONT, b2) <= 48) ||
            (b == "\326" && index(CONT, b2) >= 18 && b2 != "\276") ||
            (b == "\327" && index("\201\202\204\205\207", b2)))
                return 0
        return 1
}
function cut_cols(s, n,    i, len, used, w) {
        used = 0
        for (i = 1; i <= length(s); i += len) {
                len = char_len(s, i)
                w = char_cols(substr(s, i, len))
                if (used + w > n)
                        return substr(s, 1, i - 1)
                used += w
        }
        return s
}
function byte_of(n) {           # a continuation byte, by its value (128 to 191)
        if (CONT == "")
                char_len("", 1)
        return substr(CONT, n - 127, 1)
}
function title_first(s,    c, b, v, code) {
        if (CONT == "")
                char_len("", 1)
        c = substr(s, 1, 1)
        if (index("abcdefghijklmnopqrstuvwxyz", c))
                return toupper(c) substr(s, 2)
        b = substr(s, 2, 1)
        v = index(CONT, b) + 127                # the second byte, as a number
        if (v == 127)
                return s
        if (c == "\303" && v >= 160 && v <= 190 && v != 183)       # Latin-1
                return c byte_of(v - 32) substr(s, 3)
        if (c == "\304" || c == "\305") {                           # Latin Extended-A
                code = (c == "\304" ? 256 : 320) + v - 128
                if (((code >= 256 && code <= 311) || (code >= 330 && code <= 375)) && code % 2 == 1)
                        code--
                else if (((code >= 313 && code <= 328) || (code >= 377 && code <= 382)) && code % 2 == 0)
                        code--
                else
                        return s
                if (code < 320)
                        return "\304" byte_of(code - 256 + 128) substr(s, 3)
                return "\305" byte_of(code - 320 + 128) substr(s, 3)
        }
        if (c == "\316" && v >= 177 && v <= 191)                    # Greek
                return c byte_of(v - 32) substr(s, 3)
        if (c == "\317" && v >= 128 && v <= 137 && v != 130)
                return "\316" byte_of(v + 32) substr(s, 3)
        if (c == "\320" && v >= 176 && v <= 191)                    # Cyrillic
                return c byte_of(v - 32) substr(s, 3)
        if (c == "\321" && v >= 128 && v <= 143)
                return "\320" byte_of(v + 32) substr(s, 3)
        if (c == "\321" && v >= 144 && v <= 159)
                return "\320" byte_of(v - 16) substr(s, 3)
        if (c == "\325" || c == "\326") {                           # Armenian
                code = (c == "\325" ? 1344 : 1408) + v - 128   # (U+0540, U+0580)
                if (code < 1377 || code > 1414)                 # (U+0561 to U+0586)
                        return s
                code -= 48
                if (code < 1344)
                        return "\324" byte_of(code - 1280 + 128) substr(s, 3)
                return "\325" byte_of(code - 1344 + 128) substr(s, 3)
        }
        return s
}
function no_spaces(s) {
        return (s ~ /\343[\201-\203\220-\277]|[\344-\351][\200-\277]/ ||    # kana, Chinese
                s ~ /\340[\270-\273]/ ||                                # Thai, Lao
                s ~ /\341[\200-\202\236\237]/)                          # Burmese, Khmer
}
'


# fetch PATH FILE: copy a file of the source to FILE. Sets problem on failure.
fetch() {
        fetch_from "$source" "$1" "$2"
}

# fetch_from PLACE PATH FILE: copy PATH of PLACE (a web address, or a
# folder) to FILE. Sets problem on failure.
fetch_from() {
        from=$1
        shift
        case $from in
        http://* | https://*)
                url=${from%/}/$1
                if command -v curl > /dev/null 2>&1; then
                        http_code=$(curl -sSL --max-time 600 -o "$2" -w '%{http_code}' "$url")
                        status=$?
                        if [ $status -eq 0 ] && [ "$http_code" -ge 400 ] 2>/dev/null; then
                                problem="couldn't download $url ($http_code)"
                                return 1
                        fi
                        case $status in
                        0) ;;
                        6|7|28|35) problem="couldn't download $url (is this computer connected to the Internet?)"; return 1 ;;
                        *) problem="couldn't download $url (curl error $status)"; return 1 ;;
                        esac
                elif command -v wget > /dev/null 2>&1; then
                        # (wget says what went wrong itself)
                        if ! wget -q -O "$2" "$url"; then
                                problem="couldn't download $url"
                                return 1
                        fi
                else
                        problem="couldn't download $url: needs curl or wget, and neither is installed"
                        return 1
                fi
                ;;
        *)
                if [ ! -e "$from/$1" ]; then
                        problem="couldn't read $from/$1 (No such file or directory)"
                        return 1
                fi
                if ! cp "$from/$1" "$2" 2> /dev/null; then
                        problem="couldn't read $from/$1"
                        return 1
                fi
                ;;
        esac
        return 0
}

# Every translation, one per line in $tmp/all: name, tab, its language's
# code (as en, es, grc), tab, title.
all_translations() {
        fetch "$LIST_PATH" "$tmp/list.md" || return 1
        $AWK "$PLAIN"'
        # - **SpaRV (es)**: SpaRV: La Santa Biblia Reina-Valera (1909)
        /^- \*\*[^ \t]+ \([A-Za-z0-9-]+\)\*\*: / {
                name = substr($0, 5)
                name = substr(name, 1, index(name, " ") - 1)
                code = substr($0, index($0, " (") + 2)
                code = tolower(substr(code, 1, index(code, ")") - 1))
                title = substr($0, index($0, "**: ") + 4)
                sub(/^[^ \t]+: /, "", title)        # the short name again
                print name "\t" code "\t" plain(title)
        }' "$tmp/list.md" > "$tmp/all"
        if [ ! -s "$tmp/all" ]; then
                problem="found no translations in the list at $source"
                return 1
        fi
        return 0
}

# The translations of one language (by its code), in $tmp/list: name, tab,
# title.
translations_of() {
        $AWK -F '\t' -v code="$1" '$2 == code { print $1 "\t" $3 }' "$tmp/all" > "$tmp/list"
}

# The names of the languages, kept in languages.cache (beside this program):
# code, tab, its name in English, tab, its name in itself, tab, the code
# whose names they are (when only a shorter code has any: cop for cop-sa).
# A language's names come from Unicode the first time its code is seen in
# the list of translations, and again a month later; a code Unicode has no
# names for is shown as it is. Without the Internet, or when Unicode can't
# be reached, the names already kept are used.
language_names() {
        cache=$here/languages.cache
        if [ -f "$cache" ] && [ -n "$(find "$cache" -mtime +30 2> /dev/null)" ]; then
                rm -f "$cache"          # (a month old: the names again)
        fi
        # (its first line says what it is; one without it, as an EBaC 2.3.2
        # in the Home Assistant app wrote, keyed by a web status, goes)
        if [ -f "$cache" ] && [ "$(head -n 1 "$cache")" != "$CACHE_FORMAT" ]; then
                rm -f "$cache"
        fi
        [ -f "$cache" ] || echo "$CACHE_FORMAT" > "$cache" 2> /dev/null
        missing=$($AWK -F '\t' 'FILENAME == ARGV[1] { if ($1 !~ /^#/) kept[$1] = 1; next }
                !($2 in kept) && !($2 in seen) { seen[$2] = 1; print $2 }' \
                "$cache" "$tmp/all")
        [ -n "$missing" ] || return 0
        echo "Getting the names of the languages (from Unicode)..."
        fetch_from "$names_source" "en/languages.json" "$tmp/en.json" || return 0
        for lang_code in $missing; do
                # Unicode's name for the code: zh-hant is its zh-Hant (a
                # script, of 4 letters, with a capital; a region, of 2, in
                # capitals). If it has none, the code before the "-".
                full=$(printf '%s\n' "$lang_code" | $AWK -F - '{
                        out = $1
                        for (i = 2; i <= NF; i++)
                                if (length($i) == 4)
                                        out = out "-" toupper(substr($i, 1, 1)) substr($i, 2)
                                else if (length($i) == 2)
                                        out = out "-" toupper($i)
                                else
                                        out = out "-" $i
                        print out }')
                base=${lang_code%%-*}
                names_for=$full
                : > "$tmp/own.json"
                if ! fetch_from "$names_source" "$full/languages.json" "$tmp/own.json"; then
                        : > "$tmp/own.json"
                        if [ "$base" != "$full" ] &&
                           fetch_from "$names_source" "$base/languages.json" "$tmp/own.json"; then
                                names_for=$base
                        fi
                fi
                $AWK -v code="$lang_code" -v full="$(printf '%s' "$full" | tr 'A-Z' 'a-z')" \
                    -v base="$base" -v ownfor="$(printf '%s' "$names_for" | tr 'A-Z' 'a-z')" '
                # a line of languages.json:    "es": "espanol",
                function value(line) {
                        sub(/^[ \t]*"[^"]*": "/, "", line)
                        sub(/",?[ \t\r]*$/, "", line)
                        return line
                }
                function key(line) {
                        sub(/^[ \t]*"/, "", line)
                        return tolower(substr(line, 1, index(line, "\"") - 1))
                }
                /^[ \t]*"[^"]*": "/ {
                        k = key($0)
                        if (FILENAME == ARGV[1]) english[k] = value($0)
                        else own[k] = value($0)
                }
                END {
                        # the names of the whole code, or else of the code
                        # before its "-" (then shown with the whole code)
                        e = ""; o = ""; shorter = 0
                        if (full in english) e = english[full]
                        else if (base in english) { e = english[base]; shorter = 1 }
                        if (ownfor == full && (full in own)) o = own[full]
                        else if (base in own) { o = own[base]; if (full != base) shorter = 1 }
                        print code "\t" e "\t" o "\t" (shorter ? base : code)
                }' "$tmp/en.json" "$tmp/own.json" >> "$cache" 2> /dev/null
        done
        # Codes Unicode has no English name for: IANA's, from its registry
        # (records between %% lines: "Type: language", "Subtag: hbo",
        # "Description: Ancient Hebrew"; the first description is the name)
        unnamed=$($AWK -F '\t' '$1 !~ /^#/ && $2 == "" { print $1 }' "$cache")
        if [ -n "$unnamed" ] &&
           fetch_from "$REGISTRY_SOURCE" "language-subtag-registry" "$tmp/registry"; then
                $AWK -F '\t' -v OFS='\t' '
                FILENAME == ARGV[1] {
                        if ($0 ~ /^%%/) { type = ""; tag = ""; next }
                        if ($0 ~ /^Type: /) type = substr($0, 7)
                        else if ($0 ~ /^Subtag: /) tag = tolower(substr($0, 9))
                        else if ($0 ~ /^Description: / && type == "language" &&
                                 tag != "" && !(tag in name))
                                name[tag] = substr($0, 14)
                        next
                }
                $1 !~ /^#/ && $2 == "" {
                        base = $1
                        sub(/-.*/, "", base)
                        if ($1 in name) { $2 = name[$1]; $4 = $1 }
                        else if (base in name) { $2 = name[base]; $4 = base }
                }
                { print }' "$tmp/registry" "$cache" > "$tmp/cache" &&
                        cat "$tmp/cache" > "$cache"
        fi
        return 0
}

# The languages, one per line in $tmp/langs, in the order shown (English
# first, then by their names in English): code, tab, the name shown (its
# own name, then the English, as "Espanol, Spanish" with its accent), tab,
# its number of translations, tab, 1 if some of them are added, tab, 1 if it
# is written without spaces between words.
languages() {
        language_names
        listed_translations > "$tmp/listed"
        $AWK -F '\t' "$CHARS"'
        FILENAME == ARGV[1] { added[$0] = 1; next }
        FILENAME == ARGV[2] { english[$1] = $2; own[$1] = $3; whose[$1] = $4; next }
        {
                count[$2]++
                if ($1 in added) some[$2] = 1
                if (no_spaces($3)) nosp[$2] = 1
        }
        END {
                for (code in count) {
                        e = english[code]; o = own[code]
                        if (o != "") o = title_first(o)
                        if (o != "" && no_spaces(o)) nosp[code] = 1
                        if (o != "" && e != "" && tolower(o) != tolower(e))
                                shown = o ", " e
                        else if (e != "")
                                shown = e
                        else if (o != "")
                                shown = o
                        else
                                shown = code
                        if (whose[code] != "" && whose[code] != code)
                                shown = shown " [" code "]"
                        if (code == "en")
                                order = "0"
                        else if (e != "")
                                order = "1" tolower(e)
                        else
                                order = "2" code
                        print order "\t" code "\t" shown "\t" count[code] "\t" ((code in some) ? 1 : 0) "\t" ((code in nosp) ? 1 : 0)
                }
        }' "$tmp/listed" "$here/languages.cache" "$tmp/all" 2> /dev/null |
                sort -t "$(printf '\t')" -k1,1 | cut -f2- > "$tmp/langs"
}

# The folders 'trans' lists, one per line.
listed_translations() {
        [ -f "$here/trans" ] || return 0
        $AWK '$1 != "" && substr($1, 1, 1) != "#" { print $1 }' "$here/trans"
}

is_listed() {
        listed_translations | grep -x -F -e "$1" > /dev/null
}

# lookup WORD: the short name for a number or name (in any case), or "".
lookup() {
        $AWK -F '\t' -v w="$1" '
        (w ~ /^[0-9]+$/ && NR == w + 0) || tolower($1) == tolower(w) {
                print $1; exit
        }' "$tmp/list"
}

title_of() {
        $AWK -F '\t' -v n="$1" '$1 == n { print $3; exit }' "$tmp/all"
}

# The index program, built with make if it isn't there.
index_program() {
        if [ ! -x "$here/index" ] && [ -f "$here/makefile" ]; then
                make=make
                command -v gmake > /dev/null 2>&1 && make=gmake  # GNU make, on Solaris and the BSDs
                "$make" -C "$here" index > /dev/null
        fi
        if [ ! -x "$here/index" ]; then
                problem="couldn't build EBaC's index program (make index)"
                return 1
        fi
        return 0
}

# How many verses the index found: bvarray.dat ends with the number of the
# last verse (4 bytes, most significant first) and the number of chapters
# (4 bytes). (Before EBaC 2.1, 2 bytes each.)
index_verses() {
        od -An -tu1 "$1/bvarray.dat" |
        $AWK '{ for (i = 1; i <= NF; i++) b[++n] = $i }
              END { print ((b[n - 7] * 256 + b[n - 6]) * 256 + b[n - 5]) * 256 + b[n - 4] }'
}

# license_of NAME: the license of a translation, from its README.md in the
# source, into license_line ("GPL"; "not stated" if it says "Null" or
# nothing). The README is "# NAME: TITLE", then "**License:** X" (or
# "**License**: X", or as LEB has it, the license alone on the next line).
CROSSWIRE=https://crosswire.org/sword/modules/
license_of() {
        code=$($AWK -F '\t' -v n="$1" '$1 == n { print $2; exit }' "$tmp/all")
        license_line=""
        if fetch "sources/$code/$1/README.md" "$tmp/readme" 2> /dev/null; then
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
}

# agree NAME TITLE: show the translation's license, and ask whether the
# person adding it agrees (or --agree says so). Sets agreed to how ("typed
# y", "with --agree"); returns 1 if not, with problem saying why.
agree() {
        license_of "$1"
        printf '\n'
        # (print, not printf: the title may be beyond ASCII)
        $AWK -v l="$1: $2" -v cols="$cols" "$CHARS"'BEGIN { print cut_cols(l, cols - 1) }'
        printf 'License: %s\n' "$license_line"
        # (one line, never broken: the terminal wraps it on a narrow screen,
        # and a double-click still selects it whole, to copy)
        printf 'Details: %sModInfo.jsp?modName=%s\n' "$CROSSWIRE" "$1"
        if [ "$agree_all" = yes ]; then
                printf 'Agreed to, with --agree.\n'
                agreed="with --agree"
                return 0
        fi
        if [ ! -t 0 ]; then
                problem="no one to agree to its license (add --agree to agree to it)"
                return 1
        fi
        while :; do
                printf 'Do you agree to these terms (y/n)? '
                typing_mode
                # (the window changing size, as when a phone's keyboard comes
                # up, isn't an answer: read on, without asking again; what
                # was typed is kept)
                resized=0
                while read_line; status=$?; [ $status -eq 2 ]; do
                        resized=0
                done
                [ $status -eq 1 ] && answer=n                     # (end of input)
                case $(printf '%s\n' "$answer" | tr 'A-Z' 'a-z') in
                y|yes) agreed="typed y"; return 0 ;;
                n|no) problem="its license wasn't agreed to"; return 1 ;;
                *) printf 'Please type y or n.\n' ;;
                esac
        done
}

# add NAME TITLE: add one translation; on failure, leave no half-made folder
# behind (unless it was there, and listed in trans, before). Sets problem.
add() {
        adding=$here/$1
        if is_listed "$1"; then was_listed=yes; else was_listed=no; fi
        if ! add_to "$1" "$2"; then
                if [ "$was_listed" = no ] && [ -d "$adding" ]; then
                        rm -rf "$adding"
                fi
                adding=""
                return 1
        fi
        adding=""
        return 0
}

# trans without the line of translation NAME
without_line() {
        [ -f "$here/trans" ] || return 0
        $AWK -v n="$1" '$1 != n' "$here/trans"
}

# Whether translation NAME has been added: trans lists it, or its folder
# is here (not a link: the Home Assistant app's own translations are links)
is_added() {
        is_listed "$1" || { [ -f "$here/$1/booklist" ] && [ ! -L "$here/$1" ]; }
}

# remove NAME: delete an added translation's folder and its line in trans.
# Sets problem on failure.
remove() {
        name=$1
        if [ -L "$here/$name" ]; then
                problem="it is one of EBaC's own"
                return 1
        fi
        if ! is_added "$name"; then
                problem="it isn't added"
                return 1
        fi
        without_line "$name" > "$tmp/trans"
        if ! $AWK '$1 != "" && substr($1, 1, 1) != "#" { found = 1 } END { exit !found }' "$tmp/trans"; then
                problem="it is the only translation in trans"
                return 1
        fi
        cat "$tmp/trans" > "$here/trans" || { problem="couldn't change trans"; return 1; }
        rm -rf "$here/$name" || { problem="couldn't delete the folder $name"; return 1; }
        printf '%s: removed.\n' "$name"
        return 0
}

add_to() {
        name=$1
        title=$2
        folder=$here/$name
        printf '%s: downloading...\n' "$name"
        fetch "formats/txt/$name.txt" "$tmp/text" || return 1

        # Read the text once to check it, and to plan the books: for each
        # book with text, its number among the headings and its file name.
        # Prints the plan, then "ok BOOKS VERSES", or "problem MESSAGE".
        $AWK "$PLAIN"'
        # The first n characters of s: a character beyond ASCII is two to
        # four bytes, in UTF-8, the first not a continuation byte (\200 to
        # \277), so that a letter such as the e of Genesis, in Spanish, is
        # never cut in two.
        function first_chars(s, n,    i, count) {
                if (CONTINUATION == "")         # (octal, as every awk reads it)
                        CONTINUATION = "\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
                count = 0
                for (i = 1; i <= length(s); i++)
                        if (!index(CONTINUATION, substr(s, i, 1)) && ++count > n)
                                return substr(s, 1, i - 1)
                return s
        }
        function book_name(name,    word, limit, n, other) {
                word = name
                # (ASCII punctuation and spaces go, and the no-break space:
                # letters beyond ASCII, as in Genesis in Spanish, stay)
                gsub(/[\001-\057\072-\100\133-\140\173-\177]/, "", word)
                gsub(/\302\240/, "", word)
                if (word == "")
                        word = "Book"
                # a name like 1Samuel is shown as "1 Samuel", one character wider
                limit = (word ~ /^[0-9][A-Z][a-z]/) ? 14 : 15
                word = first_chars(word, limit)
                if (word in taken) {            # (made the same by shortening)
                        for (n = 2; n < 10; n++) {
                                other = first_chars(word, limit - 1) n
                                if (!(other in taken)) {
                                        word = other
                                        break
                                }
                        }
                }
                taken[word] = 1
                return word
        }
        function fail(message) {
                print "problem " message
                failed = 1
                exit
        }
        substr($0, 1, 4) == "### " {
                heading[++books] = spaces(substr($0, 5))
                chapter = ""
                next
        }
        spaces($0) == "" { next }
        {
                if (books == 0 || $0 !~ /^\[[0-9]+:[0-9]+\]/)
                        fail("line " NR " isn\047t a book heading or a verse: " substr($0, 1, 60))
                c = substr($0, 2, index($0, ":") - 2) + 0
                v = substr($0, index($0, ":") + 1)
                v = substr(v, 1, index(v, "]") - 1) + 0
                verses[books]++
                if (plain(substr($0, index($0, "]") + 1)) != "")
                        text[books] = 1
                # EBaC finds a chapter by its verse 1: every chapter must have one
                if (c != chapter && v != 1 && bad[books] == "")
                        bad[books] = c " starts at verse " v ", not 1"
                chapter = c
        }
        END {
                if (failed)
                        exit
                for (b = 1; b <= books; b++)
                        if (text[b]) {
                                kept++
                                total += verses[b]
                        }
                if (kept == 0)
                        fail("its text in the source is empty (only verse numbers)")
                for (b = 1; b <= books; b++)
                        if (text[b] && bad[b] != "")
                                fail(heading[b] " " bad[b])
                for (b = 1; b <= books; b++)
                        if (text[b])
                                print "book " b " " book_name(heading[b])
                print "ok " kept " " total
        }' "$tmp/text" > "$tmp/plan"
        result=$(tail -n 1 "$tmp/plan")
        case $result in
        "ok "*) ;;
        "problem "*) problem=${result#problem }; return 1 ;;
        *) problem="couldn't read the text"; return 1 ;;
        esac
        set -- $result
        books=$2
        total=$3

        printf '%s: writing %s books into the folder %s...\n' "$name" "$books" "$name"
        rm -rf "$folder"
        if ! mkdir "$folder"; then
                problem="couldn't make the folder $folder"
                return 1
        fi
        # Write each planned book's verses to its file, as "[1:1] text"
        $AWK -v folder="$folder" "$PLAIN"'
        FNR == NR {
                if ($1 == "book") {
                        file[$2] = $3
                        print $3 > (folder "/booklist")
                }
                next
        }
        substr($0, 1, 4) == "### " {
                b++
                if (out != "")
                        close(out)
                out = (b in file) ? folder "/" file[b] : ""
                # its full name, as the text has it (for ebac-api: "Song
                # of Solomon", which booklist has as SongofSolomon)
                if (b in file)
                        print plain(substr($0, 5)) > (folder "/booknames")
                next
        }
        out == "" || spaces($0) == "" { next }
        {
                c = substr($0, 2, index($0, ":") - 2) + 0
                v = substr($0, index($0, ":") + 1)
                v = substr(v, 1, index(v, "]") - 1) + 0
                t = plain(substr($0, index($0, "]") + 1))
                if (t != "")
                        print "[" c ":" v "] " t > out
                else
                        print "[" c ":" v "]" > out
        }' "$tmp/plan" "$tmp/text" || { problem="couldn't write the books"; return 1; }

        printf '%s: building the concordance index...\n' "$name"
        index_program || return 1
        if ! (cd "$folder" && "$here/index" -y > /dev/null); then
                problem="building the index failed"
                return 1
        fi
        found=$(index_verses "$folder")
        if [ "$found" != "$total" ]; then
                problem="the index has $found verses, but the text has $total"
                return 1
        fi

        # the license agreed to, and when
        { printf 'License: %s\n' "$license_line"
          printf 'Details: %sModInfo.jsp?modName=%s\n' "$CROSSWIRE" "$name"
          printf 'Agreed: %s (%s)\n' "$(date '+%Y-%m-%d')" "$agreed"
        } > "$folder/license" || { problem="couldn't write $name/license"; return 1; }

        # at the top of trans (moved there, if it was listed already), so that
        # EBaC starts with it from now on
        { printf '%-11s %s\n' "$name" "$title"
          without_line "$name"; } > "$tmp/trans" &&
                cat "$tmp/trans" > "$here/trans"   # (keeps its owner and mode)
        printf '%s: done. %s is ready: start EBaC and press C to choose it.\n' "$name" "$title"
        # Words without spaces between them (as Chinese, Japanese and Thai
        # are often written): EBaC's search goes word by word
        if [ -f "$tmp/text" ] && $AWK '/^\[[0-9]+:[0-9]+\] / {
                        t = substr($0, index($0, "] ") + 2)
                        sub(/[ \t\r]+$/, "", t)
                        if (t == "")
                                next            # (a verse with no text)
                        verses++
                        if (index(t, " ")) spaced++
                }
                END { exit !(verses > 0 && spaced * 2 < verses) }' "$tmp/text"; then
                printf '%s: note: its text has no spaces between words, so searching it works poorly.\n' "$name"
        fi
        return 0
}

# Tell the web page EBaC runs in, in the Home Assistant app, that the user
# is typing a line (see scr_mode() in curses.c): a tap brings up the phone's
# keyboard, and a swipe left erases a character.
typing_mode() {
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;input\007'
        return 0
}

# Before going back to EBaC: wait for a key, so that what was written can be
# read first. (On a phone, in the Home Assistant app, a tap is the key, as
# on EBaC's own "Press any key" screens.) The whole key is read, so that
# the rest of one such as an arrow key doesn't reach EBaC's menu.
press_any_key() {
        printf '\nPress any key to go back to EBaC. '
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;anykey\007'
        if [ -t 0 ] && command -v dd > /dev/null 2>&1 &&
           saved=$(stty -g 2> /dev/null); then
                stty -icanon -echo min 1 time 0 2> /dev/null
                dd bs=1 count=1 > /dev/null 2>&1
                # the rest of the key, if any: until nothing comes for 0.1 s
                stty min 0 time 1 2> /dev/null
                while [ "$(dd bs=64 count=1 2> /dev/null | wc -c)" -gt 0 ]; do
                        :
                done
                stty "$saved" 2> /dev/null
        else
                read -r answer
        fi
        echo
}

screen_size() {
        rows=$(stty size 2> /dev/null < /dev/tty)
        cols=${rows#* }
        rows=${rows% *}
        case $rows in ''|*[!0-9]*) rows=${LINES:-24} ;; esac
        case $cols in ''|*[!0-9]*) cols=${COLUMNS:-80} ;; esac
        [ "$rows" -ge 8 ] 2>/dev/null || rows=8
        [ "$cols" -ge 20 ] 2>/dev/null || cols=20
}

clear_screen() {
        [ -t 1 ] || return 0
        clear 2> /dev/null || tput clear 2> /dev/null || echo
}

# Show the translations a screenful at a time, and ask which to add.
# Sets chosen to their short names; none to stop.
# read_line: read a line typed into answer. Returns 2, with nothing read, if
# the window changes size meanwhile (or since the screen was drawn), so that
# it can be drawn again at the new size: as when a phone's keyboard comes up
# just after the list was drawn, which would push its top off the screen.
# 1 at the end of input.
read_line() {
        answer=""
        [ "$resized" = 1 ] && return 2
        if [ -n "${BASH_VERSION:-}" ]; then
                # (bash doesn't let a signal interrupt read: wait a second at
                # a time, and look in between)
                while :; do
                        read -r -t 1 answer && return 0
                        [ $? -gt 128 ] || { answer=""; return 1; }
                        [ "$resized" = 1 ] && { answer=""; return 2; }
                done
        fi
        read -r answer && return 0
        answer=""
        [ "$resized" = 1 ] && return 2  # (dash, BusyBox: read was interrupted)
        return 1
}

# Show the languages a screenful at a time, and ask for one. Sets lang to
# its code, and lang_name and lang_nospace; lang is "" to stop.
choose_language() {
        first=0
        note=""
        count=$($AWK 'END { print NR }' "$tmp/langs")
        while :; do
                resized=0               # (set by the WINCH trap)
                screen_size
                per_page=$((rows - 6))
                [ $per_page -ge 3 ] || per_page=3
                pages=$(( (count + per_page - 1) / per_page ))
                [ $first -le $(( (pages - 1) * per_page )) ] || first=$(( (pages - 1) * per_page ))
                if [ -n "$from_ebac" ]; then back="back to EBaC"; else back="stop"; fi
                clear_screen
                $AWK -F '\t' -v first="$first" -v per_page="$per_page" -v pages="$pages" \
                    -v cols="$cols" -v note="$note" "$CHARS"'
                {
                        shown[NR] = $2 " (" $3 ")"
                        some[NR] = $4
                        if ($4) any = 1
                }
                END {
                        # (two lines, so that the page number fits a narrow screen)
                        print cut_cols("Add or remove translations", cols - 1)
                        heading = "Choose a language"
                        if (pages > 1)
                                heading = heading " (page " (int(first / per_page) + 1) " of " pages ")"
                        print cut_cols(heading, cols - 1)
                        print any ? "* = has translations added" : ""
                        for (i = first + 1; i <= first + per_page && i <= NR; i++) {
                                n = i ""
                                while (length(n) < 2) n = " " n
                                print cut_cols((some[i] ? "*" : " ") n ") " shown[i], cols - 1)
                        }
                        print cut_cols(note, cols - 1)
                        # (one only: unlike the list of translations next,
                        # which takes several)
                        if (cols - 1 >= 44)
                                print "Type the number of one language, then Enter."
                        else
                                print "One language: its number, Enter."
                }' "$tmp/langs"
                pg=""
                if [ $((cols - 1)) -ge 66 ]; then
                        [ $pages -gt 1 ] && pg="n, p: next, previous page; "
                        printf '(%sEnter alone: %s): ' "$pg" "$back"
                else
                        [ $pages -gt 1 ] && pg="n/p: page; "
                        printf '(%sEnter: %s): ' "$pg" "$back"
                fi
                typing_mode
                read_line
                [ $? -eq 2 ] && continue        # resized: draw the list again
                note=""
                set -- $answer
                case $(printf '%s\n' "$1" | tr 'A-Z' 'a-z') in
                ""|x|q) lang=""; return 0 ;;
                n) [ $((first + per_page)) -lt $count ] && first=$((first + per_page)); continue ;;
                p) [ $first -gt 0 ] && first=$((first - per_page)); continue ;;
                esac
                case $1 in
                *[!0-9]*) ;;
                *)
                        if [ $# -eq 1 ] && [ "$1" -ge 1 ] && [ "$1" -le "$count" ]; then
                                lang=$($AWK -F '\t' -v n="$1" 'NR == n { print $1 }' "$tmp/langs")
                                lang_name=$($AWK -F '\t' -v n="$1" 'NR == n { print $2 }' "$tmp/langs")
                                lang_nospace=$($AWK -F '\t' -v n="$1" 'NR == n { print $5 }' "$tmp/langs")
                                return 0
                        fi
                        ;;
                esac
                if [ $# -gt 1 ]; then
                        # (as the list of translations takes: but here,
                        # one language, whose translations come next)
                        note="One language at a time: translations come next."
                else
                        note="Not in the list: $answer"
                fi
        done
}

# Show the translations of the language chosen (in $tmp/list) a screenful
# at a time, and ask which to add or remove. Sets chosen to them (as +NAME
# or -NAME); "" to go back to the languages.
choose() {
        first=0
        note=""
        count=$($AWK 'END { print NR }' "$tmp/list")
        listed_translations > "$tmp/listed"
        while :; do
                resized=0               # (set by the WINCH trap)
                screen_size
                per_page=$((rows - 6 - lang_nospace))
                [ $per_page -ge 3 ] || per_page=3
                pages=$(( (count + per_page - 1) / per_page ))
                [ $first -le $(( (pages - 1) * per_page )) ] || first=$(( (pages - 1) * per_page ))
                back="languages"
                clear_screen
                $AWK -F '\t' -v first="$first" -v per_page="$per_page" -v pages="$pages" \
                    -v cols="$cols" -v note="$note" -v language="$lang_name" \
                    -v nospace="$lang_nospace" "$CHARS"'
                FILENAME == ARGV[1] { added[$0] = 1; next }
                {
                        name[FNR] = $1
                        title[FNR] = $2
                        if ($1 in added)
                                any = 1
                }
                END {
                        # (two lines, so that the page number fits a narrow screen;
                        # a name too long for it is cut, not the page number)
                        print cut_cols("Add or remove translations", cols - 1)
                        page = (pages > 1) ? " (page " (int(first / per_page) + 1) " of " pages ")" : ""
                        print cut_cols(language, cols - 1 - length(page)) page
                        if (nospace)    # (search goes word by word)
                                print cut_cols("Note: often written without spaces between words; searching such a translation works poorly.", cols - 1)
                        print any ? "* = added already" : ""
                        # those already added are marked with a * before the number
                        for (i = first + 1; i <= first + per_page && i <= FNR; i++) {
                                line = sprintf("%s%2d) %-12s", (name[i] in added) ? "*" : " ",
                                               i, name[i])
                                if (length(name[i]) >= 12)
                                        line = line " "    # (a long name: a space still)
                                t = title[i]
                                room = cols - 1 - length(line)
                                if (cut_cols(t, room) != t)
                                        t = (room > 3) ? cut_cols(t, room - 3) "..." : ""
                                print cut_cols(line t, cols - 1)
                        }
                        print cut_cols(note, cols - 1)
                        if (cols - 1 >= 66)
                                print "Type numbers to add, as 6 14, or r6 to remove 6; then Enter."
                        else
                                print "Add: 6 14, remove: r6; then Enter."
                }' "$tmp/listed" "$tmp/list"
                pg=""
                if [ $((cols - 1)) -ge 66 ]; then
                        [ $pages -gt 1 ] && pg="n, p: next, previous page; "
                        printf '(%sEnter alone: %s): ' "$pg" "$back"
                else
                        [ $pages -gt 1 ] && pg="n/p: page; "
                        printf '(%sEnter: %s): ' "$pg" "$back"
                fi
                typing_mode
                read_line
                [ $? -eq 2 ] && continue        # resized: draw the list again
                note=""
                answer=$(printf '%s\n' "$answer" | tr ',' ' ')
                set -- $answer
                case $(printf '%s\n' "$1" | tr 'A-Z' 'a-z') in
                ""|x|q) chosen=""; return 0 ;;
                n|p)
                        if [ $# -eq 1 ]; then
                                if [ "$1" = n ] || [ "$1" = N ]; then
                                        [ $((first + per_page)) -lt $count ] && first=$((first + per_page))
                                else
                                        [ $first -gt 0 ] && first=$((first - per_page))
                                fi
                                continue
                        fi
                        ;;
                esac
                # each becomes +NAME (add) or -NAME (remove), in the order given
                chosen=""
                unknown=""
                not_added=""
                for word in "$@"; do
                        case $word in
                        [rR][0-9]*)
                                found=""
                                case ${word#?} in
                                *[!0-9]*) ;;
                                *) found=$(lookup "${word#?}") ;;
                                esac
                                if [ -z "$found" ]; then
                                        unknown="$unknown, $word"
                                elif is_added "$found"; then
                                        chosen="$chosen -$found"
                                else
                                        not_added="$not_added, $word"
                                fi
                                continue
                                ;;
                        esac
                        found=$(lookup "$word")
                        if [ -n "$found" ]; then
                                chosen="$chosen +$found"
                        else
                                unknown="$unknown, $word"
                        fi
                done
                if [ -n "$unknown" ] || [ -n "$not_added" ]; then
                        # nothing is done: the list again, saying what was wrong
                        if [ -n "$unknown" ] && [ -n "$not_added" ]; then
                                note="Not in the list: ${unknown#, }; not added: ${not_added#, }"
                        elif [ -n "$unknown" ]; then
                                note="Not in the list: ${unknown#, }"
                        else
                                note="Not added: ${not_added#, }"
                        fi
                        continue
                fi
                return 0
        done
}

usage() {
        echo "usage: sh add-translation.sh [--list] [--agree] [--source PLACE] [NAME ...] [--remove NAME ...]"
        echo "Downloads Bible translations and adds them to EBaC, or removes them."
        echo "  NAME            short names of translations to add, such as BSB"
        echo "  --remove NAME   remove this translation (added before)"
        echo "  --agree         agree to the license of each translation added (when"
        echo "                  run with no one to answer, as from a script)"
        echo "  --list          only list the translations"
        echo "  --source PLACE  where the translations are: a web address, or a folder"
        echo "                  with a copy of bible_databases (default: $SOURCE)"
        echo "  --names-source PLACE  where the names of languages are (Unicode's CLDR)"
}

# ---- the arguments
source=$SOURCE
names_source=$NAMES_SOURCE
list_only=no
agree_all=no
names=""
while [ $# -gt 0 ]; do
        case $1 in
        --list) list_only=yes ;;
        --agree) agree_all=yes ;;
        --source)
                [ $# -ge 2 ] || { usage >&2; exit 2; }
                source=$2; shift ;;
        --source=*) source=${1#--source=} ;;
        --names-source)
                [ $# -ge 2 ] || { usage >&2; exit 2; }
                names_source=$2; shift ;;
        --remove)
                [ $# -ge 2 ] || { usage >&2; exit 2; }
                names="$names -$2"; shift ;;
        --from-ebac)
                [ $# -ge 3 ] || { usage >&2; exit 2; }
                from_ebac=$2; start_with=$3; shift; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *) names="$names $1" ;;
        esac
        shift
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/ebac-add.XXXXXX") || exit 1
trap cleanup EXIT
trap interrupted INT
resized=0
trap 'resized=1' WINCH 2> /dev/null     # the window changed size (see read_line)

if ! all_translations; then
        printf 'add-translation: %s\n' "$problem"
        if [ -n "$from_ebac" ]; then
                press_any_key
                back_to_ebac
        fi
        exit 1
fi

if [ $list_only = yes ]; then
        echo "Translations that can be added to EBaC, by language:"
        languages
        # (print, not printf, for the names: an awk such as macOS's can't
        # printf text beyond ASCII)
        $AWK -F '\t' 'FILENAME == ARGV[1] { added[$0] = 1; next }
                FILENAME == ARGV[2] { shown[$1] = $2; order[++languages] = $1; next }
                { names[$2] = names[$2] (($1 in added) ? " *" : " ") $1 }
                END {
                        for (i = 1; i <= languages; i++)
                                print "\n" shown[order[i]] ":" names[order[i]]
                        print "\n* = added already"
                }' "$tmp/listed" "$tmp/langs" "$tmp/all"
        exit 0
fi

chosen=""
for word in $names; do         # (by name: numbers are only for the list)
        sign=+
        case $word in -*) sign=-; word=${word#-} ;; esac
        found=$($AWK -F '\t' -v w="$word" 'tolower($1) == tolower(w) { print $1; exit }' "$tmp/all")
        if [ -z "$found" ]; then
                printf '%s: not in the list of translations.\n' "$word"
                failed=$((failed + 1))
        else
                chosen="$chosen $sign$found"
        fi
done
if [ -z "$names" ]; then
        # a language, then its translations (Enter alone there: the
        # languages again)
        languages
        while :; do
                choose_language
                [ -n "$lang" ] || break
                translations_of "$lang"
                choose
                [ -n "$chosen" ] && break
        done
        [ -n "$chosen" ] && clear_screen
fi

for action in $chosen; do
        name=${action#?}
        case $action in
        -*)
                if remove "$name"; then
                        # (EBaC then starts with the first translation in trans)
                        [ "$start_with" = "$name" ] && start_with=""
                else
                        printf '%s: not removed: %s.\n' "$name" "$problem"
                        failed=$((failed + 1))
                fi
                ;;
        *)
                title=$(title_of "$name")
                screen_size
                if ! agree "$name" "$title"; then
                        printf '%s: not added: %s.\n' "$name" "$problem"
                        # (no is an answer, not a failure; no one to answer is)
                        [ ! -t 0 ] && failed=$((failed + 1))
                        continue
                fi
                if add "$name" "$title"; then
                        start_with=$name
                else
                        printf '%s: not added: %s.\n' "$name" "$problem"
                        failed=$((failed + 1))
                fi
                ;;
        esac
done

if [ -n "$from_ebac" ]; then
        # (straight back if nothing was chosen: there is nothing to read)
        if [ -n "$chosen" ] || [ $failed -gt 0 ]; then
                press_any_key
        fi
        back_to_ebac
fi
[ $failed -eq 0 ]
