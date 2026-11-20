# TODO

## WAV and OGG playback

- [ ] Play WAV files.
- [ ] Play OGG files with Vorbis or Opus audio.
- [ ] Read the title, artist, album, duration and album art from WAV and OGG files. Album art is streamed from the file, never cached.
- [ ] Give each format's decoder task its own stack size, based on measured use. MP3 uses about 3.5 KB of the current 8 KB stack.
- [ ] Remove `aac`, `m4a`, `flac` and `ts` from `typeFromPath` in `pocketmage_audio.cpp`, since they can't be played.
- [ ] Let the library scan in `src/library.cpp` pick up `.wav` and `.ogg` files as well as `.mp3`.
- [ ] Update the README and the SDK audio docs to list the supported formats.

## Speaker volume

Needs the central `pocketmage_log`, since USB serial is off while a speaker is plugged in.

- [ ] Check the log for the speaker's `Support volume control` and `Support mute control` lines.
- [ ] Ask the speaker for its volume range (GET_MIN, GET_MAX and GET_RES) instead of using the fixed `UAC_SPK_VOLUME_MIN` and `UAC_SPK_VOLUME_MAX` values in `usb_stream.c`.
- [ ] Set the volume on the speaker itself when it supports it. This keeps the full sample resolution, so the bass stays stronger at low volume.
- [ ] Send the volume again after the speaker is unplugged and plugged back in.
- [ ] Keep software volume for speakers without volume control, and add dither to it.
