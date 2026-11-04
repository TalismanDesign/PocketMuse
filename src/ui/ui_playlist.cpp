#include "muse.h"
#include "playlist.h"
#include "ui_helpers.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kItemH   = 10;
static constexpr int kListTop = 32;
static constexpr int kListBot = 214;
static constexpr int kMaxVis  = (kListBot - kListTop) / kItemH; // 18 rows

static int g_plScroll = 0;

void playlist_process_key(char ch) {
    int n = g_trackCount;

    switch (ch) {
    case 19: // LEFT
        if (g_selIndex > 0) {
            g_selIndex--;
            if (g_selIndex < g_plScroll) g_plScroll = g_selIndex;
            g_needsRedraw = true;
        }
        break;
    case 21: // RIGHT
        if (g_selIndex < n - 1) {
            g_selIndex++;
            if (g_selIndex >= g_plScroll + kMaxVis) g_plScroll = g_selIndex - kMaxVis + 1;
            g_needsRedraw = true;
        }
        break;
    case 32: case 13: // Space or Enter
        if (n > 0 && g_selIndex < n) {
            player_play_index(g_selIndex);
        }
        break;
    case 'd': case 'D':
        if (playlistMgr().source() == PlaySource::Playlist) {
            int pi = playlistMgr().playlistIndex();
            int idx = g_selIndex;
            if (idx < g_trackCount) {
                playlistMgr().removeFromPlaylist(pi, idx);
                g_trackCount = playlistMgr().entryCount();
                if (g_selIndex >= g_trackCount && g_selIndex > 0) g_selIndex--;
                g_needsRedraw = true;
            }
        }
        break;
    case 'b': case 'B':
        if (g_playState != PlayerState::Stopped) {
            g_appMode = MODE_NOWPLAYING;
        } else {
            g_appMode = MODE_BROWSER;
        }
        g_needsRedraw = true;
        break;
    case '?':
        g_prevMode = g_appMode;
        g_appMode = MODE_HELP;
        g_needsRedraw = true;
        break;
    }
}

void playlist_render() {
    pm_ui_begin_eink_screen(false);

    const char* srcName = "";
    if (playlistMgr().source() == PlaySource::Playlist) {
        int pi = playlistMgr().playlistIndex();
        if (pi >= 0) srcName = playlistMgr().playlistMeta(pi).name;
    }

    char hdr[64];
    snprintf(hdr, sizeof(hdr), "Playlist: %s  [%d]", srcName, g_trackCount);
    draw_header(hdr);

    int n = g_trackCount;
    if (n == 0) {
        pm_text(PM_TARGET_EINK, 8, 80, "Playlist is empty", PM_STYLE_BODY);
        draw_footer("B:back ?:help");
        pm_eink_refresh();
        return;
    }

    if (g_plScroll > n - kMaxVis) g_plScroll = n - kMaxVis;
    if (g_plScroll < 0) g_plScroll = 0;

    int y = kListTop;
    int end = g_plScroll + kMaxVis;
    if (end > n) end = n;

    int w = pm_eink_width();

    for (int i = g_plScroll; i < end; i++) {
        // Track number + name
        char path[256];
        char label[56];
        get_track_path(i, path, sizeof(path));
        get_display_name(path, label, sizeof(label));

        char prefix[8];
        snprintf(prefix, sizeof(prefix), "%2d.", i + 1);
        char row[96];
        snprintf(row, sizeof(row), "%s %s", prefix, label);

        // Truncate with ellipsis to fit the available width.
        const char* ell = pm_layout_truncate_with_ellipsis(
            row, w - 30, PM_STYLE_TINY, PM_TARGET_EINK);
        strncpy(row, ell, sizeof(row) - 1);
        row[sizeof(row) - 1] = '\0';

        if (i == g_selIndex) {
            // Selected row: black outline rect, text drawn on top.
            pm_eink_rect(0, y - 8, w - 1, y - 8 + kItemH, false, true);
        }
        pm_text(PM_TARGET_EINK, 8, y, row, PM_STYLE_TINY);

        // Now playing indicator
        if (i == g_nowTrackIndex && g_playState != PlayerState::Stopped) {
            pm_text(PM_TARGET_EINK, w - 22, y, ">", PM_STYLE_TINY);
        }

        y += kItemH;
    }

    draw_scrollbar(n, kMaxVis, g_plScroll);

    char footer[64];
    if (playlistMgr().source() == PlaySource::Playlist) {
        snprintf(footer, sizeof(footer),
            "L/R:nav SPC:play D:del B:back ?:help");
    } else {
        snprintf(footer, sizeof(footer),
            "L/R:nav SPC:play B:back ?:help");
    }
    draw_footer(footer);

    pm_eink_refresh();
}
