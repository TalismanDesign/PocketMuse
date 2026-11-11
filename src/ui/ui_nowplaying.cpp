#include "muse.h"
#include "ui_helpers.h"
#include "ui_player.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kArtX = 8;
static constexpr int kArtY = 28;
static constexpr int kArtSize = 120;
static constexpr int kInfoX = 136;
static constexpr int kInfoW = 176;

static uint8_t s_artCache[(kArtSize * kArtSize) / 8];
static char s_artCachePath[256] = {};
static int s_cachedW = 0, s_cachedH = 0;

void nowplaying_init() {
  g_appMode = MODE_NOWPLAYING;
  g_needsRedraw = true;
}

void nowplaying_process_key(char ch) {
  int n = g_trackCount > 0 ? g_trackCount : 1;

  switch (ch) {
  case PM_KEY_SPACE:
  case PM_KEY_ENTER:
    if (g_playState == PlayerState::Playing) {
      music_pause();
      g_playState = PlayerState::Paused;
    } else if (g_playState == PlayerState::Paused) {
      music_resume();
      g_playState = PlayerState::Playing;
    } else {
      if (g_nowTrackIndex >= 0 && g_nowTrackIndex < n) {
        player_play_index(g_nowTrackIndex);
      }
    }
    g_needsRedraw = true;
    break;
  case PM_KEY_LEFT:
    if (n > 0)
      player_prev_track();
    g_needsRedraw = true;
    break;
  case PM_KEY_RIGHT:
    if (n > 0)
      player_next_track();
    g_needsRedraw = true;
    break;
  case PM_KEY_UP:
    player_volume_up();
    g_needsRedraw = true;
    break;
  case PM_KEY_DOWN:
    player_volume_down();
    g_needsRedraw = true;
    break;
  case 'b':
  case 'B':
    g_appMode = MODE_BROWSER;
    g_needsRedraw = true;
    break;
  case 'p':
  case 'P':
    if (g_trackCount > 0) {
      g_appMode = MODE_PLAYLIST;
      g_needsRedraw = true;
    }
    break;
  case 'f':
  case 'F':
    player_toggle_favorite();
    break;
  case 's':
  case 'S':
    player_toggle_shuffle();
    break;
  case 'l':
  case 'L':
    player_cycle_loop_mode();
    break;
  case '?':
    g_prevMode = g_appMode;
    g_appMode = MODE_HELP;
    g_needsRedraw = true;
    break;
  }
}

void nowplaying_cache_art(const char *path, const pm_media_info &info) {
  s_cachedW = 0;
  s_cachedH = 0;
  s_artCachePath[0] = '\0';

  if (!path || !path[0] || !info.coverLength)
    return;

  std::memset(s_artCache, 0, sizeof(s_artCache));

  if (pm_image_decode_region_to_1bit(path, info.coverOffset, info.coverLength, s_artCache,
                                     kArtSize, kArtSize, 0, 0, kArtSize, true)) {
    s_cachedW = kArtSize;
    s_cachedH = kArtSize;
    std::strncpy(s_artCachePath, path, sizeof(s_artCachePath) - 1);
    s_artCachePath[sizeof(s_artCachePath) - 1] = '\0';
  }
}

// Draw a small outlined chip with a centered label. Returns the x just past
// the chip so chips can be laid out left-to-right.
static int draw_chip(int x, int y, const char *label) {
  const int padX = 4;
  const int chipH = 14;
  int textW = pm_text_width(PM_TARGET_EINK, label, PM_STYLE_TINY);
  int chipW = textW + padX * 2;
  pm_eink_rect(x, y, x + chipW, y + chipH, false, true);
  pm_text(PM_TARGET_EINK, x + padX, y + chipH - 4, label, PM_STYLE_TINY);
  return x + chipW + 4;
}

void nowplaying_render() {
  pm_ui_begin_eink_screen(false);

  if (g_nowTitle[0] == '\0' && g_nowPath[0] == '\0') {
    draw_header("Now Playing");
    pm_text(PM_TARGET_EINK, 8, 80, "Nothing playing", PM_STYLE_TINY);
    pm_text(PM_TARGET_EINK, 8, 94, "Select a track from the browser",
            PM_STYLE_TINY);
    draw_footer("B:browser ?:help");
    pm_eink_refresh();
    return;
  }

  const char *hdr;
  if (g_playState == PlayerState::Playing)
    hdr = "Now Playing";
  else if (g_playState == PlayerState::Paused)
    hdr = "Paused";
  else
    hdr = "Stopped";
  draw_header(hdr);

  bool artDrawn = false;
  if (s_cachedW > 0 && s_cachedH > 0) {
    int rowBytes = (s_cachedW + 7) / 8;
    for (int py = 0; py < s_cachedH; py++) {
      for (int px = 0; px < s_cachedW; px++) {
        int byteIdx = py * rowBytes + (px >> 3);
        bool bit = (s_artCache[byteIdx] >> (7 - (px & 7))) & 1;
        pm_eink_pixel(kArtX + px, kArtY + py, bit);
      }
    }
    artDrawn = true;
  }

  if (!artDrawn) {
    pm_eink_rect(kArtX, kArtY, kArtX + kArtSize - 1, kArtY + kArtSize - 1,
                 false, true);
    pm_text(PM_TARGET_EINK, kArtX + 32, kArtY + 64, "No Art", PM_STYLE_TINY);
  }

  // Title wraps up to two lines inside the info column; drop the rest. The
  // wrapper measures against e-ink metrics, which is the target here.
  int y = kArtY + 14;
  const int kTitleLineH = 18;
  int lineCount = pm_layout_word_wrap_count(g_nowTitle, kInfoW, PM_STYLE_BODY);
  int maxTitleLines = 2;
  for (int i = 0; i < lineCount && i < maxTitleLines; i++) {
    char line[128];
    int n = pm_layout_word_wrap_get(g_nowTitle, kInfoW, PM_STYLE_BODY, i, line,
                                    sizeof(line));
    if (n < 0)
      break;
    // If the title is longer than two lines, mark the truncated second line.
    if (i == maxTitleLines - 1 && lineCount > maxTitleLines) {
      char tail[132];
      snprintf(tail, sizeof(tail), "%s...", line);
      const char *trunc = pm_layout_truncate_with_ellipsis(
          tail, kInfoW, PM_STYLE_BODY, PM_TARGET_EINK);
      pm_text(PM_TARGET_EINK, kInfoX, y, trunc, PM_STYLE_BODY);
    } else {
      pm_text(PM_TARGET_EINK, kInfoX, y, line, PM_STYLE_BODY);
    }
    y += kTitleLineH;
  }

  y += 4;
  if (g_nowArtist[0]) {
    const char *artist = pm_layout_truncate_with_ellipsis(
        g_nowArtist, kInfoW, PM_STYLE_SMALL, PM_TARGET_EINK);
    pm_text(PM_TARGET_EINK, kInfoX, y, artist, PM_STYLE_SMALL);
    y += 16;
  }
  if (g_nowAlbum[0]) {
    const char *album = pm_layout_truncate_with_ellipsis(
        g_nowAlbum, kInfoW, PM_STYLE_SMALL, PM_TARGET_EINK);
    pm_text(PM_TARGET_EINK, kInfoX, y, album, PM_STYLE_SMALL);
    y += 16;
  }

  // Transport glyph + label just under the metadata.
  y += 6;
  draw_transport_icon_eink(kInfoX, y - 10, 10, g_playState);
  const char *playStatus;
  if (g_playState == PlayerState::Playing)
    playStatus = "Playing";
  else if (g_playState == PlayerState::Paused)
    playStatus = "Paused";
  else
    playStatus = "Stopped";
  pm_text(PM_TARGET_EINK, kInfoX + 16, y, playStatus, PM_STYLE_SMALL);

  // Bottom band: source label at the left, status chips, volume at the right.
  int bandY = pm_eink_height() - 40;
  pm_eink_rect(0, bandY - 6, pm_eink_width() - 1, bandY - 6, false, true);

  char volBuf[16];
  snprintf(volBuf, sizeof(volBuf), "Vol %d%%", (pm_audio_get_volume_level() * 100 + 127) / 255);
  int volW = pm_text_width(PM_TARGET_EINK, volBuf, PM_STYLE_SMALL);
  int volX = pm_eink_width() - 8 - volW;
  const char *chips[] = {g_isFavorite ? "FAV" : nullptr,
                         g_loopMode == LoopMode::One ? "R1" :
                         g_loopMode == LoopMode::All ? "RA" : nullptr,
                         g_shuffleEnabled ? "SF" : nullptr,
                         player_sink_is_usb_active() ? "USB" : nullptr};
  int chipsWidth = 0;
  for (const char *chip : chips) {
    if (chip)
      chipsWidth += pm_text_width(PM_TARGET_EINK, chip, PM_STYLE_TINY) + 12;
  }
  int chipX = volX - 8 - chipsWidth;
  const char *source = pm_layout_truncate_with_ellipsis(
      source_label(), chipX - 16, PM_STYLE_SMALL, PM_TARGET_EINK);
  pm_text(PM_TARGET_EINK, 8, bandY + 6, source, PM_STYLE_SMALL);
  for (const char *chip : chips) {
    if (chip)
      chipX = draw_chip(chipX, bandY - 3, chip);
  }
  pm_text(PM_TARGET_EINK, volX, bandY + 6, volBuf, PM_STYLE_SMALL);

  draw_footer("SPC:pause L/R:skip U:output B:browser ?:help");

  pm_eink_refresh();
}
