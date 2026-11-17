// Renders a few frames of the signal to SVG files using the portable core + view.
//   ./build/render_frames outdir
#include <fstream>
#include <iostream>
#include "../core/traffic_engine.hpp"
#include "../rendering/svg_renderer.hpp"
#include "../rendering/traffic_light_view.hpp"

using namespace traffic;

static void frame(const std::string& path, LightState s, Direction d, double zoom, double remaining) {
    TrafficEngine eng;
    eng.initialize();
    eng.setState(s);
    eng.setDirection(d);
    eng.update(eng.state().duration - remaining);
    SvgRenderer svg(640, 480);
    ViewTransform vt;
    vt.setScale(zoom);
    TrafficLightView view(svg, vt);
    HudInfo hud;
    view.renderFrame(eng.state(), hud, 640, 480);
    std::ofstream(path) << svg.str();
    std::cout << "wrote " << path << "\n";
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    frame(dir + "/green_straight_1x.svg", LightState::GREEN, Direction::STRAIGHT, 1.0, 7);
    frame(dir + "/red_left_right_1_5x.svg", LightState::RED, Direction::LEFT_RIGHT, 1.5, 4);
    frame(dir + "/yellow_all_0_75x.svg", LightState::YELLOW, Direction::ALL, 0.75, 2);
    return 0;
}
