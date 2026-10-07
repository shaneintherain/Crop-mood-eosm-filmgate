FilmGate v14 - all fixes since v13 (2 files). Upload the "modules" folder over the repo (replace files).
Sits on top of v13. Not compiled or run by me - please build and test.

modules/mlv_lite/mlv_lite.c
  - A failed REC start ("LiveView stabilizing", "Raw detect error") no longer deletes the previous clip
  - INFO button "Framing" works again (list position was out of step with crop_rec.c)
  - "N frames" (recording stopped, buffer full) is red again
  - Film window heights adjusted so Recorded Size is exactly what is written
    (8mm Actual 762->764, 16mm 2.35:1 1014->1012 lines; all other formats unchanged)
  - Time-left pill: counts free memory in bytes for lossless (was about half the real time),
    smooths the compression ratio (~1 s) and uses the recent write speed (~4 s) instead of
    the whole-clip average. Only the pill's number and colour change, not the recording.
modules/crop_rec/crop_rec.c
  - fps setting no longer shares a saved name with another setting (18 fps returning after reboot)
  - Film Format (8mm vs S8, A35 Anamorphic vs A35) and the Frame choice are remembered after reboot
  - Recorded Size text matches the written size

After the first boot: if S8/8mm still starts at 18 fps, pick the rate you want once; the old
shared line in CROP.CFG is replaced the next time the config is saved.
