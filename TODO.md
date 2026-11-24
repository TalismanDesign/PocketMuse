# TODO

- [ ] Search for specific song, album, or artist in the library

## Speaker volume

- [ ] Raise `USB_STREAM` from `PM_LOG_LEVEL_ERROR` in `beginUsb()` first. It currently hides the driver's `Support volume control` lines.
- [ ] Check the log for the speaker's `Support volume control` and `Support mute control` lines.
- [ ] Ask the speaker for its volume range (GET_MIN, GET_MAX and GET_RES) instead of using the fixed `UAC_SPK_VOLUME_MIN` and `UAC_SPK_VOLUME_MAX` values in `usb_stream.c`.
- [ ] Set the volume on the speaker itself when it supports it. This keeps the full sample resolution, so the bass stays stronger at low volume.
- [ ] Send the volume again after the speaker is unplugged and plugged back in.
- [ ] Keep software volume for speakers without volume control, and add dither to it.
