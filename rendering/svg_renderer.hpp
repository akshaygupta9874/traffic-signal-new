// rendering/svg_renderer.hpp - Renderer that emits an SVG document. Useful for
// golden-image tests, documentation screenshots and headless environments.
#pragma once
#include <sstream>
#include <string>
#include "renderer_interface.hpp"

namespace traffic {

class SvgRenderer : public Renderer {
public:
    SvgRenderer(double width, double height) : w_(width), h_(height) {}
    void clear(Color c) override;
    void drawCircle(double cx, double cy, double r, Color c, bool filled, double lineWidth) override;
    void drawRectangle(double x, double y, double w, double h, Color c, bool filled, double lineWidth) override;
    void drawLine(double x1, double y1, double x2, double y2, Color c, double lineWidth) override;
    void drawPolygon(const std::vector<Point>& pts, Color c) override;
    void drawText(double x, double y, const std::string& text, double size, Color c) override;
    std::string str() const;
    int elementCount() const { return count_; }
private:
    static std::string rgb(Color c);
    double w_, h_;
    int count_ = 0;
    std::ostringstream body_;
};

}  // namespace traffic
