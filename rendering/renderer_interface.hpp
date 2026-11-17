// rendering/renderer_interface.hpp - drawing primitives. Backends (SVG, canvas,
// SDL, Android Canvas...) implement these; the traffic-light drawing code only
// ever calls them. All coordinates here are SCREEN pixels.
#pragma once
#include <string>
#include <vector>

namespace traffic {

struct Color { unsigned char r, g, b; };
struct Point { double x, y; };
enum class Heading { UP, LEFT, RIGHT };

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void clear(Color c) = 0;
    virtual void drawCircle(double cx, double cy, double r, Color c, bool filled = true, double lineWidth = 1.0) = 0;
    virtual void drawRectangle(double x, double y, double w, double h, Color c, bool filled = true, double lineWidth = 1.0) = 0;
    virtual void drawLine(double x1, double y1, double x2, double y2, Color c, double lineWidth = 1.0) = 0;
    virtual void drawPolygon(const std::vector<Point>& pts, Color c) = 0;
    // (x, y) is the top-left of the text box; `size` is the glyph height in pixels.
    virtual void drawText(double x, double y, const std::string& text, double size, Color c) = 0;

    // Filled arrow centred on (cx, cy). `length` tip-to-tail, `thickness` shaft width.
    // Built from drawPolygon so any backend gets clean, scalable arrows.
    virtual void drawArrow(double cx, double cy, double length, double thickness, Heading h, Color c) {
        const double hl = length * 0.5, hw = length * 0.42, half = length / 2, t = thickness / 2;
        const double uv[7][2] = {{half, 0}, {half - hl, hw}, {half - hl, t}, {-half, t},
                                 {-half, -t}, {half - hl, -t}, {half - hl, -hw}};
        std::vector<Point> pts;
        for (const auto& p : uv) {
            const double u = p[0], v = p[1];
            switch (h) {
                case Heading::UP:    pts.push_back({cx + v, cy - u}); break;
                case Heading::LEFT:  pts.push_back({cx - u, cy + v}); break;
                case Heading::RIGHT: pts.push_back({cx + u, cy + v}); break;
            }
        }
        drawPolygon(pts, c);
    }
};

}  // namespace traffic
