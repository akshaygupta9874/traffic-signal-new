// rendering/traffic_light_view.hpp - turns a TrafficState into drawing calls.
// Reads state only; never computes timing (remainingTime comes from the engine).
#pragma once
#include <string>
#include "../core/traffic_state.hpp"
#include "renderer_interface.hpp"
#include "view_transform.hpp"

namespace traffic {

struct Lamp {
    double centerX, centerY, radius;   // logical units
    Color  color;                      // bright colour; dark variant derived by the view
    bool   on;
};

struct HudInfo {
    std::string mode = "AUTO";
    bool paused = false;
    std::string hardware = "simulated";
};

namespace layout {   // logical units (zoom 1.0 == 1 unit per pixel)
constexpr double kHousingHalfW = 28, kHousingHalfH = 56;
constexpr double kLampRadius = 10;
constexpr double kLampY[3] = {-40, -14, 12};      // red, yellow, green (top to bottom)
constexpr double kArrowY = 40, kArrowDX = 16;
constexpr double kPoleHalfW = 4, kPoleTop = 56, kPoleBottom = 116;
}  // namespace layout

class TrafficLightView {
public:
    TrafficLightView(Renderer& r, ViewTransform& t) : r_(r), t_(t) {}

    // Full frame: background, centred signal, HUD panel and key help.
    void renderFrame(const TrafficState& s, const HudInfo& hud, double canvasW, double canvasH);

    // Zoom-scaled parts (all use ViewTransform):
    void drawTrafficLight(const TrafficState& s);
    void drawHousing();
    void drawLamp(const Lamp& lamp);
    void drawPole();
    void drawDirectionArrow(const TrafficState& s);
    void drawCountdown(const TrafficState& s);

    // Fixed-size screen-space text panel (does not zoom, stays readable).
    void drawHud(const TrafficState& s, const HudInfo& hud, double canvasH);

    static Color dim(Color c) { return {static_cast<unsigned char>(c.r / 4), static_cast<unsigned char>(c.g / 4), static_cast<unsigned char>(c.b / 4)}; }

private:
    Renderer& r_;
    ViewTransform& t_;
};

}  // namespace traffic
