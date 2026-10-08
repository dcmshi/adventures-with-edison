#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "artech/context.h"

namespace edison {

// What both games' own code builds on top of the library: drawing on the
// current screen (clamped, and through screen 3 when that's the display),
// saved areas, text in the current font, and waits. Mystery at the Museums
// (MALL.EXE segment 6) and Rock and Bach (WINMAIN.EXE segment 28) have the
// same helpers.
class ArtechGame {
protected:
    explicit ArtechGame(Platform& platform) : ctx_(platform) {}

    void select(int screen) { current_ = screen; }
    int current() const { return current_; }
    // Most helpers draw on the current screen; when that's the display (1)
    // they draw into screen 3 and copy the rectangle back to 1.
    template <class Draw>
    void drawVia3(int x, int y, int w, int h, Draw draw) {
        if (current_ == 1) {
            copyArea(1, 3, x, y, w, h);
            draw(3);
            copyArea(3, 1, x, y, w, h);
        } else {
            draw(current_);
        }
    }

    void drawLogo(int x, int y, uint16_t id);                   // show_Clogo, clamped to the screen
    void drawOpaque(int x, int y, uint16_t id);                 // show_logo, clamped
    // At 1:1: each non-transparent pixel's colour shifted by `add`.
    void drawShifted(int x, int y, uint16_t id, int add);
    void drawCentred(int x, int y, uint16_t id);                // about the sprite's centre
    void drawScaledCentred(int x, int y, int scaleX, int scaleY, uint16_t id);  // 256 = 1:1
    void fill(int x, int y, int w, int h, uint8_t colour);
    // A filled polygon on the current screen; `edges` also draws its
    // outline in the same colour (the vertices included).
    void fillPolygon(const std::vector<std::pair<int, int>>& points, uint8_t colour, bool edges = false);
    // bmfill_poly: the polygon filled with a bitmap, tiled from the screen's
    // top left. (The display, screen 1, isn't allowed.)
    void fillPolygonWith(const std::vector<std::pair<int, int>>& points, uint16_t bitmap);
    // bmfill_poly as WMAIN.EXE's build of the library has it (f63_20e4 →
    // f81_0000, f81_0210, f82_02f0): the polygon cut to the clip, then the
    // bitmap stretched onto it, its rows over the polygon's rows and each
    // row over that row's span (16.16 steps).
    void fillPolygonStretched(const std::vector<std::pair<int, int>>& points, uint16_t bitmap);
    // draw_poly as the same build has it (f63_1fbf → f81_0280): the same
    // cut and spans, filled with one colour.
    // With `needArea` (Wild Science's f12_0f0b / f12_0fe8), nothing unless
    // the polygon cut to the clip still has some width and height (and, with
    // `maxPoints`, at most that many corners).
    void fillPolygonSolid(const std::vector<std::pair<int, int>>& points, uint8_t colour, bool needArea = false, size_t maxPoints = 0);
    // The library's polygon clip (Wild Science's f83_0000): the polygons
    // only fill inside it. The whole screen by default.
    void setPolygonClip(int x, int y, int w, int h) { clip_ = {x, y, x + w - 1, y + h - 1}; }
    void clearPolygonClip() { clip_ = {0, 0, Screen::kWidth - 1, Screen::kHeight - 1}; }
    void line(int x0, int y0, int x1, int y1, uint8_t colour);
    void frame(int x, int y, int w, int h, uint8_t colour);     // a rectangle's outline
    void text(int x, int y, const std::string& s, int colour);  // in font_
    void copyArea(int src, int dst, int x, int y, int w, int h) { ctx_.screens.copyArea(src, dst, x, y, w, h); }
    // The same, leaving colour 0 out (f37_0320: what's on dst shows through).
    void copyKeyed(int src, int dst, int x, int y, int w, int h) { ctx_.screens.copyArea(src, dst, x, y, w, h, true); }
    // duplicate_area: a rectangle to another position (possibly another screen).
    void duplicateArea(int src, int dst, int sx, int sy, int w, int h, int dx, int dy);
    int saveArea(int x, int y, int w, int h);                   // a handle
    // Onto the display through screen 3, or onto the current screen when
    // that isn't the display; the handle is freed. `onlyCurrent`: onto the
    // current screen alone, even the display (f37_1664).
    void restoreArea(int handle, bool onlyCurrent = false);
    void freeArea(int handle) { saved_.erase(handle); }         // discarded, not drawn
    // recolour_area: pixels of colour `from` become `to`.
    void recolour(int x, int y, int w, int h, uint8_t from, uint8_t to);

    void clearInput();                                          // drop pending clicks and keys
    bool anyInput();                                            // a click or key since the last check
    void waitCountdown(int tenths);                             // countdown 0 = n; wait for 0
    void spinCountdown(int tenths);                             // ... taking no messages (GameContext::spin)
    bool waitOrClick(int tenths);                               // ... or a click or key (true)

    GameContext ctx_;
    int current_ = 1;
    const Font* font_ = nullptr;

private:
    // WMAIN.EXE's library: the polygon cut to the clip (f83_0065) and each
    // row's span along its edges (f81_0000), from row `top`; false if
    // nothing's left.
    bool librarySpans(const std::vector<std::pair<int, int>>& points, int& top, std::vector<std::pair<int, int>>& span, bool needArea = false,
                      size_t maxPoints = 0) const;
    // Calls plot(x, y) for each pixel of the polygon's inside, on screen.
    template <class Plot>
    void scanPolygon(const std::vector<std::pair<int, int>>& points, Plot plot);

    struct SavedArea {
        int x, y, w, h;
        std::vector<uint8_t> pixels;
    };
    std::map<int, SavedArea> saved_;
    struct { int x0, y0, x1, y1; } clip_ = {0, 0, Screen::kWidth - 1, Screen::kHeight - 1};
    int nextHandle_ = 1;
};

}  // namespace edison
