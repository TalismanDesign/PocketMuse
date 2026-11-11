# PocketMuse

Music player app for the PocketMage PDA. Streams MP3 from SD through the OS
audio service, using the built-in PWM piezo or a USB Audio Class speaker.
Supports library browsing, now-playing with album art, playlist management,
shuffle, and loop modes. Runs as a dynamically loaded ELF app on PocketMage OS.

## Build

PocketMuse is an ELF app built against the PocketMage SDK.
Build it with the `pm` CLI from the SDK checkout:

```sh
pm build --pack
```

That compiles `src/`, links `build/PocketMuse.app.elf` plus the
`build/PocketMuse.elfo` overlay, and packs them into `build/PocketMuse.tar`
for installation through the OS app manager. Packaging requires the SDK's
`elf_overlay_relink` and `elf_overlay_emit` tools on `PATH`.

For an ELF-only clean build from this checkout:

```sh
export SDK_ROOT="$(cd ../PocketMage_PDA/Code/PocketMageOS/lib/PocketMage_SDK && pwd)"
make SDK_ROOT="$SDK_ROOT" clean
make SDK_ROOT="$SDK_ROOT" elf
```

The SDK build recipe locates the installed Espressif Xtensa C++ toolchain.
Build the matching host from `PocketMage_PDA/Code/PocketMageOS` with
`pio run -e PM_BETA`. The host must export the playback-request, image-region,
SD-stream, drawing, and activity APIs used by this app.

`mp3` files live in `/music/` at the root of the SD card; the app creates that
directory along with `/music/.muse/` on first launch.

## Key bindings

| Input        | Action                     |
|--------------|----------------------------|
| LEFT / RIGHT | Navigate list / skip track |
| UP / DOWN    | Volume (Now Playing)       |
| SPACE        | Play / pause               |
| ENTER        | Select / play              |
| P            | Cycle source in browser; show queue from Now Playing |
| F            | Toggle favorite            |
| S            | Toggle shuffle             |
| L            | Cycle loop (None/One/All)  |
| D            | Remove selected favorite / playlist entry; keep the music file |
| U            | Output: Auto / Piezo / USB  |
| B            | Switch browser / Now Playing; return from queue |
| ?            | Help overlay               |
| ESC / A      | Exit to PocketMage OS      |

Touch scroll on the slider controls volume in all modes.

## Playback and storage

- Auto selects a connected UAC speaker; Piezo forces the built-in output.
  USB falls back to piezo when no compatible speaker is available. The USB
  status chip appears only while USB is the active playback output.
- Volume and output preference are shared SDK settings in NVS (`PMAudio`). Favorites use
  an absolute-path list; playlists are `.m3u` files under
  `/music/.muse/playlists/`, with one absolute SD path per entry. Blank lines,
  comments, LF, and CRLF are supported. Empty sources remain browsable.
- Natural EOF advances once. Repeat-one affects automatic progression;
  manual next still skips. Shuffle stops after its cycle unless repeat-all
  is enabled. Favorite edits outside the active source preserve the current
  shuffle cycle; a missing playing track keeps the not-found index at `-1`.
- ID3v2.3/v2.4 tags and embedded JPEG/PNG cover regions are read by the SDK's
  streaming media service. The OS decodes covers into a 1-bit image buffer. Unsupported/missing artwork
  uses the app's fallback display.
- The OLED uses decoded PCM frame timing for elapsed playback. Total duration
  comes from MPEG headers, including Xing/VBRI when present, with a bitrate
  estimate when an exact duration is unavailable.
- Idle sleep belongs to the OS and uses its `TIMEOUT` setting. Native and ELF
  input reset the same timer. Playback, open SD streams, USB audio presence,
  USB mode, and OS timeout-inhibition flags keep the device awake.

## Architecture

PocketMuse calls the SDK C ABI; it does not link Arduino or audio/display
drivers. The host owns Espressif `esp_audio_codec`, `pwm_audio`, `usb_stream`,
JPEGDEC/PNGdec, GxEPD2, U8g2, SD access, and sleep management.

Playback queries, metadata/cover probing, image-region decoding, SD operations,
and canvas primitives are generated wrappers over native SDK services. The SDK
owns PCM gain, timing, shared audio settings, and metadata parsing. PocketMuse
owns its library, playlist, shuffle, and presentation state. The SDK retains
ABI 2 adapters for existing apps.

USB serial is the default connector role. Piezo and unattached Auto sessions
retain it; an attached accessory receives one bounded UAC probe. An unsupported
device can then be tried as a keyboard. App exit waits for audio cleanup while
its callbacks and code remain resident.

## Native tests

```sh
python3 tests/run.py
```

Requires Python 3 and a C++17 compiler with AddressSanitizer and UBSan. `CXX`
selects the compiler; `SDK_ROOT` overrides the adjacent SDK checkout. The
tests exercise playlist parsing/mutation, shuffle continuity, playback progression, source
cycling, output preference, playback request results, and idle-timer rollover
and inhibition. They use in-memory SD files and require no device.

USB enumeration/hotplug, audible output, album-art rendering, both displays,
and deep sleep still require validation on a PocketMage device.

## Project layout

```text
src/
   main.cpp             app entry, input and render loop
   muse.cpp             app-wide globals
   library.cpp          SD card track scanner (/music/)
   playlist.cpp         playlist manager (favorites, .m3u)
   ui_player.cpp        playback control, shuffle, shared media-service queries
   ui/
      ui_browser.cpp    library/favorites/playlist browser
      ui_nowplaying.cpp now-playing screen with album art
      ui_playlist.cpp   playlist queue view
      ui_helpers.cpp    header/footer/scrollbar/OLED/time formatting
include/                public headers
tests/                  native regression suites and in-memory SD support
design/                 chosen display mockups
```

## License

Apache-2.0. Same as the PocketMage project.
