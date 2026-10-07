Computer-side tests (not part of the camera build)

Run:   sh tests/run.sh        (needs gcc and python3)

What is checked:
 - film_tests.c      the Film Format table (shape, sizes, alignment, bottom-bar text)
 - settings_tests.c  the saved-settings table, range checks and version migrations
 - ltc_tests.c       the timecode decoder, fed with generated LTC audio

The tests copy the real code out of the camera source, so they test what is built.
They cannot test anything that needs the camera (display, recording, buttons).
