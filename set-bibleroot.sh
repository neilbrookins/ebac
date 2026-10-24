#!/bin/sh
# set-bibleroot.sh -- tell EBaC where its files are, once and for all.
#
#   sh ~/ebac/set-bibleroot.sh
#
# EBaC finds its files (the program, 'trans' and the translations) through
# the BIBLEROOT environment variable. This adds the setting, for the folder
# this script is in, to the settings files the user's shell reads when a
# terminal window opens, in that shell's own syntax:
#
#   zsh          ~/.zshrc
#   bash         ~/.bashrc (terminal windows), and the file bash reads when
#                you log in: ~/.bash_profile, ~/.bash_login or ~/.profile,
#                whichever exists (a new ~/.bash_profile on a Mac, a new
#                ~/.profile elsewhere, if none does). Creating
#                ~/.bash_profile when ~/.profile exists would stop bash
#                reading ~/.profile, so it is never done.
#   ksh          ~/.profile, and ~/.kshrc (or the file $ENV names)
#   sh, dash     ~/.profile
#   csh, tcsh    ~/.tcshrc if it exists (tcsh reads it instead), else ~/.cshrc
#   fish         ~/.config/fish/config.fish
#
# The shell is the one the user logs in with, $SHELL, so it works whichever
# shell this is pasted into; this script itself runs in sh. Running it
# again replaces the setting (say, after moving the folder) instead of
# adding another. Only standard commands are used (no sed -i), so that it
# works on Linux, macOS, Solaris, WSL and Cygwin alike.

set -e

root=$(cd "$(dirname "$0")" && pwd)
shell=$(basename "${SHELL:-sh}")
case $shell in
-*) shell=${shell#-} ;;         # a login shell's name, such as -bash
esac

# set FILE LINE: make LINE the file's only BIBLEROOT line
set_in() {
        file=$1 line=$2
        mkdir -p "$(dirname "$file")"
        if [ -f "$file" ] && grep 'BIBLEROOT' "$file" > /dev/null 2>&1; then
                tmp=$file.ebac-tmp
                grep -v 'BIBLEROOT' "$file" > "$tmp" || true
                echo "$line" >> "$tmp"
                cat "$tmp" > "$file"    # keeps the file's owner and mode
                rm -f "$tmp"
        else
                echo "$line" >> "$file"
        fi
        set_files="$set_files $file"
}

posix="export BIBLEROOT=\"$root\"   # EBaC (see set-bibleroot.sh)"
set_files=""

case $shell in
zsh)
        set_in "$HOME/.zshrc" "$posix"
        ;;
bash)
        set_in "$HOME/.bashrc" "$posix"
        login=""
        for f in .bash_profile .bash_login .profile; do
                if [ -f "$HOME/$f" ]; then
                        login=$HOME/$f
                        break
                fi
        done
        if [ -z "$login" ]; then
                if [ "$(uname -s)" = Darwin ]; then
                        login=$HOME/.bash_profile   # what a Mac's Terminal reads
                else
                        login=$HOME/.profile
                fi
        fi
        set_in "$login" "$posix"
        ;;
ksh | ksh93 | mksh | pdksh)
        set_in "$HOME/.profile" "$posix"
        env_file=${ENV:-$HOME/.kshrc}
        case $env_file in
        "~/"*) env_file=$HOME/${env_file#"~/"} ;;
        esac
        set_in "$env_file" "$posix"
        ;;
csh | tcsh)
        if [ -f "$HOME/.tcshrc" ]; then
                rc=$HOME/.tcshrc
        else
                rc=$HOME/.cshrc
        fi
        set_in "$rc" "setenv BIBLEROOT \"$root\"   # EBaC (see set-bibleroot.sh)"
        ;;
fish)
        set_in "$HOME/.config/fish/config.fish" \
                "set -gx BIBLEROOT \"$root\"   # EBaC (see set-bibleroot.sh)"
        ;;
*)      # sh, dash, and anything else that reads ~/.profile
        set_in "$HOME/.profile" "$posix"
        ;;
esac

echo "BIBLEROOT is set to $root, for your shell ($shell), in:"
for f in $set_files; do
        echo "    $f"
done
echo "Close this terminal window and open a new one, then run EBaC with:"
echo "    $root/bible"
