# PocketMuse

Music player app for the PocketMage PDA. Streams MP3, WAV, Ogg, and Opus files from SD through the OS audio SDK library, using the built-in PWM piezo or a USB Audio Class speaker/headphones.
Supports library browsing, now-playing with album art, playlist management, shuffle, and loop modes.
Runs as a dynamically loaded ELF app on PocketMage OS!

## Build

PocketMuse is an ELF app built against the PocketMage SDK.
Build it with the `pm` CLI from the SDK:

```sh
pm build --pack
```

That compiles `src/`, links `build/PocketMuse.app.elf` plus the `build/PocketMuse.elfo` overlay, and packs them into `build/PocketMuse.tar` for installation through the OS app manager. Packaging requires the SDK's `elf_overlay_relink` and `elf_overlay_emit` tools on `PATH`.

For an ELF-only clean build:

```sh
export SDK_ROOT="$(cd [**wherever sdk is**] && pwd)"
make SDK_ROOT="$SDK_ROOT" clean
make SDK_ROOT="$SDK_ROOT" elf
```

The SDK build recipe locates the installed Espressif Xtensa C++ toolchain, if installed.

`.mp3`, `.wav`, `.ogg`, and `.opus` files live in `/music/` at the root of the SD card; the app creates that directory along with `/music/.muse/` on first launch.

## Key bindings

| Input          | Action                                                         |
| -------------- | -------------------------------------------------------------- |
| LEFT / RIGHT   | Browse / pick a search result / page help                      |
| TOUCH SCROLL   | Volume; pick a search result; page help                        |
| SPACE          | Play / pause                                                   |
| ENTER          | Play selected track or selected search result                  |
| Q              | Open search from the Library                                   |
| B              | Back to the Library; switch to Now Playing                     |
| H              | Help overlay                                                   |
| P              | Cycle source in browser; show queue from Now Playing           |
| F              | Toggle favorite                                                |
| S              | Toggle shuffle                                                 |
| L              | Cycle loop (None/One/All)                                      |
| D              | Remove selected favorite / playlist entry; keep the music file |
| U              | Output: Auto / Piezo / USB                                     |
| BKSP           | Delete in the editor; close it when empty                      |
| SEL            | Clear the editor draft                                         |
| A              | Exit to PocketMage OS                                          |
| Fn+LEFT        | Back (optional alias)                                          |

Search runs on the OLED. Press `Q`, type a title, artist, or folder name, then press ENTER to open a separate results list. LEFT/RIGHT picks a result and ENTER plays that track from the full Library, so next, previous, and shuffle continue from it. `B` returns to the Library, `Q` reopens the editor, BKSP deletes, and SEL clears the draft. Nothing is written to the SD card for search.

On the help screen, LEFT/RIGHT or touch scroll switch pages. Any other key returns to the previous screen.

## Playback and storage

PocketMuse uses a USB speaker if one is connected, otherwise the piezo. Press `U` if you want to choose yourself. Forcing USB without a compatible speaker doesn't leave you with silence. It just plays through the piezo, and the USB chip in the status bar only shows up when USB is really doing the playing.

Volume and your output choice are saved as SDK settings in NVS under `PMAudio`, so they're still there next time. Favorites are a plain list of full paths. Playlists are `.m3u` files in `/music/.muse/playlists/`, one full path per line. Blank lines and `#` comments are ignored, and LF or CRLF both work. Even an empty source opens without complaining.

A track that ends on its own advances once. Loop-one only changes that automatic advance: pressing next still skips. Shuffle goes through its cycle and stops, unless loop-all is on. Editing favorites somewhere else doesn't disturb the shuffle cycle you're currently in, and if the playing file has gone missing, its entry stays at `-1`.

The SDK reads tags as the file streams past, so nothing is loaded into memory ahead of time. That covers ID3v2.3/v2.4 in MP3s, LIST/INFO in WAVs, and Vorbis comments in Ogg and Opus. Embedded JPEG or PNG, including the base64 picture blocks used by Ogg, streams from the file into the OS 1-bit image decoder. PocketMuse keeps the decoded image for the current track and discards it when the next track starts. Files without a usable cover get the fallback image.

On the OLED, the progress bar counts decoded audio frames, which keeps it lined up with what you hear. Total time comes from the file wherever it's available: MPEG headers with Xing or VBRI, WAV data chunks, or the last granule of an Ogg page. For MP3s without an exact length, the player estimates from the bitrate.

Sleep is the OS's job, driven by its `TIMEOUT` setting, and any input resets the timer whether it came from the device or an ELF app. Playback, open SD streams, a connected USB audio device, USB mode, and the OS's keep-awake flags all hold the device awake.

## License

Apache-2.0
