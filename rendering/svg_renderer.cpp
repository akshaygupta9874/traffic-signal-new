#include "svg_renderer.hpp"

#include <cstdio>

namespace traffic {

std::string SvgRenderer::rgb(Color c) {
    char b[16];
    std::snprintf(b, sizeof b, "#%02x%02x%02x", c.r, c.g, c.b);
    return b;
}

void SvgRenderer::clear(Color c) {
    body_.str("");
    count_ = 0;
    body_ << "<rect width=\"" << w_ << "\" height=\"" << h_ << "\" fill=\"" << rgb(c) << "\"/>\n";
}

void SvgRenderer::drawCircle(double cx, double cy, double r, Color c, bool filled, double lw) {
    ++count_;
    body_ << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << r << "\" ";
    if (filled) body_ << "fill=\"" << rgb(c) << "\"/>\n";
    else body_ << "fill=\"none\" stroke=\"" << rgb(c) << "\" stroke-width=\"" << lw << "\"/>\n";
}

void SvgRenderer::drawRectangle(double x, double y, double w, double h, Color c, bool filled, double lw) {
    ++count_;
    body_ << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h << "\" rx=\"" << w * 0.06 << "\" ";
    if (filled) body_ << "fill=\"" << rgb(c) << "\"/>\n";
    else body_ << "fill=\"none\" stroke=\"" << rgb(c) << "\" stroke-width=\"" << lw << "\"/>\n";
}

void SvgRenderer::drawLine(double x1, double y1, double x2, double y2, Color c, double lw) {
    ++count_;
    body_ << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2 << "\" y2=\"" << y2
          << "\" stroke=\"" << rgb(c) << "\" stroke-width=\"" << lw << "\"/>\n";
}

void SvgRenderer::drawPolygon(const std::vector<Point>& pts, Color c) {
    ++count_;
    body_ << "<polygon fill=\"" << rgb(c) << "\" points=\"";
    for (const Point& p : pts) body_ << p.x << "," << p.y << " ";
    body_ << "\"/>\n";
}

void SvgRenderer::drawText(double x, double y, const std::string& text, double size, Color c) {
    ++count_;
    std::string esc;
    for (char ch : text) {
        if (ch == '&') esc += "&amp;";
        else if (ch == '<') esc += "&lt;";
        else if (ch == '>') esc += "&gt;";
        else esc += ch;
    }
    body_ << "<text x=\"" << x << "\" y=\"" << y + size * 0.85 << "\" font-family=\"monospace\" font-size=\"" << size
          << "\" fill=\"" << rgb(c) << "\" xml:space=\"preserve\">" << esc << "</text>\n";
}

std::string SvgRenderer::str() const {
    std::ostringstream o;
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w_ << "\" height=\"" << h_
      << "\" viewBox=\"0 0 " << w_ << " " << h_ << "\">\n" << body_.str() << "</svg>\n";
    return o.str();
}

}  // namespace traffic
