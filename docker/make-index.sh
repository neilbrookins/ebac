#!/bin/sh
# make-index.sh -- make ttyd's web page with EBaC's touch gestures added.
#
#   sh make-index.sh GESTURES_JS OUTPUT_HTML
#
# ttyd serves its page from inside its own program. This starts ttyd for a
# moment, saves that page, and inserts the gesture script just before
# </body>. Run while building the app image, it always matches the ttyd
# that is installed; the app then runs ttyd with --index OUTPUT_HTML.
set -e

js=$1
out=$2
port=7682
work=$(mktemp -d)

ttyd --port "$port" --interface 127.0.0.1 true > /dev/null 2>&1 &
pid=$!
i=0
until curl -fs --compressed -o "$work/ttyd.html" "http://127.0.0.1:$port/"; do
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        echo "make-index.sh: ttyd did not answer" >&2
        kill "$pid"
        exit 1
    fi
    sleep 0.2
done
kill "$pid"

if [ "$(grep -c '</body>' "$work/ttyd.html")" != 1 ]; then
    echo "make-index.sh: expected one </body> in ttyd's page" >&2
    exit 1
fi

# Put </body> at the start of its own line, then insert the script there.
sed 's#</body>#\n</body>#' "$work/ttyd.html" > "$work/split.html"
{
    echo '<script>'
    cat "$js"
    echo '</script>'
} > "$work/script.html"
awk -v script="$work/script.html" '
    /^<\/body>/ { while ((getline line < script) > 0) print line }
    { print }
' "$work/split.html" > "$out"

rm -r "$work"
echo "make-index.sh: wrote $out ($(wc -c < "$out") bytes)"
