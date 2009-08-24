#!/bin/sh
# Run this to generate all the initial makefiles, etc.

echo "Currently this autogen.sh should be run outside the scratchbox."

echo source helper functions
if test ! -f common/gst-autogen.sh;
then
  echo There is something wrong with your source tree.
  echo You are missing common/gst-autogen.sh
  exit 1
fi
. common/gst-autogen.sh

CONFIGURE_DEF_OPT='--enable-maintainer-mode'

autogen_options $@

echo -n "+ check for build tools"
if test ! -z "$NOCHECK"; then echo " skipped"; else  echo; fi
version_check "autoconf" "$AUTOCONF autoconf autoconf-2.54 autoconf-2.53 autoconf-2.52" \
              "ftp://ftp.gnu.org/pub/gnu/autoconf/" 2 52 || DIE=1
version_check "automake" "$AUTOMAKE automake automake-1.9 automake-1.7 automake-1.6 automake-1.5" \
              "ftp://ftp.gnu.org/pub/gnu/automake/" 1 7 || DIE=1
version_check "autopoint" "autopoint" \
              "ftp://ftp.gnu.org/pub/gnu/gettext/" 0 14 || DIE=1
version_check "libtoolize" "$LIBTOOLIZE libtoolize glibtoolize" \
              "ftp://ftp.gnu.org/pub/gnu/libtool/" 1 5 0 || DIE=1
version_check "pkg-config" "" \
              "http://www.freedesktop.org/software/pkgconfig" 0 8 0 || DIE=1

die_check $DIE

autoconf_2_52d_check || DIE=1
aclocal_check || DIE=1
autoheader_check || DIE=1

die_check $DIE

toplevel_check $srcfile
toplevel_check $srcfile2

# autopoint
#    older autopoint (< 0.12) has a tendency to complain about mkinstalldirs
if test -x mkinstalldirs; then rm mkinstalldirs; fi
#    first remove patch if necessary, then run autopoint, then reapply
if test -f po/Makefile.in.in;
then
  patch -p0 -R < common/gettext.patch
fi
#tool_run "$autopoint --force"
#patch -p0 < common/gettext.patch

tool_run "$aclocal" "-I m4 -I common/m4 $ACLOCAL_FLAGS"
tool_run "$libtoolize" "--copy --force"
tool_run "$autoheader"

# touch the stamp-h.in build stamp so we don't re-run autoheader in maintainer mode -- wingo
echo timestamp > stamp-h.in 2> /dev/null

tool_run "$autoconf"
tool_run "$automake" "-a -c -Wno-portability"

# if enable exists, add an -enable option for each of the lines in that file
if test -f enable; then
  for a in `cat enable`; do
    CONFIGURE_FILE_OPT="--enable-$a"
  done
fi

# if disable exists, add an -disable option for each of the lines in that file
if test -f disable; then
  for a in `cat disable`; do
    CONFIGURE_FILE_OPT="$CONFIGURE_FILE_OPT --disable-$a"
  done
fi

echo "Now type run ./configure inside scratchbox."
