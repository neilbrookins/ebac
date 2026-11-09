# importer.sh -- what EBaC's importers of Bible translations from GitHub
#                share: add-translation.sh (scrollmapper/bible_databases)
#                and add-thiagobodruk.sh (thiagobodruk/bible). Read by
#                them ("." in sh), not run itself.
#
# An importer sets, before reading this:
#   PROGRAM      its name, for messages ("add-translation")
#   SOURCE       where its translations are by default (a web address)
#   OWN_MARK     the file its texts' folders have, saying they are its own
#                ("" for none: scrollmapper/bible_databases, the first)
#   here         the folder EBaC is in
# and defines, for its source:
#   all_translations   every translation, one per line in $tmp/all: short
#                      name (as EBaC knows it), tab, language code (en,
#                      grc), tab, title
#   license_of NAME    its license: license_line ("GPL"), details_line (a
#                      web page with more)
#   version_of NAME    the text's fingerprint in the source, without
#                      downloading it ("" if it can't be had)
#   fetch_text NAME FILE  the text, into FILE, in the plain-text layout of
#                      scrollmapper/bible_databases: "### Book" before each book, then a
#                      verse a line, "[1:1] In the beginning..."; a book's
#                      heading may have its short name in the source after
#                      a tab ("### Genesis<tab>gn"), kept in the folder's
#                      abbreviations file, which EBaC lets a book be typed
#                      by too (match_it() in userint.c)
#   usage              how to run it
# then runs importer_main "$@". The rest (the lists of languages and
# translations, the license question, room on the disk, writing the books,
# the index, 'trans', removing) is here, the same for both: so a text
# comes out the same whichever source it is from, as long as its source's
# text is the same. (The design: "Importers" in readme.architecture.)

# The names of languages, in English and in each language itself, from
# Unicode's Common Locale Data Repository (CLDR): the list of translations
# gives each one's language only as a code (such as es).
NAMES_SOURCE=https://raw.githubusercontent.com/unicode-org/cldr-json/main/cldr-json/cldr-localenames-full/main
# For a code Unicode has no names for (hbo, ancient Hebrew; tsg, Tausug):
# its English name from IANA's registry of language codes (BCP 47), which
# names every code.
REGISTRY_SOURCE=https://www.iana.org/assignments/language-subtag-registry
CACHE_FORMAT="# languages.cache: code, English name, own name, code named (EBaC 2.4.1)"


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
        # (the terminal as it was, if read_line had changed it)
        [ -n "$tty_saved" ] && stty "$tty_saved" 2> /dev/null
        tty_saved=""
}

# Start a new EBaC in this program's place (when run from EBaC).
back_to_ebac() {
        cleanup
        trap - EXIT INT
        exec "$from_ebac" "$start_with"
        printf '%s: couldn\047t start EBaC again (%s).\n' "$PROGRAM" "$from_ebac" >&2
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


# Room on the disk. Adding stops while less than MIN_FREE_MB megabytes are
# free on the disk the translations are kept on, or on the one the text is
# downloaded to first (the temporary folder, TMPDIR or /tmp: often another
# disk, or another part of one), so that adding one after another (by a
# script, or by voice, say) can't fill either: a fixed amount, not a share
# of the disk, which would stop a large disk too soon and a small one too
# late. If the free space can't be found, adding stops too.
MIN_FREE_MB=1024

# enough_room: whether there is room to add a text, on both disks. Sets
# problem if not.
enough_room() {
        room_on "$here" "the disk EBaC keeps its translations on" &&
                room_on "$tmp" "the disk it downloads to ($(dirname "$tmp"))"
}

# room_on FOLDER WHAT: whether FOLDER's disk has MIN_FREE_MB free
room_on() {
        free_kb=$(df -Pk "$1" 2> /dev/null | $AWK 'NR == 2 { print $4 }')
        case $free_kb in
        "" | *[!0-9]*)
                problem="couldn't find how much space is free on $2, so nothing is added"
                return 1
                ;;
        esac
        if $AWK -v f="$free_kb" -v m="$MIN_FREE_MB" 'BEGIN { exit !(f < m * 1024) }'; then
                problem="not enough disk space: $(($free_kb / 1024)) MB free on $2, and adding needs at least $MIN_FREE_MB MB free"
                return 1
        fi
        return 0
}

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

# agree NAME TITLE: show the translation's license (license_of, the
# importer's own: license_line and details_line), and ask whether the
# person adding it agrees (or --agree says so). Sets agreed to how ("typed
# y", "with --agree"); returns 1 if not, with problem saying why.
agree() {
        license_of "$1"
        # (no blank line first: importer_main puts one between translations,
        # and before the first the screen is clear; with both, there were
        # two, November 8, 2026)
        # (print, not printf: the title may be beyond ASCII)
        $AWK -v l="$1: $2" -v cols="$cols" "$CHARS"'BEGIN { print cut_cols(l, cols - 1) }'
        printf 'License: %s\n' "$license_line"
        # (one line, never broken: the terminal wraps it on a narrow screen,
        # and a double-click still selects it whole, to copy)
        printf 'Details: %s\n' "$details_line"
        if [ "$agree_all" = yes ]; then
                if [ -n "$agreed_how" ]; then
                        printf 'Agreed to, %s.\n' "$agreed_how"
                        agreed=$agreed_how
                else
                        printf 'Agreed to, with --agree.\n'
                        agreed="with --agree"
                fi
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
                read_line --through-resize
                status=$?
                [ $status -eq 1 ] && answer=n                     # (end of input)
                case $(printf '%s\n' "$answer" | tr 'A-Z' 'a-z') in
                y|yes) agreed="typed y"; return 0 ;;
                n|no) problem="its license wasn't agreed to"; return 1 ;;
                *) printf 'Please type y or n.\n' ;;
                esac
        done
}

# others_text FOLDER: whether FOLDER holds a text another importer added,
# which this one neither writes over nor removes: its mark says whose
# (gutenberg: add-gutenberg.sh's; thiagobodruk: add-thiagobodruk.sh's;
# those from scrollmapper/bible_databases have none). Sets problem if so.
others_text() {
        if [ "$OWN_MARK" != gutenberg ] && [ -f "$1/gutenberg" ]; then
                problem="$(basename "$1") is a text from Project Gutenberg (A, then P, removes it)"
                return 0
        fi
        if [ "$OWN_MARK" != thiagobodruk ] && [ -f "$1/thiagobodruk" ]; then
                problem="$(basename "$1") is a text from thiagobodruk/bible (A, then T, removes it)"
                return 0
        fi
        return 1
}

# kept_version NAME: the fingerprint kept when it was downloaded, and the
# date, "FINGERPRINT<tab>DATE"; nothing if none was kept (added before
# EBaC 2.4.2, or by hand)
kept_version() {
        [ -f "$here/$1/version" ] || return 0
        $AWK '$1 == "Version:" { v = $2; for (i = 3; i <= NF; i++) v = v " " $i }
              $1 == "Downloaded:" { d = $2 }
              END { if (v != "") print v "\t" d }' "$here/$1/version"
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

# replace_trans FILE: 'trans' becomes FILE's lines, all at once: written
# beside it, then renamed in its place, so that another EBaC reading it
# meanwhile (another user's, or one pressing C) never reads it half written.
# Its permissions are kept (copied first, cp -p). A 'trans' that is a
# symbolic link is written through, in place, so the link stays one.
replace_trans() {
        if [ -L "$here/trans" ]; then
                cat "$1" > "$here/trans"
                return
        fi
        new="$here/trans.$$"
        { [ ! -f "$here/trans" ] || cp -p "$here/trans" "$new" 2> /dev/null || :; } &&
                cat "$1" > "$new" && mv -f "$new" "$here/trans" && return 0
        rm -f "$new"
        return 1
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
        others_text "$here/$name" && return 1
        if ! is_added "$name"; then
                problem="it isn't added"
                return 1
        fi
        without_line "$name" > "$tmp/trans"
        if ! $AWK '$1 != "" && substr($1, 1, 1) != "#" { found = 1 } END { exit !found }' "$tmp/trans"; then
                problem="it is the only translation in trans"
                return 1
        fi
        replace_trans "$tmp/trans" || { problem="couldn't change trans"; return 1; }
        rm -rf "$here/$name" || { problem="couldn't delete the folder $name"; return 1; }
        printf '%s: removed.\n' "$name"
        return 0
}

add_to() {
        name=$1
        title=$2
        folder=$here/$name
        # (another importer's text is never written over: their short names
        # end in their own suffix, -PG, -TB, so they shouldn't share one,
        # but if one ever does, it is left alone)
        others_text "$folder" && return 1
        enough_room || return 1
        printf '%s: downloading...\n' "$name"
        # the text, as the plain text of scrollmapper/bible_databases has it ("### Book", then
        # "[1:1] text"): the importer's own fetch_text
        fetch_text "$name" "$tmp/text" || return 1

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
                h = substr($0, 5)               # (a short name after a tab: not the name)
                if (index(h, "\t"))
                        h = substr(h, 1, index(h, "\t") - 1)
                heading[++books] = spaces(h)
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
                h = substr($0, 5)
                code = ""
                if (index(h, "\t")) {
                        code = spaces(substr(h, index(h, "\t") + 1))
                        h = substr(h, 1, index(h, "\t") - 1)
                }
                # its full name, as the text has it (for ebac-api: "Song
                # of Solomon", which booklist has as SongofSolomon)
                if (b in file)
                        print plain(h) > (folder "/booknames")
                # and its short name in the source, if it has one, with the
                # name EBaC knows the book by: "gn<tab>Genesis" (see the top)
                if ((b in file) && code != "")
                        print code "\t" file[b] > (folder "/abbreviations")
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
          printf 'Details: %s\n' "$details_line"
          printf 'Agreed: %s (%s)\n' "$(date '+%Y-%m-%d')" "$agreed"
        } > "$folder/license" || { problem="couldn't write $name/license"; return 1; }

        # the importer's mark, which says the text is its own (none for
        # scrollmapper/bible_databases, the first)
        if [ -n "$OWN_MARK" ]; then
                printf '%s\n' "$source" > "$folder/$OWN_MARK" ||
                        { problem="couldn't write $name/$OWN_MARK"; return 1; }
        fi

        # the text's fingerprint, to tell next time whether it has changed
        if [ -n "$version_now" ]; then
                { printf 'Version: %s\n' "$version_now"
                  printf 'Downloaded: %s\n' "$(date '+%Y-%m-%d')"
                } > "$folder/version" ||
                        { problem="couldn't write $name/version"; return 1; }
        else
                rm -f "$folder/version"     # (unknown: download next time)
        fi

        # at the top of trans (moved there, if it was listed already), so that
        # EBaC starts with it from now on; with --at-end, at the end (or
        # where it was, if listed already)
        if [ "$at_end" = yes ]; then
                if ! is_listed "$name"; then
                        { cat "$here/trans" 2> /dev/null
                          printf '%-11s %s\n' "$name" "$title"; } > "$tmp/trans" &&
                                replace_trans "$tmp/trans"
                fi
        else
                { printf '%-11s %s\n' "$name" "$title"
                  without_line "$name"; } > "$tmp/trans" &&
                        replace_trans "$tmp/trans"
        fi
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
# keyboard. typing_mode alone: a swipe left erases a character (as at the
# license question). typing_mode addlist: one of the lists of languages and
# translations, where a swipe up is the next page (n, Enter), down the
# previous (p, Enter), and left or right Enter (what was typed, or back).
typing_mode() {
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;%s\007' "${1:-input}"
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
# read_line [--through-resize]: read a line typed into answer. Returns 1 at
# the end of input; 2, with nothing read yet, if the window changes size
# meanwhile (or since the screen was drawn), so that the caller can draw it
# again for the new size, as when a phone's keyboard comes up or goes down
# (the WINCH trap sets resized); with --through-resize, it reads on
# instead, the screen being left as it is (the license question).
#
# At a terminal, it reads a key at a time, echoing it, and keeps what is
# typed so far in typed: so when it is called again after the screen is
# drawn again, it shows that first, and what is on the screen is what
# Enter will send. (Read by the terminal a line at a time, as before, the
# typed part was in the terminal's own buffer, unseen after the screen was
# drawn again, but sent all the same with Enter: November 8, 2026.)
# Backspace takes back a character (a whole one, in UTF-8), CTRL-U the
# line; CTRL-D, on an empty line, is the end of input; other control keys,
# and the rest of an escape sequence (an arrow key), are left out. Each
# key is a dd, which returns every half second when none is typed, so that
# a resize is seen in time. Otherwise (not a terminal, or no stty or dd),
# the line is read whole.
typed=""
tty_saved=""
read_line() {
        answer=""
        [ "$resized" = 1 ] && [ "$1" != --through-resize ] && return 2
        if [ ! -t 0 ] || ! command -v dd > /dev/null 2>&1 ||
           ! tty_saved=$(stty -g 2> /dev/null); then
                tty_saved=""
                read -r answer && return 0
                answer=""
                return 1
        fi
        stty -icanon -echo min 0 time 5 2> /dev/null
        printf '%s' "$typed"            # (shown again, after a redraw)
        skipping=""                     # (the rest of an escape sequence)
        while :; do
                if [ "$resized" = 1 ] && [ "$1" != --through-resize ]; then
                        stty "$tty_saved" 2> /dev/null
                        tty_saved=""
                        return 2
                fi
                key=$(dd bs=1 count=1 2> /dev/null; echo x)
                key=${key%x}
                case $key in
                "")     ;;              # (none typed in half a second)
                "$NL"|"$CR")
                        printf '\n'
                        answer=$typed
                        typed=""
                        break ;;
                "$DEL"|"$BS")
                        if [ -n "$typed" ]; then
                                typed=$(without_last "$typed")
                                printf '\b \b'
                        fi ;;
                "$CTRL_U")
                        while [ -n "$typed" ]; do
                                typed=$(without_last "$typed")
                                printf '\b \b'
                        done ;;
                "$CTRL_D")
                        if [ -z "$typed" ]; then
                                stty "$tty_saved" 2> /dev/null
                                tty_saved=""
                                return 1
                        fi ;;
                "$ESC") skipping=yes ;;
                [$CONTROL])
                        ;;              # (another control key: left out)
                *)
                        if [ -n "$skipping" ]; then
                                # (an escape sequence ends with a letter or ~)
                                case $key in [A-Za-z~]) skipping="" ;; esac
                        else
                                typed=$typed$key
                                printf '%s' "$key"
                        fi ;;
                esac
        done
        stty "$tty_saved" 2> /dev/null
        tty_saved=""
        return 0
}

# without_last TEXT: TEXT without its last character: in UTF-8, its last
# byte and, if that continues a character, the bytes back to its start
without_last() {
        rest=$1
        while :; do
                case $rest in
                *[$CONTINUATION]) rest=${rest%?} ;;
                *) break ;;
                esac
        done
        printf '%s' "${rest%?}"
}
NL=$(printf '\nx'); NL=${NL%x}
CR=$(printf '\r')
DEL=$(printf '\177')
BS=$(printf '\b')
CTRL_U=$(printf '\025')
CTRL_D=$(printf '\004')
ESC=$(printf '\033')
CONTROL=$(printf '\001-\037')           # (a range, in a pattern)
CONTINUATION=$(printf '\200-\277')      # (UTF-8's continuation bytes)

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
                # (on a page boundary for this size, which changes with the
                # window, as when a phone's keyboard comes and goes: the page
                # holding the entry that was at the top)
                [ $first -ge 0 ] || first=0
                first=$(( first / per_page * per_page ))
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
                typing_mode addlist
                read_line
                [ $? -eq 2 ] && continue        # resized: draw the list again
                note=""
                set -- $answer
                case $(printf '%s\n' "$1" | tr 'A-Z' 'a-z') in
                ""|x|q) lang=""; return 0 ;;
                n) [ $((first + per_page)) -lt $count ] && first=$((first + per_page)); continue ;;
                p) first=$((first - per_page)); [ $first -ge 0 ] || first=0; continue ;;
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
                # (on a page boundary for this size, which changes with the
                # window, as when a phone's keyboard comes and goes: the page
                # holding the entry that was at the top)
                [ $first -ge 0 ] || first=0
                first=$(( first / per_page * per_page ))
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
                typing_mode addlist
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
                                        first=$((first - per_page))
                                        [ $first -ge 0 ] || first=0
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

# importer_main ARGUMENTS: the importer, as its comments at the top say:
# its arguments, then a language and its translations to choose (or those
# named), each added or removed.
importer_main() {
        source=$SOURCE
        names_source=$NAMES_SOURCE
        list_only=no
        agree_all=no
        names=""
        mode=""                 # catalog, license or check: for programs
        mode_name=""
        at_end=no
        agreed_how=""
        while [ $# -gt 0 ]; do
                case $1 in
                --list) list_only=yes ;;
                --agree) agree_all=yes ;;
                --catalog) mode=catalog ;;
                --license|--check)
                        [ $# -ge 2 ] || { usage >&2; exit 2; }
                        mode=${1#--}; mode_name=$2; shift ;;
                --at-end) at_end=yes ;;
                --agreed-how)
                        [ $# -ge 2 ] || { usage >&2; exit 2; }
                        agreed_how=$2; shift ;;
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
                printf '%s: %s\n' "$PROGRAM" "$problem"
                if [ -n "$from_ebac" ]; then
                        press_any_key
                        back_to_ebac
                fi
                exit 1
        fi

        # For programs: plain lines, tab-separated (see the top)
        case $mode in
        catalog)
                languages > /dev/null
                $AWK -F '\t' -v OFS='\t' '
                        FILENAME == ARGV[1] { added[$0] = 1; next }
                        FILENAME == ARGV[2] { english[$1] = $2; own[$1] = $3; next }
                        FILENAME == ARGV[3] { print "language", $1, english[$1], own[$1], $3; next }
                        { print "translation", $1, $2, $3, (($1 in added) ? 1 : 0) }' \
                        "$tmp/listed" "$here/languages.cache" "$tmp/langs" "$tmp/all"
                exit 0
                ;;
        license|check)
                found=$($AWK -F '\t' -v w="$mode_name" 'tolower($1) == tolower(w) { print $1; exit }' "$tmp/all")
                if [ -z "$found" ]; then
                        printf '%s: not in the list of translations.\n' "$mode_name" >&2
                        exit 1
                fi
                if [ $mode = license ]; then
                        license_of "$found"
                        printf 'Title: %s\n' "$(title_of "$found")"
                        printf 'License: %s\n' "$license_line"
                        printf 'Details: %s\n' "$details_line"
                elif ! is_added "$found"; then
                        echo "not added"
                else
                        echo "added"
                        version_now=$(version_of "$found")
                        kept=$(kept_version "$found")
                        if [ -n "$version_now" ] && [ "${kept%%	*}" = "$version_now" ]; then
                                printf 'same version\t%s\n' "${kept#*	}"
                        fi
                fi
                exit 0
                ;;
        esac

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

        first=yes
        for action in $chosen; do
                name=${action#?}
                # A blank line between translations: each one's last line is how it
                # ended, and the blanks count the translations done.
                [ -z "$first" ] && printf '\n'
                first=""
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
                        # Added already, and the same version in the source: refused
                        # (nothing changes). A new version, or none kept: as a new one.
                        version_now=$(version_of "$name")
                        kept=$(kept_version "$name")
                        if [ -n "$version_now" ] && is_added "$name" &&
                           [ "${kept%%	*}" = "$version_now" ]; then
                                printf '%s: you already have this version (downloaded %s). To download it again, remove it first (r and its number), then add it.\n' \
                                        "$name" "${kept#*	}"
                                continue
                        fi
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
}
