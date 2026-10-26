#!/bin/sh
# add-translation.sh -- download a Bible translation and add it to EBaC.
#
#   sh add-translation.sh             list the English translations, and
#                                     ask which to add
#   sh add-translation.sh BSB LEB     add these (by short name)
#   sh add-translation.sh --list      just list them
#
# The translations come from the plain-text Bibles of the bible_databases
# project (https://github.com/scrollmapper/bible_databases), made from
# CrossWire's SWORD modules. Each is one file: a "### Book" heading for each
# book, then one verse per line, "[1:1] In the beginning ...". For each
# translation chosen, this:
#
#   1. downloads it;
#   2. splits it into a file per book, in a folder named after it (such as
#      BSB), and writes the folder's booklist: the books, in order;
#   3. builds its concordance index (runs EBaC's index program in it), and
#      checks that the index has every verse;
#   4. adds it to 'trans', so that EBaC offers it under C)hange translation.
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
# Translations with more verses than EBaC can number (32767), such as those
# with the Apocrypha, aren't offered.
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
MAX_VERSES=32767        # EBaC numbers verses with 16 bits
# More verses than that (they include the Apocrypha); not offered for now
TOO_LARGE="KJVA DRC CPDV"

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

# fetch PATH FILE: copy a file of the source to FILE. Sets problem on failure.
fetch() {
        case $source in
        http://* | https://*)
                url=${source%/}/$1
                if command -v curl > /dev/null 2>&1; then
                        code=$(curl -sSL --max-time 600 -o "$2" -w '%{http_code}' "$url")
                        status=$?
                        if [ $status -eq 0 ] && [ "$code" -ge 400 ] 2>/dev/null; then
                                problem="couldn't download $url ($code)"
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
                if [ ! -e "$source/$1" ]; then
                        problem="couldn't read $source/$1 (No such file or directory)"
                        return 1
                fi
                if ! cp "$source/$1" "$2" 2> /dev/null; then
                        problem="couldn't read $source/$1"
                        return 1
                fi
                ;;
        esac
        return 0
}

# The English translations, one per line in $tmp/list: name, tab, title.
english_translations() {
        fetch "$LIST_PATH" "$tmp/list.md" || return 1
        $AWK -v too_large=" $TOO_LARGE " "$PLAIN"'
        # - **BSB (en)**: BSB: Berean Standard Bible
        /^- \*\*[^ \t]+ \(en\)\*\*: / {
                name = substr($0, 5)
                name = substr(name, 1, index(name, " ") - 1)
                if (index(too_large, " " name " ") > 0)
                        next
                title = substr($0, index($0, "**: ") + 4)
                sub(/^[^ \t]+: /, "", title)        # the short name again
                print name "\t" plain(title)
        }' "$tmp/list.md" > "$tmp/list"
        if [ ! -s "$tmp/list" ]; then
                problem="found no English translations in the list at $source"
                return 1
        fi
        return 0
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
        $AWK -F '\t' -v n="$1" '$1 == n { print $2; exit }' "$tmp/list"
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

# How many verses the index found: the last number in bvarray.dat.
index_verses() {
        od -An -tu1 "$1/bvarray.dat" |
        $AWK '{ for (i = 1; i <= NF; i++) b[++n] = $i }
              END { print b[n - 3] * 256 + b[n - 2] }'
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

add_to() {
        name=$1
        title=$2
        folder=$here/$name
        printf '%s: downloading...\n' "$name"
        fetch "formats/txt/$name.txt" "$tmp/text" || return 1

        # Read the text once to check it, and to plan the books: for each
        # book with text, its number among the headings and its file name.
        # Prints the plan, then "ok BOOKS VERSES", or "problem MESSAGE".
        $AWK -v max="$MAX_VERSES" "$PLAIN"'
        function book_name(name,    word, limit, n, other) {
                word = name
                gsub(/[^A-Za-z0-9]/, "", word)
                if (word == "")
                        word = "Book"
                # a name like 1Samuel is shown as "1 Samuel", one character wider
                limit = (word ~ /^[0-9][A-Z][a-z]/) ? 14 : 15
                word = substr(word, 1, limit)
                if (word in taken) {            # (made the same by shortening)
                        for (n = 2; n < 10; n++) {
                                other = substr(word, 1, limit - 1) n
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
                        fail("it has no text")
                if (total > max)
                        fail("it has " total " verses, more than EBaC can number (" max ")")
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

        if ! is_listed "$name"; then
                printf '%-11s %s\n' "$name" "$title" >> "$here/trans"
        fi
        printf '%s: done. %s is ready: start EBaC and press C to choose it.\n' "$name" "$title"
        return 0
}

# Tell the web page EBaC runs in, in the Home Assistant app, that the user
# is typing a line (see scr_mode() in curses.c): a tap brings up the phone's
# keyboard, and a swipe left erases a character.
typing_mode() {
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;input\007'
        return 0
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
choose() {
        first=0
        note=""
        count=$($AWK 'END { print NR }' "$tmp/list")
        listed_translations > "$tmp/listed"
        while :; do
                screen_size
                per_page=$((rows - 5))
                [ $per_page -ge 3 ] || per_page=3
                pages=$(( (count + per_page - 1) / per_page ))
                [ $first -le $(( (pages - 1) * per_page )) ] || first=$(( (pages - 1) * per_page ))
                if [ -n "$from_ebac" ]; then back="back to EBaC"; else back="stop"; fi
                clear_screen
                $AWK -F '\t' -v first="$first" -v per_page="$per_page" \
                    -v pages="$pages" -v cols="$cols" -v note="$note" -v back="$back" '
                FNR == NR { added[$0] = 1; next }
                {
                        name[FNR] = $1
                        title[FNR] = $2
                }
                END {
                        heading = "Add a translation to EBaC"
                        if (pages > 1)
                                heading = heading " (page " (int(first / per_page) + 1) " of " pages ")"
                        print substr(heading, 1, cols - 1)
                        print ""
                        for (i = first + 1; i <= first + per_page && i <= FNR; i++) {
                                mark = (name[i] in added) ? "  (added)" : ""
                                line = sprintf("%3d) %-12s", i, name[i])
                                t = title[i]
                                room = cols - 1 - length(line) - length(mark)
                                if (length(t) > room)
                                        t = (room > 3) ? substr(t, 1, room - 3) "..." : ""
                                print substr(line t mark, 1, cols - 1)
                        }
                        print substr(note, 1, cols - 1)
                        if (cols - 1 >= 66)
                                print "Type the numbers of the translations to add, as 6 14, and Enter."
                        else
                                print "Numbers to add (as 6 14), Enter."
                }' "$tmp/listed" "$tmp/list"
                note=""
                pg=""
                if [ $((cols - 1)) -ge 66 ]; then
                        [ $pages -gt 1 ] && pg="n, p: next, previous page; "
                        printf '(%sEnter alone: %s): ' "$pg" "$back"
                else
                        [ $pages -gt 1 ] && pg="n/p: page; "
                        printf '(%sEnter: %s): ' "$pg" "$back"
                fi
                typing_mode
                answer=""
                read -r answer || answer=""
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
                chosen=""
                unknown=""
                for word in "$@"; do
                        found=$(lookup "$word")
                        if [ -n "$found" ]; then
                                chosen="$chosen $found"
                        elif too_large "$word"; then
                                unknown="$unknown, $word (too big for EBaC)"
                        else
                                unknown="$unknown, $word"
                        fi
                done
                if [ -n "$unknown" ]; then
                        note="Not in the list: ${unknown#, }"
                        continue
                fi
                return 0
        done
}

too_large() {
        for big in $TOO_LARGE; do
                [ "$(printf '%s\n' "$1" | tr 'a-z' 'A-Z')" = "$big" ] && return 0
        done
        return 1
}

usage() {
        echo "usage: sh add-translation.sh [--list] [--source PLACE] [NAME ...]"
        echo "Downloads Bible translations and adds them to EBaC."
        echo "  NAME            short names of translations to add, such as BSB"
        echo "  --list          only list the translations"
        echo "  --source PLACE  where the translations are: a web address, or a folder"
        echo "                  with a copy of bible_databases (default: $SOURCE)"
}

# ---- the arguments
source=$SOURCE
list_only=no
names=""
while [ $# -gt 0 ]; do
        case $1 in
        --list) list_only=yes ;;
        --source)
                [ $# -ge 2 ] || { usage >&2; exit 2; }
                source=$2; shift ;;
        --source=*) source=${1#--source=} ;;
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

if ! english_translations; then
        printf 'add-translation: %s\n' "$problem"
        if [ -n "$from_ebac" ]; then
                printf '\nPress Enter to go back to EBaC. '
                typing_mode
                read -r answer
                back_to_ebac
        fi
        exit 1
fi

if [ $list_only = yes ]; then
        echo "English translations that can be added to EBaC:"
        echo
        listed_translations > "$tmp/listed"
        $AWK -F '\t' 'FNR == NR { added[$0] = 1; next }
                { printf "%3d) %-12s %s%s\n", FNR, $1, $2, ($1 in added) ? "  (added)" : "" }' \
                "$tmp/listed" "$tmp/list"
        exit 0
fi

chosen=""
for word in $names; do         # (by name: numbers are only for the list)
        found=$($AWK -F '\t' -v w="$word" 'tolower($1) == tolower(w) { print $1; exit }' "$tmp/list")
        if [ -z "$found" ]; then
                printf '%s: not in the list of English translations.\n' "$word"
                too_large "$word" && echo "    (It has more verses than EBaC can number.)"
                failed=$((failed + 1))
        else
                chosen="$chosen $found"
        fi
done
if [ -z "$names" ]; then
        choose
        [ -n "$chosen" ] && clear_screen
fi

for name in $chosen; do
        if add "$name" "$(title_of "$name")"; then
                start_with=$name
        else
                printf '%s: not added: %s.\n' "$name" "$problem"
                failed=$((failed + 1))
        fi
done

if [ -n "$from_ebac" ]; then
        if [ $failed -gt 0 ]; then
                printf '\nPress Enter to go back to EBaC. '
                typing_mode
                read -r answer
        fi
        back_to_ebac
fi
[ $failed -eq 0 ]
