#include "traffic_light_view.hpp"

#include <cmath>
#include <cstdio>

namespace traffic {

namespace {
const Color kBackground{0x10, 0x14, 0x1a};
const Color kHousing{0x26, 0x28, 0x2c};
const Color kHousingEdge{0x6a, 0x6e, 0x76};
const Color kPole{0x55, 0x58, 0x5e};
const Color kText{0xe6, 0xe8, 0xec};
const Color kMuted{0x9a, 0xa0, 0xa8};
const Color kLampColor[3] = {{0xff, 0x2d, 0x2d}, {0xff, 0xd2, 0x1f}, {0x2d, 0xe0, 0x5a}};

Color stateColor(LightState s) {
    switch (s) {
        case LightState::RED:    return kLampColor[0];
        case LightState::YELLOW: return kLampColor[1];
        case LightState::GREEN:  return kLampColor[2];
        default:                 return kMuted;
    }
}
}  // namespace

void TrafficLightView::drawHousing() {
    using namespace layout;
    r_.drawRectangle(t_.sx(-kHousingHalfW), t_.sy(-kHousingHalfH), t_.len(2 * kHousingHalfW),
                     t_.len(2 * kHousingHalfH), kHousing, true);
    r_.drawRectangle(t_.sx(-kHousingHalfW), t_.sy(-kHousingHalfH), t_.len(2 * kHousingHalfW),
                     t_.len(2 * kHousingHalfH), kHousingEdge, false, std::fmax(1.0, t_.len(2)));
}

void TrafficLightView::drawPole() {
    using namespace layout;
    r_.drawRectangle(t_.sx(-kPoleHalfW), t_.sy(kPoleTop), t_.len(2 * kPoleHalfW),
                     t_.len(kPoleBottom - kPoleTop), kPole, true);
}

void TrafficLightView::drawLamp(const Lamp& lamp) {
    const double cx = t_.sx(lamp.centerX), cy = t_.sy(lamp.centerY), rad = t_.len(lamp.radius);
    r_.drawCircle(cx, cy, rad + t_.len(2), {0, 0, 0}, true);                  // bezel
    r_.drawCircle(cx, cy, rad, lamp.on ? lamp.color : dim(lamp.color), true);
    if (lamp.on)                                                                // specular glint
        r_.drawCircle(cx - rad * 0.3, cy - rad * 0.3, rad * 0.25, {255, 255, 255}, true);
}

void TrafficLightView::drawDirectionArrow(const TrafficState& s) {
    using namespace layout;
    if (s.direction == Direction::NONE || s.state == LightState::OFF) return;
    const Color c = stateColor(s.state);
    const double len = t_.len(15), thick = t_.len(4);
    if (hasLeft(s.direction))     r_.drawArrow(t_.sx(-kArrowDX), t_.sy(kArrowY), len, thick, Heading::LEFT, c);
    if (hasStraight(s.direction)) r_.drawArrow(t_.sx(0),         t_.sy(kArrowY), len, thick, Heading::UP, c);
    if (hasRight(s.direction))    r_.drawArrow(t_.sx(kArrowDX),  t_.sy(kArrowY), len, thick, Heading::RIGHT, c);
}

void TrafficLightView::drawCountdown(const TrafficState& s) {
    if (s.state == LightState::OFF) return;
    const int secs = static_cast<int>(std::ceil(s.remainingTime - 1e-9));
    r_.drawText(t_.sx(36), t_.sy(-52), std::to_string(secs < 0 ? 0 : secs), t_.len(30), stateColor(s.state));
    r_.drawText(t_.sx(36), t_.sy(-18), toString(s.state), t_.len(10), kMuted);
}

void TrafficLightView::drawTrafficLight(const TrafficState& s) {
    drawPole();
    drawHousing();
    for (int i = 0; i < 3; ++i) {                      // i: 0 red, 1 yellow, 2 green
        const LightState lampState = static_cast<LightState>(2 - i);
        drawLamp({0, layout::kLampY[i], layout::kLampRadius, kLampColor[i], s.state == lampState});
    }
    drawDirectionArrow(s);
    drawCountdown(s);
}

void TrafficLightView::drawHud(const TrafficState& s, const HudInfo& hud, double canvasH) {
    char buf[96];
    double y = 12;
    auto line = [&](const std::string& text, Color c, double size = 14) {
        r_.drawText(12, y, text, size, c);
        y += size + 6;
    };
    line("Traffic Signal", kText, 18);
    y += 4;
    line("Current:", kMuted);   line(std::string("  ") + toString(s.state), stateColor(s.state));
    line("Direction:", kMuted); line(std::string("  ") + toString(s.direction), kText);
    line("Remaining:", kMuted);
    std::snprintf(buf, sizeof buf, "  %d seconds", static_cast<int>(std::ceil(s.remainingTime - 1e-9)));
    line(buf, kText);
    line("Zoom:", kMuted);
    std::snprintf(buf, sizeof buf, "  %.2fx", t_.scale());
    line(buf, kText);
    line("Mode:", kMuted);
    line("  " + hud.mode + (hud.paused ? " (PAUSED)" : ""), kText);
    line("Output:", kMuted);
    line("  " + hud.hardware, kText);

    static const char* help[] = {"[+] Zoom In", "[-] Zoom Out", "[0] Reset zoom", "[SPACE] Pause",
                                 "[G] Green  [Y] Yellow  [R] Red", "[L] Left  [S] Straight  [D] Right",
                                 "[C] Cycle dir  [N] None  [A] All", "[M] Auto/Manual", "[ESC] Exit"};
    double hy = canvasH - 12 - 18.0 * (sizeof help / sizeof help[0]);
    for (const char* h : help) { r_.drawText(12, hy, h, 12, kMuted); hy += 18; }
}

void TrafficLightView::renderFrame(const TrafficState& s, const HudInfo& hud, double w, double h) {
    r_.clear(kBackground);
    t_.setOrigin(w / 2.0, h / 2.0 - t_.len(10));       // housing centred; pole hangs below
    drawTrafficLight(s);
    drawHud(s, hud, h);
}

}  // namespace traffic
