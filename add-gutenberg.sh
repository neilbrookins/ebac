#!/bin/sh
# add-gutenberg.sh -- add a text from Project Gutenberg to EBaC, by its
#                     ebook number (or remove one added so).
#
#   sh add-gutenberg.sh                ask for the ebook number (EBaC's P)
#   sh add-gutenberg.sh 17             add ebook 17
#   sh add-gutenberg.sh --agree 17     the same, agreeing to its license (for
#                                      scripts: no one to answer)
#   sh add-gutenberg.sh --remove BOM-PG  remove one added by this script
#   sh add-gutenberg.sh --list         list those added by this script
#   sh add-gutenberg.sh --file PATH 17 read the text from PATH instead of
#                                      downloading it (for testing)
#   sh add-gutenberg.sh --page PATH 17 read its page from PATH instead of
#                                      gutenberg.org (for testing)
#
# The person adding a text finds it on gutenberg.org themselves, checks it
# is the one, and gives its ebook number here (17, the Book of Mormon: the
# "EBook-No." on its page, or the number in its address). Nothing is
# searched or listed here: what is added is their own choice. The address
# of its plain text isn't typed: it is made from the number, as Gutenberg
# gives it, https://www.gutenberg.org/ebooks/17.txt.utf-8 (see PLAIN_TEXT).
# Then, as add-scrollmapper.sh does for those from scrollmapper/bible_databases, this:
#
#   1. refuses it, by its fingerprint (asked for without downloading it),
#      if it is the same version as the copy added before (remove that
#      first);
#   2. reads its page on gutenberg.org (https://www.gutenberg.org/ebooks/17,
#      small: see PAGE), for its title, its last update and its copyright
#      status, shows them and asks whether you agree to its license; so
#      nothing is downloaded that isn't wanted (if its page can't be read,
#      the text is downloaded first, and its license read from it);
#   3. checks there is room on the disk, and downloads it;
#   4. splits it into a file per book, in a folder named by its short name
#      (BOM-PG), with booklist, booknames, license and version files (license:
#      what was agreed to, and the terms the text itself gives);
#   5. builds its concordance index, and checks it has every verse;
#   6. adds it to the top of 'trans', so that EBaC starts with it.
#
# One text, and one format, at a time (see "Texts from Project Gutenberg"
# in readme.architecture). Gutenberg's texts are laid out in many ways, so
# each is known by its ebook number, from the list below: its short name,
# and its books (their names, and the headings that start them). A text
# not in the list is refused. Each new one
# is looked at by hand and added to the list (with what its layout needs).
#
# Nothing the text gives is left out: its front matter (title page,
# testimonies, contents), each book's heading and summary, and its
# verses. Only Gutenberg's own header and license around the text, and the
# lines that number the chapters ("Jacob Chapter 1"), are dropped. The
# text before a book's chapter 1 (its heading and summary; before the
# first book, the front matter too) is its chapter 0, a verse for each row
# as the text lays it out: a title's line, a name, a line of contents, a
# blank line, or a paragraph (its lines joined where the text wrapped
# them, to be wrapped again to the screen). EBaC shows chapter 0 as plain
# text, without the 0s, which aren't in the text. (See put_intro().)
#
# This script adds and removes only its own texts (those whose folder has
# a file named gutenberg); add-scrollmapper.sh, those from
# scrollmapper/bible_databases.
#
# EBaC runs this itself for P)roject Gutenberg under A)dd on its main menu, as "sh
# add-gutenberg.sh --from-ebac PROGRAM NAME", and is started again when it
# is done, as for add-scrollmapper.sh.

LC_ALL=C
export LC_ALL
set -f

here=$(cd "$(dirname "$0")" && pwd)

if command -v nawk > /dev/null 2>&1; then
        AWK=nawk
else
        AWK=awk
fi

# ---- The texts this script knows ----------------------------------------
#
# A line per text: its ebook number, short name, format, title and
# language (for the list the EBaC integration reads, --catalog: what is
# said for it, and the language it is listed in); then a line per book:
# "book", its name (as EBaC shows it), and the start of the heading that
# begins it in the text.
#
# Formats:
#   chapters   books start with their heading (and a summary); a book of
#              several chapters has a line per chapter, "Alma Chapter 5";
#              verses are "C:V text", wrapped, a blank line between them,
#              and may start in the middle of a line, with the book's name
#              before them or not ("...them. 2:13 Neither", "4 Nephi 1:1").
#   paragraphs books start with their heading (and an introduction); a
#              chapter starts with "Genesis Chapter 1" (its summary after
#              it); a verse is a paragraph, "1:1. In the beginning...",
#              and the paragraphs between verses are notes on the verse
#              before them, or headings (in capitals) of the verse after
#              them. A "text" line is a book without chapters or verses
#              (an appendix): its rows only. (See split_paragraphs().)
#   verses     each book starts with a line "Book 01 Genesis" (its name
#              from there: no book lines needed); a verse is "001:001
#              text", its wrapped lines indented under it; anything before
#              the first book is its chapter 0. (See split_verses().)
#   sidebyside several translations in one text, each verse a number
#              "002.255" and then a paragraph per translation, each
#              starting with its key ("Y: "); a "translation" line per
#              translation: its key, short name, and where the text says
#              which it is (that line, after "= ", is its title). A
#              chapter starts with " Chapter 2:", then its heading lines
#              (its verse 0); the books are given by "book" lines, the
#              first chapter of each with them. (See split_sidebyside().)
#
# A text of several translations has their short names, separated by
# spaces, as its short name: each is added, and removed, on its own.
KNOWN='
16955	YUSUFALI-PG PICKTHALL-PG SHAKIR-PG	sidebyside	The Qur'"'"'an in three translations	en
translation	Y	YUSUFALI-PG
translation	P	PICKTHALL-PG
translation	S	SHAKIR-PG
book	Qur'"'"'an	1
8294	WEB-PG	verses	The World English Bible	en
1581	DRC-PG	paragraphs	The Douay-Rheims Bible	en
book	Genesis	THE BOOK OF GENESIS
book	Exodus	THE BOOK OF EXODUS
book	Leviticus	THE BOOK OF LEVITICUS
book	Numbers	THE BOOK OF NUMBERS
book	Deuteronomy	THE BOOK OF DEUTERONOMY
book	Josue	THE BOOK OF JOSUE
book	Judges	THE BOOK OF JUDGES
book	Ruth	THE BOOK OF RUTH
book	1 Kings	THE FIRST BOOK OF SAMUEL, OTHERWISE CALLED THE FIRST BOOK OF KINGS
book	2 Kings	THE SECOND BOOK OF SAMUEL, OTHERWISE CALLED THE SECOND BOOK OF KINGS
book	3 Kings	THE THIRD BOOK OF KINGS
book	4 Kings	THE FOURTH BOOK OF KINGS
book	1 Paralipomenon	THE FIRST BOOK OF PARALIPOMENON
book	2 Paralipomenon	THE SECOND BOOK OF PARALIPOMENON
book	1 Esdras	THE FIRST BOOK OF ESDRAS
book	2 Esdras	THE BOOK OF NEHEMIAS, WHICH IS CALLED THE SECOND OF ESDRAS
book	Tobias	THE BOOK OF TOBIAS
book	Judith	THE BOOK OF JUDITH
book	Esther	THE BOOK OF ESTHER
book	Job	THE BOOK OF JOB
book	Psalms	THE BOOK OF PSALMS
book	Proverbs	THE BOOK OF PROVERBS
book	Ecclesiastes	ECCLESIASTES
book	Canticle of Canticles	SOLOMON’S CANTICLE OF CANTICLES
book	Wisdom	THE BOOK OF WISDOM
book	Ecclesiasticus	ECCLESIASTICUS
book	Isaias	THE PROPHECY OF ISAIAS
book	Jeremias	THE PROPHECY OF JEREMIAS
book	Lamentations	THE LAMENTATIONS OF JEREMIAS
book	Baruch	THE PROPHECY OF BARUCH
book	Ezechiel	THE PROPHECY OF EZECHIEL
book	Daniel	THE PROPHECY OF DANIEL
book	Osee	THE PROPHECY OF OSEE
book	Joel	THE PROPHECY OF JOEL
book	Amos	THE PROPHECY OF AMOS
book	Abdias	THE PROPHECY OF ABDIAS
book	Jonas	THE PROPHECY OF JONAS
book	Micheas	THE PROPHECY OF MICHEAS
book	Nahum	THE PROPHECY OF NAHUM
book	Habacuc	THE PROPHECY OF HABACUC
book	Sophonias	THE PROPHECY OF SOPHONIAS
book	Aggeus	THE PROPHECY OF AGGEUS
book	Zacharias	THE PROPHECY OF ZACHARIAS
book	Malachias	THE PROPHECY OF MALACHIAS
book	1 Machabees	THE FIRST BOOK OF MACHABEES
book	2 Machabees	THE SECOND BOOK OF MACHABEES
book	Matthew	THE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO SAINT MATTHEW
book	Mark	THE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO ST. MARK
book	Luke	THE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO ST. LUKE
book	John	THE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO ST. JOHN
book	Acts	THE ACTS OF THE APOSTLES
book	Romans	THE EPISTLE OF ST. PAUL THE APOSTLE TO THE ROMANS
book	1 Corinthians	THE FIRST EPISTLE OF ST. PAUL TO THE CORINTHIANS
book	2 Corinthians	THE SECOND EPISTLE OF ST. PAUL TO THE CORINTHIANS
book	Galatians	THE EPISTLE OF ST. PAUL TO THE GALATIANS
book	Ephesians	THE EPISTLE OF ST. PAUL TO THE EPHESIANS
book	Philippians	THE EPISTLE OF ST. PAUL TO THE PHILIPPIANS
book	Colossians	THE EPISTLE OF ST. PAUL TO THE COLOSSIANS
book	1 Thessalonians	THE FIRST EPISTLE OF ST. PAUL TO THE THESSALONIANS
book	2 Thessalonians	THE SECOND EPISTLE OF ST. PAUL TO THE THESSALONIANS
book	1 Timothy	THE FIRST EPISTLE OF ST. PAUL TO TIMOTHY
book	2 Timothy	THE SECOND EPISTLE OF ST. PAUL TO TIMOTHY
book	Titus	THE EPISTLE OF ST. PAUL TO TITUS
book	Philemon	THE EPISTLE OF ST. PAUL TO PHILEMON
book	Hebrews	THE EPISTLE OF ST. PAUL TO THE HEBREWS
book	James	THE CATHOLIC EPISTLE OF ST. JAMES THE APOSTLE
book	1 Peter	THE FIRST EPISTLE OF ST. PETER THE APOSTLE
book	2 Peter	THE SECOND EPISTLE OF ST. PETER THE APOSTLE
book	1 John	THE FIRST EPISTLE OF ST. JOHN THE APOSTLE
book	2 John	THE SECOND EPISTLE OF ST. JOHN THE APOSTLE
book	3 John	THE THIRD EPISTLE OF ST. JOHN THE APOSTLE
book	Jude	THE CATHOLIC EPISTLE OF ST. JUDE
book	Apocalypse	THE APOCALYPSE OF ST. JOHN THE APOSTLE
text	Prayer of Manasses	APPENDICES
text	3 Esdras	THE THIRD BOOKE OF ESDRAS.
text	4 Esdras	THE FOVRTH BOOKE OF ESDRAS.
text	Abdias 1610	BOOKS FOR COMPARISON
text	Jude 1582	THE CATHOLIKE EPISTLE OF IVDE THE APOSTLE.
text	Preface 1582	THE PREFACE TO THE READER
text	Hard Words 1582	HARD VVORDES EXPLICATED
17	BOM-PG	chapters	The Book of Mormon	en
book	1 Nephi	THE FIRST BOOK OF NEPHI
book	2 Nephi	THE SECOND BOOK OF NEPHI
book	Jacob	THE BOOK OF JACOB
book	Enos	THE BOOK OF ENOS
book	Jarom	THE BOOK OF JAROM
book	Omni	THE BOOK OF OMNI
book	Words of Mormon	THE WORDS OF MORMON
book	Mosiah	THE BOOK OF MOSIAH
book	Alma	THE BOOK OF ALMA
book	Helaman	THE BOOK OF HELAMAN
book	3 Nephi	THIRD BOOK OF NEPHI
book	4 Nephi	FOURTH NEPHI
book	Mormon	THE BOOK OF MORMON
book	Ether	THE BOOK OF ETHER
book	Moroni	THE BOOK OF MORONI
'

# White space as single spaces, with none at either end; every other
# character as the ebook has it (curly quotes, dashes and the ellipsis too:
# EBaC counts every apostrophe as ' where words are compared, u8_fold() in
# utf8.c). As importer.sh's plain(), which this importer doesn't share.
PLAIN='
function plain(s) {
        gsub(/\302\240/, " ", s)                                 # no-break space
        gsub(/[\t\r\v\f]/, " ", s)
        gsub(/  +/, " ", s)
        sub(/^ /, "", s)
        sub(/ $/, "", s)
        return s
}'

tmp=""
adding=""                       # (the names being added: see drop_adding)
names=""
from_ebac=""
start_with=""
failed=0
problem=""

cleanup() {
        [ -n "$tmp" ] && rm -rf "$tmp"
        tmp=""
}

back_to_ebac() {
        cleanup
        trap - EXIT INT
        exec "$from_ebac" "$start_with"
        printf 'add-gutenberg: couldn\047t start EBaC again (%s).\n' "$from_ebac" >&2
        exit 1
}

interrupted() {
        drop_adding
        echo
        if [ -n "$from_ebac" ]; then
                echo "Stopped."
                back_to_ebac
        fi
        cleanup
        exit 1
}

# Tell EBaC's web page in the Home Assistant app that a line is being
# typed (see add-scrollmapper.sh).
typing_mode() {
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;%s\007' "${1:-input}"
        return 0
}

press_any_key() {
        printf '\nPress any key to go back to EBaC. '
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;anykey\007'
        if [ -t 0 ] && command -v dd > /dev/null 2>&1 &&
           saved=$(stty -g 2> /dev/null); then
                stty -icanon -echo min 1 time 0 2> /dev/null
                dd bs=1 count=1 > /dev/null 2>&1
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

# known NUMBER: the text's line of KNOWN ("17<tab>BOM-PG<tab>chapters"), or ""
known() {
        printf '%s\n' "$KNOWN" | $AWK -F '\t' -v n="$1" '
                $1 ~ /^[0-9]+$/ { this = ($1 == n); if (this) print; next }'
}

# known_detail NUMBER: its book and front lines
known_detail() {
        printf '%s\n' "$KNOWN" | $AWK -F '\t' -v n="$1" '
                $1 ~ /^[0-9]+$/ { this = ($1 == n); next }
                this && ($1 == "book" || $1 == "text" || $1 == "front" ||
                         $1 == "translation") { print }'
}

# fetch URL FILE (sets problem on failure)
fetch() {
        if command -v curl > /dev/null 2>&1; then
                http_code=$(curl -sSL --max-time 600 -o "$2" -w '%{http_code}' "$1")
                status=$?
                if [ $status -eq 0 ] && [ "$http_code" -ge 400 ] 2>/dev/null; then
                        problem="couldn't download $1 ($http_code)"
                        return 1
                fi
                case $status in
                0) ;;
                6|7|28|35) problem="couldn't download $1 (is this computer connected to the Internet?)"; return 1 ;;
                *) problem="couldn't download $1 (curl error $status)"; return 1 ;;
                esac
        elif command -v wget > /dev/null 2>&1; then
                wget -q -O "$2" "$1" || { problem="couldn't download $1"; return 1; }
        else
                problem="couldn't download $1: needs curl or wget, and neither is installed"
                return 1
        fi
        return 0
}

# version_of URL: what the server says of the file, asked for without
# downloading it, as "ETAG<tab>SIZE<tab>MODIFIED" (its fingerprint, its
# length and its Last-Modified date; of the last answer, after any
# redirect; any may be empty). With --file, its checksum as the ETag.
version_of() {
        if [ -n "$from_file" ]; then
                cksum < "$from_file" | $AWK '{ print "cksum " $1 " " $2 "\t\t" }'
                return
        fi
        if command -v curl > /dev/null 2>&1; then
                curl -sSIL --max-time 60 "$1" 2> /dev/null
        elif command -v wget > /dev/null 2>&1; then
                wget --spider --server-response "$1" 2>&1
        fi | tr -d '\r' | $AWK '
                { sub(/^[ \t]+/, "") }
                /^HTTP\// { e = ""; n = ""; m = "" }
                tolower($1) == "etag:" { e = $2 }
                tolower($1) == "content-length:" { n = $2 }
                tolower($1) == "last-modified:" { m = $0; sub(/^[^:]*:[ \t]*/, "", m) }
                END { print e "\t" n "\t" m }'
}

# kept_field NAME FIELD: a line of the version file of the copy kept
kept_field() {
        [ -f "$here/$1/version" ] || return 0
        $AWK -v f="$2:" '$1 == f { v = $0; sub(/^[^:]*:[ \t]*/, "", v) } END { print v }' "$here/$1/version"
}

# same_version NAME: whether the copy of NAME kept is the version offered
# now (etag, size, modified and updated set): by its fingerprint; or by
# its size and date both; or by the date its page says it was updated.
# (The fingerprint can differ from one of Gutenberg's servers to another;
# and after downloading, its checksum is compared too: see add_number.)
same_version() {
        [ -f "$here/$1/gutenberg" ] || return 1
        same_layout "$1" || return 1
        [ -n "$etag" ] && [ "$etag" = "$(kept_field "$1" Version)" ] && return 0
        [ -n "$size" ] && [ -n "$modified" ] &&
                [ "$size" = "$(kept_field "$1" Size)" ] &&
                [ "$modified" = "$(kept_field "$1" Modified)" ] && return 0
        [ -n "$updated" ] && [ "$updated" = "$(kept_field "$1" Updated)" ] && return 0
        return 1
}

# same_layout NAME: whether the copy of NAME kept was written in this
# script's layout (LAYOUT): if not, it is added again, to be shown as now
same_layout() {
        [ "$(kept_field "$1" Layout)" = "$LAYOUT" ]
}

# The texts this script added: a folder with a file named gutenberg
# ("Ebook: 17", "Address: ..."), listed in trans. NAME, tab, title.
ours() {
        [ -f "$here/trans" ] || return 0
        $AWK '$1 != "" && substr($1, 1, 1) != "#" { name = $1; $1 = ""; sub(/^ +/, ""); print name "\t" $0 }' \
                "$here/trans" | while IFS='	' read -r name title; do
                [ -f "$here/$name/gutenberg" ] && printf '%s\t%s\n' "$name" "$title"
        done
}

index_program() {
        if [ ! -x "$here/index" ] && [ -f "$here/makefile" ]; then
                make=make
                command -v gmake > /dev/null 2>&1 && make=gmake
                "$make" -C "$here" index > /dev/null
        fi
        [ -x "$here/index" ] || { problem="couldn't build EBaC's index program (make index)"; return 1; }
}

index_verses() {
        od -An -tu1 "$1/bvarray.dat" |
        $AWK '{ for (i = 1; i <= NF; i++) b[++n] = $i }
              END { print ((b[n - 7] * 256 + b[n - 6]) * 256 + b[n - 5]) * 256 + b[n - 4] }'
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

without_line() {
        [ -f "$here/trans" ] || return 0
        $AWK -v n="$1" '$1 != n' "$here/trans"
}

# Short names end in -PG (November 2026), so that a text from Project
# Gutenberg and one from GitHub (add-scrollmapper.sh's) can both be kept,
# side by side: DRC-PG and DRC. A text added before, under its name
# without -PG (BOM), is renamed: its folder, and its line in trans, in its
# place there (so EBaC starts with the same one).
rename_old_names() {
        printf '%s\n' "$KNOWN" | $AWK -F '\t' '$1 ~ /^[0-9]+$/ { print $2 }' |
        while read -r new; do
                old=${new%-PG}
                [ "$old" != "$new" ] && [ -f "$here/$old/gutenberg" ] &&
                        [ ! -e "$here/$new" ] || continue
                mv "$here/$old" "$here/$new" || continue
                if [ -f "$here/trans" ]; then
                        $AWK -v o="$old" -v n="$new" '
                                $1 == o { t = $0; sub(/^[^ \t]+[ \t]+/, "", t)
                                          printf "%-11s %s\n", n, t; next }
                                { print }' "$here/trans" > "$here/trans.new" &&
                                replace_trans "$here/trans.new"
                        rm -f "$here/trans.new"   # (before the temporary folder is made)
                fi
                printf '%s: renamed %s (from Project Gutenberg).\n' "$old" "$new"
        done
}

# remove NAME (sets problem on failure)
remove() {
        if [ ! -f "$here/$1/gutenberg" ]; then
                problem="it wasn't added from Project Gutenberg (A, then T or S, removes those from GitHub)"
                return 1
        fi
        without_line "$1" > "$tmp/trans"
        if ! $AWK '$1 != "" && substr($1, 1, 1) != "#" { f = 1 } END { exit !f }' "$tmp/trans"; then
                problem="it is the only translation in trans"
                return 1
        fi
        replace_trans "$tmp/trans" || { problem="couldn't change trans"; return 1; }
        rm -rf "$here/$1" || { problem="couldn't delete the folder $1"; return 1; }
        printf '%s: removed.\n' "$1"
}

# agree NUMBER: show Gutenberg's license, from the file's header, and ask
agree() {
        printf '\n%s: %s\n' "$name" "$title"
        [ -n "$updated" ] && printf 'Last updated: %s\n' "$updated"
        printf 'License: %s\n' "$license_text"
        printf 'Details: https://www.gutenberg.org/ebooks/%s\n' "$1"
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
                read -r answer || answer=n
                case $(printf '%s\n' "$answer" | tr 'A-Z' 'a-z') in
                y|yes) agreed="typed y"; return 0 ;;
                n|no) problem="its license wasn't agreed to"; return 1 ;;
                *) printf 'Please type y or n.\n' ;;
                esac
        done
}

# split_chapters FILE NUMBER FOLDER: the "chapters" format (see KNOWN), into
# FOLDER's book files, booklist and booknames. Prints "ok BOOKS VERSES" or
# "problem MESSAGE".
split_chapters() {
        known_detail "$2" > "$tmp/detail"
        tr -d '\r' < "$1" | $AWK -F '\t' -v folder="$3" "$PLAIN"'
        function fail(m) { if (!failed) print "problem " m; failed = 1; exit }
        function starts(line, s) { return substr(line, 1, length(s)) == s }
        function file_of(name,    f) { f = name; gsub(/[^A-Za-z0-9]/, "", f); return f }
        # a verse (or a row of chapter 0), as a line of its book file; a
        # blank row of chapter 0 is a verse with no text
        function put(c, v, text) {
                text = plain(text)
                if (text == "" && c > 0) return
                if (text == "")
                        print "[" c ":" v "]" > out
                else
                        print "[" c ":" v "] " text > out
                verses++
                if (c > 0 && v > 0) { counted[b]++ }
        }
        # the characters of s (UTF-8: its bytes but continuation bytes)
        function chars(s,    t) {
                t = s
                return length(t) - gsub(/[\200-\277]/, "", t)
        }
        function first_word(s) {
                sub(/^ +/, "", s)
                sub(/ .*$/, "", s)
                return s
        }
        # Chapter 0: the lines before a book'\''s chapter 1 (intro[1..nintro]),
        # as rows, each a verse: a line goes on into the next only if it
        # was wrapped there, the next line'\''s first word not fitting after
        # it within the text'\''s width (wrap); else it ends its row, as a
        # title'\''s, a name'\''s or a line of contents does. A blank line is a
        # blank row (as many as there are); those at the start and end go.
        function put_intro(    i, row, n, k, first, last) {
                n = 0
                for (i = 1; i <= nintro; i++) {
                        if (intro[i] == "") { rows[++n] = ""; continue }
                        row = intro[i]
                        while (i < nintro && intro[i + 1] != "" &&
                               chars(intro[i]) + 1 + chars(first_word(intro[i + 1])) > wrap) {
                                i++
                                row = row " " intro[i]
                        }
                        rows[++n] = row
                }
                for (first = 1; first <= n && rows[first] == ""; first++) ;
                for (last = n; last >= first && rows[last] == ""; last--) ;
                k = 0
                for (i = first; i <= last; i++)
                        put(0, ++k, rows[i])
                nintro = 0
        }
        function add_intro(line) { intro[++nintro] = line }
        function flush() {
                if (have) put(cur_c, cur_v, buf)
                have = 0; buf = ""
        }
        function add(text) {
                if (text == "") return
                buf = (buf == "") ? text : buf " " text
        }
        function start_book(k) {
                flush()
                if (out != "") close(out)
                b = k
                out = folder "/" file_of(bookname[b])
                print file_of(bookname[b]) > (folder "/booklist")
                print bookname[b] > (folder "/booknames")
                mode = "heading"; chap = 0; vnum = 0; have = 0; buf = ""; nintro = 0
        }
        # the next verse expected ("C:V", the next of the chapter, or the
        # first of the next) in line, at a word: its index, and in
        # mlen how long it is with the book name before it, if any
        function find_verse(line, c, v,    pat, i, j, from, pre, n) {
                pat = c ":" v
                from = 1
                while ((i = index(substr(line, from), pat)) > 0) {
                        j = from + i - 1
                        n = j + length(pat)
                        if ((j == 1 || substr(line, j - 1, 1) == " ") &&
                            (n > length(line) || substr(line, n, 1) == " ")) {
                                pre = bookname[b] " "
                                mstart = j; mlen = length(pat)
                                if (j > length(pre) && substr(line, j - length(pre), length(pre)) == pre &&
                                    (j - length(pre) == 1 || substr(line, j - length(pre) - 1, 1) == " ")) {
                                        mstart = j - length(pre); mlen += length(pre)
                                }
                                return mstart
                        }
                        from = j + 1
                }
                return 0
        }
        # the rest of a line of verses: split where the next verses start
        function verses_in(line,    m, nc, nv) {
                for (;;) {
                        nc = chap; nv = vnum + 1
                        if (chap == 0) { nc = 1; nv = 1 }
                        m = find_verse(line, nc, nv)
                        if (m == 0) { add(line); return }
                        if (chap == 0) {
                                # verse 1:1: what came before it is chapter 0
                                if (m > 1) add_intro(substr(line, 1, m - 1))
                                put_intro()
                                chap = 1
                        } else
                                add(substr(line, 1, m - 1))
                        flush()
                        vnum = nv; cur_c = chap; cur_v = vnum; have = 1
                        line = substr(line, m + mlen)
                        sub(/^ +/, "", line)
                        mode = "verses"
                }
        }
        FILENAME == ARGV[1] {
                if ($1 == "book") { nb++; bookname[nb] = $2; heading[nb] = $3 }
                next
        }
        /^\*\*\* START OF / { body = 1; next }
        /^\*\*\* END OF / { body = 0; next }
        !body { next }
        {
                line = $0
                sub(/^[ \t]+/, "", line); sub(/[ \t]+$/, "", line)
                text[++lines] = line
                if (line != "") lengths[++nlengths] = chars(line)
        }
        END {
                if (failed) exit
                if (lines == 0) fail("no text between Gutenberg'\''s START and END lines")
                # The width the text was wrapped at: as long as all but the
                # longest 1 in 100 of its lines (so that a line or two
                # longer, as of a table, doesn'\''t count)
                for (i = 1; i <= nlengths; i++) count[lengths[i]]++
                for (wrap = 0; wrap < 1000; wrap++) {
                        under += count[wrap]
                        if (under >= nlengths * 0.99) break
                }
                # Where book 1 starts: its heading'\''s last line before the
                # first verse (the Contents list names it too)
                for (i = 1; i <= lines; i++)
                        if (text[i] ~ /^([0-9] [A-Za-z]+ )?1:1 /) { first_verse = i; break }
                if (!first_verse) fail("no verse 1:1 in it")
                for (i = first_verse; i >= 1; i--)
                        if (starts(text[i], heading[1])) { book1 = i; break }
                if (!book1) fail("no heading \"" heading[1] "\" before its first verse")

                # Book 1: its chapter 0 starts with the front matter
                start_book(1)
                for (i = 1; i < book1; i++)
                        add_intro(text[i])

                for (i = book1; i <= lines; i++) {
                        line = text[i]
                        if (line == "") {
                                if (mode == "heading") add_intro("")    # (a blank row)
                                continue
                        }
                        # the next book: its heading, once this one has verses
                        if (b < nb && mode == "verses" && starts(line, heading[b + 1])) {
                                start_book(b + 1)
                                add_intro(line)
                                continue
                        }
                        # a line numbering a chapter: "Alma Chapter 5"
                        if (line == bookname[b] " Chapter " (chap + 1) || (chap == 0 && line == bookname[b] " Chapter 1")) {
                                # (dropped: the chapter number is the verses'\'')
                                continue
                        }
                        if (line ~ / Chapter [0-9]+$/ && starts(line, bookname[b] " Chapter "))
                                fail("\"" line "\" out of order (in " bookname[b] " " chap ")")
                        if (mode == "heading") {
                                # its heading and summary, until verse 1:1
                                if (find_verse(line, 1, 1) == 0) { add_intro(line); continue }
                        }
                        # (a new chapter: the verse after it is C:1)
                        if (mode == "verses" && find_verse(line, chap + 1, 1) == 1 &&
                            find_verse(line, chap, vnum + 1) == 0) {
                                flush(); chap++; vnum = 0
                        }
                        verses_in(line)
                }
                flush()
                if (b != nb) fail("found " b " of its " nb " books (the next: \"" heading[b + 1] "\")")
                for (k = 1; k <= nb; k++)
                        if (!counted[k]) fail(bookname[k] " has no verses")
                print "ok " nb " " verses
        }' "$tmp/detail" -
}

# split_paragraphs FILE NUMBER FOLDER: the "paragraphs" format (see KNOWN),
# into FOLDER's book files, booklist and booknames. Prints "ok BOOKS
# VERSES" or "problem MESSAGE". Nothing in the text is left out:
#   - a book's heading and introduction (before book 1, the front matter
#     too) are its chapter 0, row by row, as in split_chapters();
#   - a chapter's heading line ("Genesis Chapter 1") starts it (dropped:
#     the chapter number is the verses'); its summary, and anything else
#     before its verse 1 (a psalm's title), are its verse 0;
#   - a verse is a paragraph "C:V. text" (or "C:V: text", "C:V text",
#     "C:V.text"; "9a:1" is chapter 9 going on): a number lower than the
#     verse before (the second half of a psalm numbered from 1 again) is
#     numbered on from it; the same number again is more of that verse,
#     on a line of its own (so the numbers after it stay as printed);
#   - a paragraph after a verse is a note on it, kept in the verse on a
#     line of its own (EBaC shows a tab in a verse as a new line), marked
#     as a note by a "-" alone after the tab (shown as "---" in the
#     margin, and not spoken by voice); one in
#     capitals (ALEPH.) is a heading of the next verse, on a line before
#     it, or of the next chapter or book if one comes first;
#   - a "text" book (an appendix) is rows only, as chapter 0.
split_paragraphs() {
        known_detail "$2" > "$tmp/detail"
        tr -d '\r' < "$1" | $AWK -F '\t' -v folder="$3" "$PLAIN"'
        function fail(m) { if (!failed) print "problem " m; failed = 1; exit }
        function starts(line, s) { return substr(line, 1, length(s)) == s }
        function file_of(name,    f) { f = name; gsub(/[^A-Za-z0-9]/, "", f); return f }
        function chars(s,    t) { t = s; return length(t) - gsub(/[\200-\277]/, "", t) }
        function first_word(s) { sub(/^ +/, "", s); sub(/ .*$/, "", s); return s }
        # a verse, or a row of chapter 0, as a line of its book file (its
        # parts already plain, joined by tabs)
        function put(c, v, text) {
                if (text == "" && c > 0) return
                print "[" c ":" v "]" (text == "" ? "" : " " text) > out
                verses++
                if (c > 0 && v > 0) counted[b]++
        }
        # (a tab on its own between spaces: EBaC and its index read words
        # between spaces, so a word is never joined to the tab)
        function part(s, p) { return (s == "") ? p : s " \t " p }
        function put_intro(    i, row, n, k, first, last) {
                n = 0
                for (i = 1; i <= nintro; i++) {
                        if (intro[i] == "") { rows[++n] = ""; continue }
                        row = intro[i]
                        while (i < nintro && intro[i + 1] != "" &&
                               chars(intro[i]) + 1 + chars(first_word(intro[i + 1])) > wrap) {
                                i++
                                row = row " " intro[i]
                        }
                        rows[++n] = row
                }
                for (first = 1; first <= n && rows[first] == ""; first++) ;
                for (last = n; last >= first && rows[last] == ""; last--) ;
                k = 0
                for (i = first; i <= last; i++)
                        put(0, ++k, plain(rows[i]))
                nintro = 0
        }
        function add_intro(line) { intro[++nintro] = line }
        function flush_verse() {
                if (have) put(chap, vnum, vb)
                have = 0; vb = ""
        }
        function start_book(k,    i, n, bits) {
                if (b > 0) {
                        flush_verse()
                        if (mode == "intro") put_intro()
                        close(out)
                }
                b = k
                out = folder "/" file_of(bookname[b])
                print file_of(bookname[b]) > (folder "/booklist")
                print bookname[b] > (folder "/booknames")
                mode = "intro"; chap = 0; vnum = 0; have = 0; vb = ""; pre = ""; nintro = 0
                if (pend != "") {       # (a heading of this book)
                        n = split(pend, bits, / \t /)
                        for (i = 1; i <= n; i++) { add_intro(bits[i]); add_intro("") }
                        pend = ""
                }
        }
        function chapter_heading(line, n) {
                return line == bookname[b] " Chapter " n || line == bookname[b] " Chapter " n "."
        }
        function heading_like(t) {
                return t == toupper(t) && t ~ /[A-Z]/ && length(t) <= 60
        }
        # a paragraph (para[1..np]) of a book with chapters, once its
        # chapter 1 has begun
        function paragraph(    f, p, i, rest, lab, num, n, t) {
                f = para[1]
                p = para[1]
                for (i = 2; i <= np; i++) p = p " " para[i]
                np = 0
                if (b < nb && starts(f, heading[b + 1])) {
                        start_book(b + 1)
                        add_intro(f)
                        return 2        # (the rest of it: its introduction)
                }
                if (chapter_heading(f, chap + 1)) {
                        flush_verse()
                        chap++; vnum = 0; pre = pend; pend = ""; wrote0 = 0
                        rest = substr(p, length(f) + 2)
                        if (rest != "") pre = part(pre, plain(rest))
                        return 0
                }
                if (f ~ / Chapter [0-9]+\.?$/ && starts(f, bookname[b] " Chapter "))
                        fail("\"" f "\" out of order (in " bookname[b] " " chap ")")
                if (match(p, /^[0-9]+a?:[0-9]+[.:]? ?/)) {
                        lab = substr(p, 1, index(p, ":") - 1)
                        num = substr(p, index(p, ":") + 1, RLENGTH - index(p, ":"))
                        gsub(/[^0-9]/, "", num)
                        rest = substr(p, RLENGTH + 1)
                        if (lab != chap "" && lab != chap "a")
                                fail("verse " lab ":" num " in " bookname[b] " chapter " chap)
                        n = num + 0
                        if (lab == chap "" && n == vnum && have) {
                                # the same number again: more of that verse,
                                # on a line of its own (the numbers after it
                                # go on from it)
                                vb = part(vb, plain(rest)); repeated++
                                return 0
                        }
                        if (lab != chap "" || n < vnum) { n = vnum + 1; renumbered++ }
                        else if (n > vnum + 1) gaps++
                        if (vnum == 0) {
                                if (pre != "") { put(chap, 0, pre); pre = ""; wrote0 = 1 }
                                if (n > 1 && !wrote0)
                                        fail(bookname[b] " " chap " starts at verse " n ", with nothing before it")
                        }
                        flush_verse()
                        t = plain(rest)
                        vb = (pend != "") ? pend " \t " t : t
                        pend = ""; vnum = n; have = 1
                        return 0
                }
                t = plain(p)
                if (vnum == 0) { pre = part(pre, t); return 0 }
                if (heading_like(t) || pend != "") { pend = part(pend, t); notes++; return 0 }
                # (a note: "-" first, alone, so it is shown with "---"
                # in the margin and not spoken; see "Notes" in
                # readme.architecture)
                vb = part(vb, "- " t); notes++
                return 0
        }
        FILENAME == ARGV[1] {
                if ($1 == "book" || $1 == "text") { nb++; kind[nb] = $1; bookname[nb] = $2; heading[nb] = $3 }
                next
        }
        /^\*\*\* START OF / { body = 1; next }
        /^\*\*\* END OF / { body = 0; next }
        !body { next }
        {
                line = $0
                sub(/^[ \t]+/, "", line); sub(/[ \t]+$/, "", line)
                text[++lines] = line
                if (line != "") lengths[++nlengths] = chars(line)
        }
        END {
                if (failed) exit
                if (lines == 0) fail("no text between Gutenberg'\''s START and END lines")
                for (i = 1; i <= nlengths; i++) count[lengths[i]]++
                for (wrap = 0; wrap < 1000; wrap++) {
                        under += count[wrap]
                        if (under >= nlengths * 0.99) break
                }
                for (i = 1; i <= lines; i++)
                        if (text[i] == heading[1]) { book1 = i; break }
                if (!book1) fail("no heading \"" heading[1] "\"")
                start_book(1)
                for (i = 1; i < book1; i++)
                        add_intro(text[i])
                skip_intro = 0
                for (i = book1; i <= lines; i++) {
                        line = text[i]
                        if (mode == "intro") {
                                if (b < nb && kind[b] == "text" && starts(line, heading[b + 1])) {
                                        start_book(b + 1)
                                        add_intro(line)
                                        continue
                                }
                                if (kind[b] == "book" && chapter_heading(line, 1)) {
                                        put_intro()
                                        mode = "verses"
                                        np = 1; para[1] = line
                                        continue
                                }
                                add_intro(line)
                                continue
                        }
                        if (line == "") {
                                if (np) {
                                        if (paragraph() == 2) {
                                                # (a new book: the rest of its heading
                                                # paragraph is its introduction)
                                                for (k = 2; k <= saved_np; k++) add_intro(saved[k])
                                                add_intro("")
                                        }
                                }
                                continue
                        }
                        para[++np] = line
                        saved_np = np; saved[np] = line
                }
                if (np && mode == "verses") paragraph()
                if (have && pend != "") { vb = part(vb, pend); pend = "" }
                flush_verse()
                if (mode == "intro") put_intro()
                if (b != nb) fail("found " b " of its " nb " books (the next: \"" heading[b + 1] "\")")
                for (k = 1; k <= nb; k++)
                        if (kind[k] == "book" && !counted[k]) fail(bookname[k] " has no verses")
                print "notes " notes ", numbered on " renumbered ", repeated " repeated ", gaps " gaps > "/dev/stderr"
                print "ok " nb " " verses
        }' "$tmp/detail" -
}

# split_verses FILE NUMBER FOLDER: the "verses" format (see KNOWN), into
# FOLDER's book files, booklist and booknames. Prints "ok BOOKS VERSES" or
# "problem MESSAGE". Nothing in the text is left out:
#   - a book's line ("Book 01 Genesis") starts it, and names it (the line
#     itself isn't kept: its number is the book's place, its name the
#     book's);
#   - a verse, "001:001 In the beginning...", goes on in the lines under
#     it (indented), joined to it with a space, as the text wraps it;
#   - its brackets and braces are kept as they are: [words supplied], and
#     {notes} (EBaC's own marks for them; braces joined to a word end it,
#     in the index: see read_word() in index.c), and a psalm's title,
#     <<A Psalm by David.>>, at the start of its verse 1, as printed;
#   - what comes before the first book is that book's chapter 0, a row a
#     line (blank lines at its start and end left out).
# The verses must come in order, from 1:1, each book's chapters one after
# another and each chapter's verses too: anything else stops it.
split_verses() {
        tr -d '\r' < "$1" | $AWK -v folder="$3" "$PLAIN"'
        function fail(m) { if (!failed) print "problem " m; failed = 1; exit }
        function file_of(name,    f) { f = name; gsub(/[^A-Za-z0-9]/, "", f); return f }
        function put(c, v, text) {
                if (text == "")
                        print "[" c ":" v "]" > out
                else
                        print "[" c ":" v "] " text > out
                verses++
        }
        function flush() { if (have) put(chap, vnum, plain(vb)); have = 0 }
        /^\*\*\* START OF / { body = 1; next }
        /^\*\*\* END OF / { body = 0; next }
        !body { next }
        /^Book [0-9]+ / {
                flush()
                if (b > 0 && chap == 0) fail(name " has no verses")
                n = $2 + 0
                if (n != b + 1) fail("book " n " after book " b)
                b = n
                name = $0; sub(/^Book [0-9]+ +/, "", name); sub(/ +$/, "", name)
                if (out != "") close(out)
                out = folder "/" file_of(name)
                print file_of(name) > (folder "/booklist")
                print name > (folder "/booknames")
                chap = 0; vnum = 0
                if (b == 1) {           # (what came before: its chapter 0)
                        for (first = 1; first <= nfront && front[first] == ""; first++) ;
                        for (last = nfront; last >= first && front[last] == ""; last--) ;
                        for (i = first; i <= last; i++) put(0, i - first + 1, plain(front[i]))
                }
                next
        }
        b == 0 { front[++nfront] = $0; next }
        /^[0-9]+:[0-9]+ / {
                flush()
                c = substr($1, 1, index($1, ":") - 1) + 0
                v = substr($1, index($1, ":") + 1) + 0
                if (!((c == chap && v == vnum + 1) || (c == chap + 1 && v == 1)))
                        fail("verse " c ":" v " of " name " after " chap ":" vnum)
                chap = c; vnum = v
                vb = substr($0, length($1) + 2); have = 1
                next
        }
        /^ +[^ ]/ {
                if (!have) fail("a line of " name " outside a verse: " $0)
                vb = vb " " $0
                next
        }
        /^ *$/ { next }
        { fail("a line not understood in " name ": " $0) }
        END {
                if (failed) exit
                flush()
                if (b == 0) fail("no books in the text")
                if (chap == 0) fail(name " has no verses")
                print "ok " b " " verses
        }'
}

# split_sidebyside FILE NUMBER HERE: the "sidebyside" format (see KNOWN):
# each translation into its own folder, HERE/NAME, as a text of its own.
# Prints "title NAME TITLE" for each, then "ok BOOKS VERSES" (the same for
# each) or "problem MESSAGE". Nothing in the text is left out:
#   - each translation's paragraph of a verse is that verse in it (its key,
#     "Y: ", is the translation's, not kept in the verse), its lines
#     joined with a space, as the text wraps them; it may go on in more
#     paragraphs, until the next key or verse, each on a line of its own
#     (a tab before it: see dis_verse() in userint.c);
#   - a chapter's heading lines after " Chapter 2:" (its name; its verses
#     and where it was revealed) are its verse 0, in each, a line of its
#     own each (a tab between them: see dis_verse() in userint.c);
#   - what comes before the first chapter is the first book's chapter 0
#     in each, a row a paragraph: but the lines saying which translation
#     each key is go each to its own translation only (its title, too);
#     blank lines between them, one blank row;
#   - the " Chapter 2:" lines (EBaC numbers the chapters itself), and the
#     lines of dashes between the headings and the verses, aren't kept.
# A verse number must be the one after the verse before ("002.255" after
# "002.254", or the first of the next chapter): a number line written
# wrongly in the text (as "0.033" for 017.033) is taken as the next verse
# only if the next number line is the one after it. Each verse needs a
# paragraph from every translation, once.
split_sidebyside() {
        known_detail "$2" > "$tmp/detail"
        tr -d '\r' < "$1" | $AWK -F '\t' -v here="$3" "$PLAIN"'
        function fail(m) { if (!failed) print "problem " m; failed = 1; exit }
        function file_of(name,    f) { f = name; gsub(/[^A-Za-z0-9]/, "", f); return f }
        function put(k, c, v, text) {
                if (text == "")
                        print "[" c ":" v "]" > out[k]
                else
                        print "[" c ":" v "] " text > out[k]
                count[k]++
        }
        function start_book(n,    k) {
                b = n
                for (k = 1; k <= nk; k++) {
                        if (out[k] != "") close(out[k])
                        out[k] = here "/" tname[k] "/" file_of(bookname[b])
                        print file_of(bookname[b]) > (here "/" tname[k] "/booklist")
                        print bookname[b] > (here "/" tname[k] "/booknames")
                }
        }
        # the paragraph being read: of translation key (k), or a heading
        function end_para() {
                if (pk != "") {
                        if (got[pk]) fail("two of " key[pk] ": at " chap ":" vnum)
                        got[pk] = 1; text[pk] = plain_parts(pb)
                }
                pk = ""; pb = ""; brk = 0
        }
        # (each part plain, the tabs between them kept)
        function plain_parts(s,    n, i, bits, t) {
                n = split(s, bits, /\t/)
                t = plain(bits[1])
                for (i = 2; i <= n; i++) t = t " \t " plain(bits[i])
                return t
        }
        function flush_verse(    k) {
                end_para()
                if (!have) return
                for (k = 1; k <= nk; k++) {
                        if (!got[k]) fail("no " key[k] ": at " chap ":" vnum)
                        put(k, chap, vnum, text[k])
                        got[k] = 0
                }
                have = 0
        }
        function put_front(    k, i, n, row, rows, first, last, blank) {
                for (k = 1; k <= nk; k++) {
                        n = 0; row = ""; blank = 0
                        for (i = 1; i <= nfront; i++) {
                                if (front[i] == "") {
                                        if (row != "") { rows[++n] = row; row = "" }
                                        blank = 1
                                        continue
                                }
                                if (fkey[i] != "" && fkey[i] != k) continue
                                if (blank && n > 0) rows[++n] = ""
                                blank = 0
                                row = (row == "") ? front[i] : row " " front[i]
                        }
                        if (row != "") rows[++n] = row
                        for (i = 1; i <= n; i++) put(k, 0, i, plain(rows[i]))
                }
        }
        FILENAME == ARGV[1] {
                if ($1 == "translation") { nk++; key[nk] = $2; tname[nk] = $3; which[$2] = nk }
                if ($1 == "book") { nb++; bookname[nb] = $2; bookstart[nb] = $3 + 0 }
                next
        }
        /^\*\*\* START OF / { body = 1; next }
        /^\*\*\* END OF / { body = 0; next }
        !body { next }
        /^ Chapter [0-9]+:$/ {
                flush_verse()
                c = $0; gsub(/[^0-9]/, "", c); c += 0
                if (c != chap + 1) fail("chapter " c " after chapter " chap)
                if (chap == 0) {
                        if (nk == 0 || nb == 0) fail("no translations or books for it in this script")
                        start_book(1); put_front()
                } else if (b < nb && c == bookstart[b + 1])
                        start_book(b + 1)
                chap = c; vnum = 0; heading = ""; mode = "heading"
                next
        }
        /^-+$/ { next }
        chap == 0 {
                # (before the first chapter: which translation each key is)
                line = $0; sub(/^ +/, "", line); sub(/ +$/, "", line)
                nfront++; front[nfront] = line; fkey[nfront] = ""
                if (match(line, /^[^ :]+: = /)) {
                        k = which[substr(line, 1, index(line, ":") - 1)]
                        if (k) {
                                fkey[nfront] = k
                                title[k] = substr(line, RLENGTH + 1)
                        }
                }
                next
        }
        mode == "heading" && !/^[0-9.]+$/ {
                line = plain($0)
                if (line != "") heading = (heading == "") ? line : heading " \t " line
                next
        }
        /^[0-9.]+$/ {
                flush_verse()
                if (mode == "heading") {
                        for (k = 1; k <= nk; k++) put(k, chap, 0, heading)
                        mode = "verses"
                }
                if ($0 ~ /^[0-9]+\.[0-9]+$/ && length($0) == 7) {
                        c = substr($0, 1, 3) + 0; v = substr($0, 5) + 0
                        if (c != chap || v != vnum + 1)
                                fail("verse " $0 " after " chap ":" vnum)
                } else {
                        # (written wrongly: the next verse, if the next
                        # number line says so)
                        # (the check above, on that line, makes sure)
                        v = vnum + 1; slips++
                }
                vnum = v; have = 1
                next
        }
        /^[^ :]+: / && have {
                k = which[substr($0, 1, index($0, ":") - 1)]
                if (k) {
                        end_para()
                        pk = k; pb = substr($0, index($0, ":") + 2)
                        next
                }
        }
        # (a blank line: the paragraph may go on after it, a new paragraph
        # of the same translation, on a line of its own; until a key, or
        # the next verse)
        /^$/ { if (pk != "") brk = 1; next }
        {
                if (pk == "") fail("a line not understood, at " chap ":" vnum ": " $0)
                pb = pb (brk ? "\t" : " ") $0
                if (brk) paras++
                brk = 0
        }
        END {
                if (failed) exit
                flush_verse()
                if (chap == 0) fail("no chapters in the text")
                for (k = 1; k <= nk; k++) {
                        if (title[k] == "") fail("the text doesn'"'"'t say which translation " key[k] " is")
                        if (count[k] != count[1]) fail("the translations have different numbers of verses")
                        print "title\t" tname[k] "\t" plain(title[k])
                }
                print "number lines written wrongly, taken as the next verse: " slips ", paragraphs within a verse: " paras > "/dev/stderr"
                print "ok " nb " " count[1]
        }' "$tmp/detail" -
}

# add NUMBER URL: add the text, as each of its names (sets problem on
# failure)
add() {
        if [ "$name" = "$names" ]; then
                printf '%s: writing its books into the folder %s...\n' "$name" "$name"
        else
                printf 'Ebook %s: writing its translations into the folders %s...\n' "$1" "$names"
        fi
        adding=$names
        for n in $names; do
                rm -rf "$here/$n"
                mkdir "$here/$n" || { problem="couldn't make the folder $here/$n"; return 1; }
        done
        case $format in
        chapters) split_chapters "$tmp/text" "$1" "$here/$name" > "$tmp/split" ;;
        paragraphs) split_paragraphs "$tmp/text" "$1" "$here/$name" > "$tmp/split" ;;
        verses) split_verses "$tmp/text" "$1" "$here/$name" > "$tmp/split" ;;
        sidebyside) split_sidebyside "$tmp/text" "$1" "$here" > "$tmp/split" ;;
        *) echo "problem this script doesn't know the format $format" > "$tmp/split" ;;
        esac
        result=$(tail -n 1 "$tmp/split")
        case $result in
        "ok "*) ;;
        "problem "*) problem=${result#problem }; return 1 ;;
        *) problem="couldn't read the text"; return 1 ;;
        esac
        set -- $result
        total=$3
        index_program || return 1
        for n in $names; do
                folder=$here/$n
                printf '%s: building the concordance index...\n' "$n"
                (cd "$folder" && "$here/index" -y > /dev/null) || { problem="building the index of $n failed"; return 1; }
                found=$(index_verses "$folder")
                if [ "$found" != "$total" ]; then
                        problem="the index of $n has $found verses, but the text has $total"
                        return 1
                fi
                { printf 'License: %s\n' "$license_text"
                  [ -n "$terms" ] && printf 'Terms: %s\n' "$terms"
                  [ -n "$updated" ] && printf 'Updated: %s\n' "$updated"
                  printf 'Details: https://www.gutenberg.org/ebooks/%s\n' "$number"
                  printf 'Agreed: %s (%s)\n' "$(date '+%Y-%m-%d')" "$agreed"
                } > "$folder/license" || { problem="couldn't write $n/license"; return 1; }
                { [ -n "$etag" ] && printf 'Version: %s\n' "$etag"
                  [ -n "$size" ] && printf 'Size: %s\n' "$size"
                  [ -n "$modified" ] && printf 'Modified: %s\n' "$modified"
                  [ -n "$updated" ] && printf 'Updated: %s\n' "$updated"
                  printf 'Checksum: %s\n' "$checksum"
                  printf 'Downloaded: %s\n' "$(date '+%Y-%m-%d')"
                  printf 'Layout: %s\n' "$LAYOUT"
                } > "$folder/version"
                { printf 'Ebook: %s\n' "$number"
                  printf 'Address: %s\n' "$url"
                } > "$folder/gutenberg" || { problem="couldn't write $n/gutenberg"; return 1; }
        done
        # (into trans, at the top, in the order of names: so the last first;
        # with --at-end, at the end, in their order, as add-scrollmapper.sh
        # does for voice: EBaC's default stays the first)
        if [ "$at_end" = yes ]; then
                order=$names
        else
                order=$(printf '%s\n' $names | $AWK '{ l[NR] = $0 } END { for (i = NR; i > 0; i--) print l[i] }')
        fi
        for n in $order; do
                t=$($AWK -F '\t' -v n="$n" '$1 == "title" && $2 == n { print $3 }' "$tmp/split")
                [ -n "$t" ] || t=$title
                if [ "$at_end" = yes ]; then
                        { without_line "$n"
                          printf '%-11s %s\n' "$n" "$t"; } > "$tmp/trans"
                else
                        { printf '%-11s %s\n' "$n" "$t"
                          without_line "$n"; } > "$tmp/trans"
                fi && replace_trans "$tmp/trans"
        done
        adding=""
        for n in $names; do
                printf '%s: done. %s is ready: start EBaC and press C to choose it.\n' "$n" "$(trans_title "$n")"
        done
        return 0
}

# trans_title NAME: its title, as trans lists it
trans_title() {
        $AWK -v n="$1" '$1 == n { $1 = ""; sub(/^ +/, ""); print; exit }' "$here/trans"
}

# drop_adding: the folders of a text not wholly added (stopped, or failed)
drop_adding() {
        for n in $adding; do
                [ -d "$here/$n" ] && rm -rf "$here/$n"
        done
        adding=""
}

# The address of an ebook's plain text in UTF-8, from its number:
# Gutenberg's own form, https://www.gutenberg.org/ebooks/17.txt.utf-8
# (which leads on to the file). Made here, not looked up (Gutendex, a
# catalogue that gives it, refused the app's requests: HTTP 403).
PLAIN_TEXT=https://www.gutenberg.org/ebooks/%s.txt.utf-8
# and its page, which gives its title, its last update and its copyright
# status (a few kilobytes: read before the text is downloaded)
PAGE=https://www.gutenberg.org/ebooks/%s
GUTENBERG_LICENSE=https://www.gutenberg.org/policy/license.html

# page_facts FILE: from an ebook's page, the rows of its table of facts
# ("Title", "Last Update" or "Most Recently Updated", "Copyright Status",
# found by the words in them, whatever else is there),
# as "title", "updated" and "rights", a tab, and the value as plain text
# (its tags out, &amp; and the like read, spaces run together). Of the
# title, only the main title (Gutenberg puts the subtitle after a colon).
page_facts() {
        $AWK "$PLAIN"'
        function text(h) {
                gsub(/\r/, " ", h)
                gsub("\302\240", " ", h)      # (a non-breaking space)
                gsub(/<[Bb][Rr][^>]*>/, " : ", h)
                gsub(/<[^>]*>/, " ", h)
                gsub(/&amp;/, "\\&", h); gsub(/&quot;/, "\"", h)
                gsub(/&#39;|&#x27;|&apos;/, "\047", h)
                gsub(/&lt;/, "<", h); gsub(/&gt;/, ">", h); gsub(/&nbsp;/, " ", h)
                gsub(/[ \t]+/, " ", h)
                sub(/^ /, "", h); sub(/ $/, "", h)
                return h
        }
        { page = page " " $0 }
        END {
                n = split(page, rows, /<[Tt][Rr][ >]/)
                for (i = 2; i <= n; i++) {
                        row = rows[i]
                        if (!match(row, /<[Tt][Hh][^>]*>/)) continue
                        rest = substr(row, RSTART + RLENGTH)
                        if (!match(rest, /<\/[Tt][Hh]>/)) continue
                        label = tolower(text(substr(rest, 1, RSTART - 1)))
                        gsub(/[^a-z ]/, "", label)
                        rest = substr(rest, RSTART + RLENGTH)
                        if (!match(rest, /<[Tt][Dd][^>]*>/)) continue
                        rest = substr(rest, RSTART + RLENGTH)
                        if (match(rest, /<\/[Tt][Dd]>/))
                                rest = substr(rest, 1, RSTART - 1)
                        value = plain(text(rest))
                        if (value == "") continue
                        # (by the words in the label, not all of it:
                        # "Copyright Status", "Last Update", "Most
                        # Recently Updated", "Title")
                        if (label ~ /^ *title *$/) {
                                sub(/ ?:.*/, "", value)
                                key = "title"
                        } else if (label ~ /updated?/ && label !~ /release/)
                                key = "updated"
                        else if (label ~ /copyright/)
                                key = "rights"
                        else
                                continue
                        if (got[key]++) continue
                        print key "\t" value
                }
        }' "$1"
}

# Room on the disk. Adding stops while less than MIN_FREE_MB megabytes are
# free on the disk the translations are kept on, or on the one the text is
# downloaded to first (the temporary folder, TMPDIR or /tmp: often another
# disk, or another part of one), so that adding one after another (by a
# script, or by voice, say) can't fill either: a fixed amount, not a share
# of the disk, which would stop a large disk too soon and a small one too
# late. If the free space can't be found, adding stops too.
MIN_FREE_MB=1024
# The layout of the books this script writes from a text: raised when it
# changes, so that a text added before is added again, though Gutenberg's
# is the same version (kept as "Layout:" in its version file; none: 1).
#   2: November 5, 2026: notes marked with a "-" (see split_paragraphs())
LAYOUT=2

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

# download_text: the text, into $tmp/text (or from --file), without
# carriage returns; and in terms, the license its header gives ("This
# eBook is for the use of anyone anywhere..."). Sets problem on failure.
download_text() {
        printf '%s: downloading...\n' "$name"
        if [ -n "$from_file" ]; then
                cp "$from_file" "$tmp/text" || { problem="couldn't read $from_file"; return 1; }
        else
                fetch "$url" "$tmp/text" || return 1
        fi
        # (carriage returns out once, here: awk stops reading at what it
        # wants, and tr writing into it would then complain of a broken
        # pipe)
        tr -d '\r' < "$tmp/text" > "$tmp/text.lf" && mv "$tmp/text.lf" "$tmp/text" ||
                { problem="couldn't read the download"; return 1; }
        terms=$($AWK "$PLAIN"'
                /^This (e|E)(b|B)ook is for the use of anyone/ { on = 1 }
                on && /^[ \t]*$/ { exit }
                on { t = t " " $0 }
                END { print plain(t) }' "$tmp/text")
        terms=${terms# }
        # (the same text as the copy kept, though nothing said so before:
        # not added again)
        checksum=$(cksum < "$tmp/text" | $AWK '{ print $1 " " $2 }')
        if all_same_checksum; then
                already_have
                return 1
        fi
        return 0
}

# all_same_version: whether the copy of each of the text's names kept is
# the version offered now (see same_version): if one is missing, or older,
# the text is added again, as all of them
all_same_version() {
        for n in $names; do
                same_version "$n" || return 1
        done
        return 0
}

# all_same_checksum: whether each copy kept is of the very text downloaded
all_same_checksum() {
        for n in $names; do
                [ -f "$here/$n/gutenberg" ] &&
                        [ "$checksum" = "$(kept_field "$n" Checksum)" ] &&
                        same_layout "$n" || return 1
        done
        return 0
}

# already_have: the problem when the version offered is the one kept
already_have() {
        problem="you already have this version (downloaded $(kept_field "$name" Downloaded)). To download it again, remove it first (r and its number), then add it"
}

# add_number NUMBER: from the ebook number to the text added
add_number() {
        number=$1
        case $number in
        "" | *[!0-9]*)
                problem="not an ebook number (such as 17)"
                return 1
                ;;
        esac
        number=$(($number + 0))         # (no leading zeros)
        line=$(known "$number")
        if [ -z "$line" ]; then
                problem="ebook $number isn't one this importer knows yet (each text's layout is added by hand; see readme.architecture)"
                return 1
        fi
        names=$(printf '%s\n' "$line" | cut -f2)
        name=${names%% *}               # (the first, for what is said)
        format=$(printf '%s\n' "$line" | cut -f3)
        for n in $names; do
                if [ -d "$here/$n" ] && [ ! -f "$here/$n/gutenberg" ]; then
                        problem="a translation named $n is there already, not from Project Gutenberg"
                        return 1
                fi
        done
        url=$(printf "$PLAIN_TEXT" "$number")
        version_now=$(version_of "$url")
        etag=$(printf '%s\n' "$version_now" | cut -f1)
        size=$(printf '%s\n' "$version_now" | cut -f2)
        modified=$(printf '%s\n' "$version_now" | cut -f3)
        updated=""
        all_same_version && { already_have; return 1; }
        # Its page first: its title, last update and copyright status,
        # agreed to before the text is downloaded. (Without its page, the
        # text is downloaded first, and its license read from it.)
        title=""; license_text=""; terms=""
        page_read=no
        page_why="couldn't download its page"
        if [ -n "$from_page" ]; then
                cp "$from_page" "$tmp/page" 2> /dev/null && page_read=yes
        elif [ -z "$from_file" ]; then
                fetch "$(printf "$PAGE" "$number")" "$tmp/page" 2> /dev/null && page_read=yes
        fi
        if [ $page_read = yes ]; then
                page_facts "$tmp/page" > "$tmp/facts"
                title=$($AWK -F '\t' '$1 == "title" { print $2 }' "$tmp/facts")
                updated=$($AWK -F '\t' '$1 == "updated" { print $2 }' "$tmp/facts")
                rights=$($AWK -F '\t' '$1 == "rights" { print $2 }' "$tmp/facts")
                if [ -n "$rights" ]; then
                        license_text="$rights Project Gutenberg License ($GUTENBERG_LICENSE)"
                else
                        page_read=no    # (not as expected: as without it)
                        page_why="its page has no copyright status"
                fi
                all_same_version && { already_have; return 1; }
        fi
        if [ $page_read = yes ]; then
                [ -n "$title" ] || title="Project Gutenberg ebook $number"
                agree "$number" || return 1
                enough_room || return 1
                download_text || return 1
        else
                [ -z "$from_file" ] && [ -z "$from_page" ] &&
                        printf "%s: couldn't read its page on gutenberg.org (%s); its license is read from the text\n" "$name" "$page_why"
                enough_room || return 1
                download_text || return 1
                license_text=$terms
                [ -n "$license_text" ] || license_text="Project Gutenberg License ($GUTENBERG_LICENSE)"
                terms=""
                title=$($AWK "$PLAIN"'/^Title: / { print plain(substr($0, 8)); exit }' "$tmp/text")
                [ -n "$title" ] || title="Project Gutenberg ebook $number"
                agree "$number" || return 1
        fi
        # (the text's own title, as before, for trans)
        title=$($AWK "$PLAIN"'/^Title: / { print plain(substr($0, 8)); exit }' "$tmp/text")
        [ -n "$title" ] || title="Project Gutenberg ebook $number"
        add "$number" "$url"
}

# ---- For programs (the EBaC integration's voice, through ebac-admin), as
# add-scrollmapper.sh's: what can be added, a text's license, whether it is
# added. Only the texts in KNOWN: each needs its layout, added by hand.

# known_texts: a line per text in KNOWN: number, names, format, title,
# language
known_texts() {
        printf '%s\n' "$KNOWN" | $AWK -F '\t' '$1 ~ /^[0-9]+$/'
}

# all_ours NAMES: whether each is a text this script added
all_ours() {
        for n in $1; do
                [ -f "$here/$n/gutenberg" ] || return 1
        done
        return 0
}

# --catalog: as add-scrollmapper.sh's: "language", code, English name, own
# name, how many; then "translation", its ebook number (what --license,
# --check and adding take), language, title, 1 if added, and its short
# names (the texts' own, which the integration matches too: DRC-PG).
catalog() {
        known_texts > "$tmp/known"
        $AWK -F '\t' '{ n[$5]++ }
                END { for (l in n) print "language\t" l "\t" (l == "en" ? "English" : l) "\t" (l == "en" ? "English" : l) "\t" n[l] }' "$tmp/known"
        while IFS='	' read -r number names format title language; do
                added=0
                all_ours "$names" && added=1
                printf 'translation\t%s\t%s\t%s\t%s\t%s\n' "$number" "$language" "$title" "$added" "$names"
        done < "$tmp/known"
}

# license_facts NUMBER: its title and license, from its page (as adding
# shows them; the license from Gutenberg's own terms if the page can't be
# read), into title and license_text
license_facts() {
        title=$(printf '%s\n' "$(known "$1")" | cut -f4)
        license_text="Project Gutenberg License ($GUTENBERG_LICENSE)"
        if fetch "$(printf "$PAGE" "$1")" "$tmp/page" 2> /dev/null; then
                page_facts "$tmp/page" > "$tmp/facts"
                rights=$($AWK -F '\t' '$1 == "rights" { print $2 }' "$tmp/facts")
                [ -n "$rights" ] && license_text="$rights Project Gutenberg License ($GUTENBERG_LICENSE)"
        fi
}

# check_number NUMBER: "added" or "not added"; and, if what is kept is the
# version offered now, "same version", a tab, and when it was downloaded
check_number() {
        names=$(printf '%s\n' "$(known "$1")" | cut -f2)
        if all_ours "$names"; then
                echo added
                url=$(printf "$PLAIN_TEXT" "$1")
                version_now=$(version_of "$url")
                etag=$(printf '%s\n' "$version_now" | cut -f1)
                size=$(printf '%s\n' "$version_now" | cut -f2)
                modified=$(printf '%s\n' "$version_now" | cut -f3)
                updated=""
                all_same_version &&
                        printf 'same version\t%s\n' "$(kept_field "${names%% *}" Downloaded)"
        else
                echo "not added"
        fi
        return 0
}

usage() {
        echo "usage: sh add-gutenberg.sh [--agree] [--file PATH] [--page PATH] NUMBER | --remove NAME | --list"
        echo "Adds a text from Project Gutenberg to EBaC by its ebook number, or removes one."
        echo "For programs: --catalog, --license NUMBER, --check NUMBER, --at-end (into trans"
        echo "last), --agreed-how TEXT (with --agree: how it was agreed to)."
}

# ---- the arguments
agree_all=no
from_file=""
from_page=""
number_arg=""
remove_name=""
list_only=no
mode=""
mode_number=""
at_end=no
agreed_how=""
while [ $# -gt 0 ]; do
        case $1 in
        --agree) agree_all=yes ;;
        --catalog) mode=catalog ;;
        --license|--check)
                [ $# -ge 2 ] || { usage >&2; exit 2; }
                mode=${1#--}; mode_number=$2; shift ;;
        --at-end) at_end=yes ;;
        --agreed-how) [ $# -ge 2 ] || { usage >&2; exit 2; }; agreed_how=$2; shift ;;
        --list) list_only=yes ;;
        --file) [ $# -ge 2 ] || { usage >&2; exit 2; }; from_file=$2; shift ;;
        --page) [ $# -ge 2 ] || { usage >&2; exit 2; }; from_page=$2; shift ;;
        --remove) [ $# -ge 2 ] || { usage >&2; exit 2; }; remove_name=$2; shift ;;
        --from-ebac) [ $# -ge 3 ] || { usage >&2; exit 2; }; from_ebac=$2; start_with=$3; shift; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *) number_arg=$1 ;;
        esac
        shift
done

renamed=$(rename_old_names)
[ -n "$from_ebac" ] || { [ -n "$renamed" ] && printf '%s\n' "$renamed"; }
case $start_with in
*-PG) ;;
?*) [ -f "$here/$start_with-PG/gutenberg" ] && [ ! -e "$here/$start_with" ] &&
        start_with=$start_with-PG ;;     # (EBaC's, renamed just now)
esac
tmp=$(mktemp -d "${TMPDIR:-/tmp}/ebac-gutenberg.XXXXXX") || exit 1
trap cleanup EXIT
trap interrupted INT

if [ $list_only = yes ]; then
        ours | $AWK -F '\t' '{ print $1 ": " $2 }'
        exit 0
fi

case $mode in
catalog)
        catalog
        exit 0
        ;;
license|check)
        if [ -z "$(known "$mode_number")" ]; then
                printf 'ebook %s isn'"'"'t one this importer knows (17, 1581, 8294, 16955).\n' "$mode_number" >&2
                exit 1
        fi
        if [ $mode = license ]; then
                license_facts "$mode_number"
                printf 'Title: %s\nLicense: %s\nDetails: %s\n' "$title" "$license_text" \
                        "$(printf "$PAGE" "$mode_number")"
        else
                check_number "$mode_number"
        fi
        exit 0
        ;;
esac

if [ -n "$remove_name" ]; then
        remove "$remove_name" || { printf '%s: not removed: %s.\n' "$remove_name" "$problem"; exit 1; }
        exit 0
fi

if [ -z "$number_arg" ]; then
        # Asked for, from EBaC's P: the ebook number, or r and a number to
        # remove
        [ -t 1 ] && { clear 2> /dev/null || echo; }
        echo "Add a text from Project Gutenberg"
        echo
        [ -n "$renamed" ] && { printf '%s\n' "$renamed"; echo; }
        # (lines of 50 columns at most: a phone's screen)
        echo "Find it on gutenberg.org, check it is the one,"
        echo "and type its ebook number here: the EBook-No."
        echo "on its page. EBaC knows, for now:"
        echo "  17    the Book of Mormon (BOM-PG)"
        echo "  1581  the Douay-Rheims Bible (DRC-PG)"
        echo "  8294  the World English Bible (WEB-PG)"
        echo "  16955 the Qur'an in three translations:"
        echo "        YUSUFALI-PG, PICKTHALL-PG, SHAKIR-PG"
        ours > "$tmp/ours"
        if [ -s "$tmp/ours" ]; then
                echo
                echo "Added from Project Gutenberg:"
                $AWK -F '\t' '{ printf "%2d) %s: %s\n", NR, $1, $2 }' "$tmp/ours"
                echo
                echo "To remove one: r and its number (r1)."
        fi
        echo
        printf 'Ebook number (Enter alone: back): '
        typing_mode
        read -r answer || answer=""
        # (the letters under the digits on a phone's keyboard are digits,
        # as at EBaC's own number prompts: qwertyuiop, 1234567890; but r
        # and a number removes)
        answer=$(printf '%s\n' "$answer" | $AWK '{ sub(/^[ \t]+/, ""); sub(/[ \t]+$/, ""); print }')
        case $answer in
        [rR]*) answer=$(printf '%s\n' "$answer" | $AWK '{ print substr($0, 1, 1) }')$(printf '%s\n' "${answer#?}" | tr 'qwertyuiopQWERTYUIOP' '12345678901234567890') ;;
        *) answer=$(printf '%s\n' "$answer" | tr 'qwertyuiopQWERTYUIOP' '12345678901234567890') ;;
        esac
        case $answer in
        "")
                [ -n "$from_ebac" ] && back_to_ebac
                exit 0
                ;;
        [rR]*[!0-9]* | [rR])
                printf '%s: r and a number from the list, only (r1).\n' "$answer"
                failed=1
                ;;
        [rR][0-9]*)
                k=${answer#?}
                pick=$($AWK -F '\t' -v n="$k" 'NR == n + 0 { print $1 }' "$tmp/ours")
                if [ -z "$pick" ]; then
                        printf '%s: not in the list.\n' "$answer"
                        failed=1
                elif remove "$pick"; then
                        [ "$start_with" = "$pick" ] && start_with=""
                else
                        printf '%s: not removed: %s.\n' "$pick" "$problem"
                        failed=1
                fi
                ;;
        *)
                number_arg=$answer
                ;;
        esac
fi

if [ -n "$number_arg" ]; then
        if add_number "$number_arg"; then
                start_with=$name
        else
                drop_adding
                if [ -n "$names" ] && [ "$names" != "$name" ]; then
                        printf 'Ebook %s (%s): not added: %s.\n' "$number_arg" "$names" "$problem"
                else
                        printf '%s: not added: %s.\n' "${name:-$number_arg}" "$problem"
                fi
                failed=1
        fi
fi

if [ -n "$from_ebac" ]; then
        press_any_key
        back_to_ebac
fi
[ $failed -eq 0 ]
