# PocketMuse

Music player app for the PocketMage PDA. Streams MP3, (AAC and WAV soon)
from SD card over I2S single-pin PDM output through the piezo buzzer GPIO.
Supports library browsing, now-playing with album art, playlist management,
shuffle, and loop modes. Runs as an OTA application slot on the PocketMage OS.

## Build

PocketMuse is an ELF app built against the PocketMage SDK.
Build it with the `pm` CLI from the SDK checkout:

```sh
pm build --pack
```

That compiles `src/`, links `build/PocketMuse.app.elf` plus the
`build/PocketMuse.elfo` overlay, and packs both into `PocketMuse.tar` for the
OS to install into an app slot. The raw `make` recipe lives in `Makefile` and
`$(SDK_ROOT)/tools/app.mk`; it needs `SDK_ROOT` (or `PM_SDK_ROOT`) pointing at
the SDK checkout.

`mp3` files live in `/music/` at the root of the SD card; the app creates that
directory along with `/music/.muse/` on first launch.

## Key bindings

| Input        | Action                     |
|--------------|----------------------------|
| LEFT / RIGHT | Navigate list / skip track |
| UP / DOWN    | Volume (Now Playing)       |
| SPACE        | Play / pause               |
| ENTER        | Select / play              |
| P            | Cycle source (Lib/Fav/Pl)  |
| F            | Toggle favorite            |
| S            | Toggle shuffle             |
| L            | Cycle loop (None/One/All)  |
| D            | Delete from playlist       |
| B            | Back to browser            |
| ?            | Help overlay               |
| ESC / A      | Exit to PocketMage OS      |

Touch scroll on the slider controls volume in all modes.

## Dependencies

- **PlatformIO** (`espressif32` platform, Arduino framework)
- **arduino-audio-tools** - audio pipeline (I2S, source callbacks, fading)
- **arduino-libhelix** - MP3/AAC decode
- **GxEPD2** (fork:
  [`ashtf8/GxEPD2_Editable_useFastFullUpdate`](https://github.com/ashtf8/GxEPD2_Editable_useFastFullUpdate))
- **JPEGDEC** - album art decode
- **PNGdec** - album art decode
- **U8g2** - OLED display

## Project layout

```text
src/
   main.cpp             audio pipeline, app entry, setup/loop
   muse.cpp             app-wide globals
   library.cpp          SD card track scanner (/music/)
   playlist.cpp         playlist manager (favorites, .m3u)
   metacache.cpp        ID3 metadata cache on SD
   albumart.cpp         ID3v2 APIC extraction, JPEG/PNG decode, 1-bit render
   ui_player.cpp        playback control, shuffle, duration compute
   ui/
      ui_browser.cpp    library/favorites/playlist browser
      ui_nowplaying.cpp now-playing screen with album art
      ui_playlist.cpp   playlist queue view
      ui_helpers.cpp    header/footer/scrollbar/OLED/time formatting
include/                public headers
```

## License

Apache-2.0. Same as the PocketMage project.
