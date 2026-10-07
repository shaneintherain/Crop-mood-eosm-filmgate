FilmGate v17 - everything since v13, in one zip (6 files). Upload the "modules" and "src" folders
over the repo (replace files; ltc-decode.h is new). Not compiled or run on a camera - please build and test.
NEW (v17): Quick menu has a navy bottom bar like the other pages; the two rows of buttons are centred between the bars (src/menu-grid.c).

NEW: Timecode In (Movie menu, bottom of the list): OFF (camera mics) / Left channel / Right channel
  - Top bar: the audio meters are replaced by "TC" before REC, then by the SMPTE timecode while recording
    (the timecode is read from the audio the recorder is already capturing; nothing extra is captured)
  - Colours: cream = fine, orange = peak near full scale (held 1 s), blue = level very low,
    red dashes = recording but no valid timecode for 1 s. No dots/ticks in this mode.
  - Selecting a timecode mode shows a one-time notice: "Timecode shows while recording"
  - Mode OFF = nothing changes from v14.
  src/audio-common.c, src/ltc-decode.h (new, decoder), modules/mlv_snd/mlv_snd.c (one added call)

From v14 (fixes):
  - A failed REC start no longer deletes the previous clip
  - fps setting no longer shares a saved name with another setting
  - Film Format (8mm/S8, A35 Anamorphic/A35) and Frame choice are remembered after reboot
  - INFO button "Framing" works again; "N frames" pill is red again
  - Recorded Size text matches the written size (8mm Actual 762->764, 16mm 2.35:1 1014->1012 lines)
  - Time-left pill: byte-based capacity in lossless, smoothed ratio, recent write speed
