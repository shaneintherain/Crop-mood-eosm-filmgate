#!/bin/sh
# Runs the computer-side tests.  Needs only gcc and python3.   Usage:  sh tests/run.sh
# Nothing here touches the camera build; it only compiles copies of the pure logic.
set -e
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
CF="-std=gnu99 -Wall -Wextra -Werror -Isrc -I$OUT"
CR=modules/crop_rec/crop_rec.c

# 1. copy the real pieces out of crop_rec.c
{
  python3 tests/extract.py $CR block    "static const struct setting_range crop_settings[]"
  python3 tests/extract.py $CR line     "#define CROP_SETTINGS_VERSION"
  python3 tests/extract.py $CR function crop_settings_load
  python3 tests/extract.py $CR function slim_film_frame_get
  python3 tests/extract.py $CR function slim_film_frame_set
} > "$OUT/crop_rec_snippets.h"

# 2. structure check: the film table exists once, in src/film-formats.h only
if grep -n '"Super 8 Actual"\|"A35 Anamorphic 2x"' modules/crop_rec/crop_rec.c modules/mlv_lite/mlv_lite.c; then
  echo "FAIL: a film table is duplicated in a module"; exit 1
fi

# 3. build and run
for t in film settings ltc; do
  echo "== $t"
  gcc $CF -o "$OUT/$t" tests/${t}_tests.c -lm
  "$OUT/$t"
done
echo "ALL TESTS PASSED"
