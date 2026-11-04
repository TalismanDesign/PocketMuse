#include "muse.h"
#include "library.h"
#include "playlist.h"
#include "ui_helpers.h"

#include "paths.h"
#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kItemH     = 10;
static constexpr int kListTop   = 32;
static constexpr int kListBot   = 214;
static constexpr int kMaxVis    = (kListBot - kListTop) / kItemH; // 18 rows

static int g_scrollOffset = 0;

static const char* source_label() {
    switch (playlistMgr().source()) {
        case PlaySource::Library:   return "Library";
        case PlaySource::Favorites: return "Favorites";
        case PlaySource::Playlist: {
            int pi = playlistMgr().playlistIndex();
            if (pi >= 0) return playlistMgr().playlistMeta(pi).name;
            return "Playlist";
        }
    }
    return "Library";
}

static int track_count() {
    switch (playlistMgr().source()) {
        case PlaySource::Library:   return library().count();
        case PlaySource::Favorites:
        case PlaySource::Playlist:  return playlistMgr().entryCount();
    }
    return 0;
}

void browser_init() {
    g_appMode = MODE_BROWSER;
    g_selIndex = 0;
    g_scrollOffset = 0;
    g_trackCount = track_count();
    g_needsRedraw = true;
}

void browser_process_key(char ch) {
    int n = track_count();

    switch (ch) {
    case 19: // LEFT - prev track
        if (g_selIndex > 0) {
            g_selIndex--;
            if (g_selIndex < g_scrollOffset) g_scrollOffset = g_selIndex;
            g_needsRedraw = true;
        }
        break;
    case 21: // RIGHT - next track
        if (g_selIndex < n - 1) {
            g_selIndex++;
            if (g_selIndex >= g_scrollOffset + kMaxVis) g_scrollOffset = g_selIndex - kMaxVis + 1;
            g_needsRedraw = true;
        }
        break;
    case 32: case 13: // Space or Enter
        if (n > 0 && g_selIndex < n) {
            player_play_index(g_selIndex);
        }
        break;
    case 'p': case 'P':
        player_cycle_source();
        g_selIndex = 0;
        g_scrollOffset = 0;
        g_trackCount = track_count();
        g_needsRedraw = true;
        break;
    case 'f': case 'F':
        if (n > 0 && g_selIndex < n) {
            char path[256];
            if (get_track_path(g_selIndex, path, sizeof(path)) && path[0]) {
                g_isFavorite = playlistMgr().toggleFavorite(path);
            }
        }
        g_needsRedraw = true;
        break;
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
    case 'd': case 'D':
        if (playlistMgr().source() == PlaySource::Playlist && n > 0) {
            int pi = playlistMgr().playlistIndex();
            int idx = g_selIndex;
            if (idx < g_trackCount) {
                playlistMgr().removeFromPlaylist(pi, idx);
                g_trackCount = track_count();
                if (g_selIndex >= g_trackCount && g_selIndex > 0) g_selIndex--;
                g_needsRedraw = true;
            }
        }
        break;
    case 'b': case 'B':
        if (g_playState != PlayerState::Stopped) {
            g_appMode = MODE_NOWPLAYING;
            g_needsRedraw = true;
        }
        break;
    case '?':
        g_prevMode = g_appMode;
        g_appMode = MODE_HELP;
        g_needsRedraw = true;
        break;
    }
}

void browser_render() {
    pm_ui_begin_eink_screen(false);

    char hdr[64];
    snprintf(hdr, sizeof(hdr), "%s  [%d]", source_label(), g_trackCount);
    draw_header(hdr);

    int n = track_count();
    if (n == 0) {
        char hint[96];
        snprintf(hint, sizeof(hint), "Put .mp3 files in %s", paths::kMusicDir);
        pm_text(PM_TARGET_EINK, 8, 80, "No tracks found", PM_STYLE_BODY);
        pm_text(PM_TARGET_EINK, 8, 94, hint, PM_STYLE_BODY);
        draw_footer("No tracks - add .mp3 files to the music folder");
        pm_eink_refresh();
        return;
    }

    if (g_scrollOffset > n - kMaxVis) g_scrollOffset = n - kMaxVis;
    if (g_scrollOffset < 0) g_scrollOffset = 0;

    int y = kListTop;
    int end = g_scrollOffset + kMaxVis;
    if (end > n) end = n;

    int w = pm_eink_width();

    for (int i = g_scrollOffset; i < end; i++) {
        char path[256];
        char label[64];
        get_track_path(i, path, sizeof(path));
        get_display_name(path, label, sizeof(label));

        // Truncate with ellipsis to fit the available width.
        const char* ell = pm_layout_truncate_with_ellipsis(
            label, w - 24, PM_STYLE_TINY, PM_TARGET_EINK);
        char row[96];
        strncpy(row, ell, sizeof(row) - 1);
        row[sizeof(row) - 1] = '\0';

        if (i == g_selIndex) {
            // Selected row: black outline rect, text drawn on top.
            pm_eink_rect(0, y - 8, w - 1, y - 8 + kItemH, false, true);
        }
        pm_text(PM_TARGET_EINK, 8, y, row, PM_STYLE_TINY);
        y += kItemH;
    }

    draw_scrollbar(n, kMaxVis, g_scrollOffset);

    char footer[64] = {};
    if (playlistMgr().source() == PlaySource::Playlist) {
        snprintf(footer, sizeof(footer),
            "L/R:nav SPC:play P:src F:fav D:del ?:help");
    } else {
        snprintf(footer, sizeof(footer),
            "L/R:nav SPC:play P:src F:fav L:loop ?:help");
    }
    draw_footer(footer);

    pm_eink_refresh();
}
