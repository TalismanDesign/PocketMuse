#include "muse.h"
#include "albumart.h"
#include "ui_helpers.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kArtX = 8;
static constexpr int kArtY = 28;
static constexpr int kArtSize = 120;
static constexpr int kInfoX = 136;
static constexpr int kInfoW = 176;

static uint8_t  s_artCache[(kArtSize * kArtSize) / 8];
static char     s_artCachePath[256] = {};
static int      s_cachedW = 0, s_cachedH = 0;

void nowplaying_init() {
    g_appMode = MODE_NOWPLAYING;
    g_needsRedraw = true;
}

void nowplaying_process_key(char ch) {
    int n = g_trackCount > 0 ? g_trackCount : 1;

    switch (ch) {
    case 32: case 13: // Space or Enter
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
    case 19: // LEFT - prev track
        if (n > 0) player_prev_track();
        g_needsRedraw = true;
        break;
    case 21: // RIGHT - next track
        if (n > 0) player_next_track();
        g_needsRedraw = true;
        break;
    case 24: // UP - volume up
        if (g_volume < 245) g_volume += 10;
        else g_volume = 255;
        g_needsRedraw = true;
        break;
    case 25: // DOWN - volume down
        if (g_volume > 10) g_volume -= 10;
        else g_volume = 0;
        g_needsRedraw = true;
        break;
    case 'b': case 'B':
        g_appMode = MODE_BROWSER;
        g_needsRedraw = true;
        break;
    case 'p': case 'P':
        if (g_trackCount > 0) {
            g_appMode = MODE_PLAYLIST;
            g_needsRedraw = true;
        }
        break;
    case 'f': case 'F': {
        if (g_nowPath[0]) {
            g_isFavorite = playlistMgr().toggleFavorite(g_nowPath);
        }
        g_needsRedraw = true;
        break;
    }
    case 's': case 'S':
        g_shuffleEnabled = !g_shuffleEnabled;
        g_needsRedraw = true;
        break;
    case 'l': case 'L': {
        switch (g_loopMode) {
            case LoopMode::None: g_loopMode = LoopMode::One; break;
            case LoopMode::One:  g_loopMode = LoopMode::All; break;
            case LoopMode::All:  g_loopMode = LoopMode::None; break;
        }
        g_needsRedraw = true;
        break;
    }
    case '?':
        g_prevMode = g_appMode;
        g_appMode = MODE_HELP;
        g_needsRedraw = true;
        break;
    }
}

void nowplaying_cache_art(const char* path) {
    s_cachedW = 0;
    s_cachedH = 0;
    s_artCachePath[0] = '\0';

    if (!path || !path[0]) return;

    AlbumArt art;
    if (!art.load(path)) return;

    if (art.render1Bit(s_artCache, kArtSize, kArtSize, 0, 0, kArtSize)) {
        s_cachedW = art.width();
        s_cachedH = art.height();
        std::strncpy(s_artCachePath, path, sizeof(s_artCachePath) - 1);
        s_artCachePath[sizeof(s_artCachePath) - 1] = '\0';
    }
}

void nowplaying_render() {
    pm_ui_begin_eink_screen(false);

    if (g_nowTitle[0] == '\0' && g_nowPath[0] == '\0') {
        draw_header("Now Playing");
        pm_text(PM_TARGET_EINK, 8, 80, "Nothing playing", PM_STYLE_TINY);
        pm_text(PM_TARGET_EINK, 8, 94, "Select a track from the browser", PM_STYLE_TINY);
        draw_footer("B:browser ?:help");
        pm_eink_refresh();
        return;
    }

    // Header
    char hdr[32];
    if (g_playState == PlayerState::Playing) {
        snprintf(hdr, sizeof(hdr), "Now Playing");
    } else if (g_playState == PlayerState::Paused) {
        snprintf(hdr, sizeof(hdr), "Paused");
    } else {
        snprintf(hdr, sizeof(hdr), "Stopped");
    }
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

    // If no art, draw a placeholder frame
    if (!artDrawn) {
        pm_eink_rect(kArtX, kArtY, kArtX + kArtSize - 1, kArtY + kArtSize - 1, false, true);
        pm_text(PM_TARGET_EINK, kArtX + 32, kArtY + 64, "No Art", PM_STYLE_TINY);
    }

    // Track info
    char buf[128];

    // Title
    truncate_text(g_nowTitle, buf, sizeof(buf));
    pm_text(PM_TARGET_EINK, kInfoX, kArtY + 18, buf, PM_STYLE_BODY);

    // Artist
    truncate_text(g_nowArtist, buf, sizeof(buf));
    if (buf[0]) {
        pm_text(PM_TARGET_EINK, kInfoX, kArtY + 40, buf, PM_STYLE_BODY);
    }

    // Album
    truncate_text(g_nowAlbum, buf, sizeof(buf));
    if (buf[0]) {
        pm_text(PM_TARGET_EINK, kInfoX, kArtY + 60, buf, PM_STYLE_BODY);
    }

    // Status line
    int statusY = kArtY + 82;
    int col = kInfoX;
    if (g_isFavorite) {
        pm_text(PM_TARGET_EINK, col, statusY, "Fav", PM_STYLE_TINY);
        col += 40;
    }
    switch (g_loopMode) {
        case LoopMode::None: break;
        case LoopMode::One:
            pm_text(PM_TARGET_EINK, col, statusY, "R1", PM_STYLE_TINY);
            col += 35;
            break;
        case LoopMode::All:
            pm_text(PM_TARGET_EINK, col, statusY, "RA", PM_STYLE_TINY);
            col += 35;
            break;
    }
    if (g_shuffleEnabled) {
        pm_text(PM_TARGET_EINK, col, statusY, "Sf", PM_STYLE_TINY);
        col += 30;
    }

    // Volume
    char volBuf[16];
    snprintf(volBuf, sizeof(volBuf), "V:%u", (g_volume * 100 + 127) / 255);
    pm_text(PM_TARGET_EINK, col, statusY, volBuf, PM_STYLE_TINY);

    // Play/pause status
    int playStatusY = statusY + 16;
    const char* playStatus;
    if (g_playState == PlayerState::Playing) {
        playStatus = "> Playing";
    } else if (g_playState == PlayerState::Paused) {
        playStatus = "|| Paused";
    } else {
        playStatus = "[] Stopped";
    }
    pm_text(PM_TARGET_EINK, kInfoX, playStatusY, playStatus, PM_STYLE_TINY);

    // Progress bar
    int barY = kArtY + kArtSize + 14;
    int barW = pm_eink_width() - 16;
    pm_eink_rect(8, barY, 8 + barW - 1, barY + 7, false, true);
    if (g_nowProgress > 0.0f && g_nowProgress <= 1.0f) {
        int filled = (int)((barW - 2) * g_nowProgress);
        if (filled > 0) pm_eink_rect(9, barY + 1, 9 + filled - 1, barY + 6, true, true);
    }

    // Time
    char timeBuf[32];
    char elapsedStr[16];
    char durationStr[16];
    uint32_t elapsed = static_cast<uint32_t>(g_nowProgress * g_nowDuration);
    format_time(elapsedStr, sizeof(elapsedStr), elapsed);
    format_time(durationStr, sizeof(durationStr), g_nowDuration);
    snprintf(timeBuf, sizeof(timeBuf), "%s / %s", elapsedStr, durationStr);
    pm_text(PM_TARGET_EINK, 8, barY + 24, timeBuf, PM_STYLE_TINY);

    // Footer
    draw_footer("SPC:pause < >:skip /\\/:vol B:browser P:plist ?:help");

    pm_eink_refresh();
}
