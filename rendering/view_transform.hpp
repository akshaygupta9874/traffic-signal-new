// rendering/view_transform.hpp - logical -> screen mapping and zoom.
//   screenX = originX + logicalX * scale     screenY = originY + logicalY * scale
//   screenR = logicalR * scale
// Nothing in the drawing code contains per-zoom coordinates; only this class changes.
#pragma once
#include <cmath>
#include "../core/traffic_config.hpp"

namespace traffic {

class ViewTransform {
public:
    static constexpr double kZoomStep = 0.25;

    explicit ViewTransform(double scale = 1.0) { if (!setScale(scale)) scale_ = 1.0; }

    // Rejects NaN/inf/out-of-range values and keeps the old scale.
    bool setScale(double s) {
        if (!std::isfinite(s) || s < kMinZoom || s > kMaxZoom) return false;
        scale_ = s;
        return true;
    }
    void zoomIn()    { scale_ = clamp(scale_ + kZoomStep); }
    void zoomOut()   { scale_ = clamp(scale_ - kZoomStep); }
    void resetZoom() { scale_ = 1.0; }
    double scale() const { return scale_; }

    void setOrigin(double x, double y) { originX_ = x; originY_ = y; }
    double sx(double lx) const { return originX_ + lx * scale_; }
    double sy(double ly) const { return originY_ + ly * scale_; }
    double len(double l)  const { return l * scale_; }

private:
    static double clamp(double v) { return v < kMinZoom ? kMinZoom : (v > kMaxZoom ? kMaxZoom : v); }
    double scale_ = 1.0, originX_ = 0.0, originY_ = 0.0;
};

}  // namespace traffic
