// Dependency-free tests. Build & run:  make test
// The first group proves the core works with NO graphics and NO hardware.
#include <cmath>
#include <iostream>
#include <sstream>
#include "../core/traffic_engine.hpp"
#include "../hardware/hardware_select.hpp"
#include "../hardware/i8255_backend.hpp"
#include "../hardware/serial_backend.hpp"
#include "../hardware/simulated_hardware.hpp"
#include "../rendering/svg_renderer.hpp"
#include "../rendering/traffic_light_view.hpp"

using namespace traffic;

static int g_fail = 0, g_run = 0;
#define CHECK(c) do { ++g_run; if (!(c)) { ++g_fail; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #c "\n"; } } while (0)
#define NEAR(a, b) CHECK(std::fabs((a) - (b)) < 1e-6)

static TrafficEngine fresh() { TrafficEngine e; e.initialize(); return e; }

static void test_state_machine() {
    TrafficEngine e = fresh();                       // no hardware attached at all
    CHECK(e.getState() == LightState::GREEN);
    NEAR(e.getRemainingTime(), 10.0);
    e.update(4);   NEAR(e.getRemainingTime(), 6.0);  CHECK(e.getState() == LightState::GREEN);
    e.update(6);   CHECK(e.getState() == LightState::YELLOW);  NEAR(e.getRemainingTime(), 3.0);
    e.update(3);   CHECK(e.getState() == LightState::RED);     NEAR(e.getRemainingTime(), 10.0);
    e.update(10);  CHECK(e.getState() == LightState::GREEN);   NEAR(e.state().duration, 10.0);
}

static void test_multi_phase_and_carry() {
    TrafficEngine e = fresh();
    e.update(11.5);                                  // 10 green + 1.5 into yellow
    CHECK(e.getState() == LightState::YELLOW);  NEAR(e.getRemainingTime(), 1.5);
    e.update(1.5 + 10 + 2);                          // finish yellow, whole red, 2 into green
    CHECK(e.getState() == LightState::GREEN);   NEAR(e.getRemainingTime(), 8.0);
    e.update(1e9);                                   // huge delta stays fast and valid
    CHECK(e.getRemainingTime() > 0 && e.getRemainingTime() <= 10.0);
    e.update(-5); e.update(NAN);                     // ignored
    CHECK(e.getRemainingTime() > 0);
}

static void test_countdown_values() {
    TrafficEngine e = fresh();
    for (int expect = 10; expect >= 1; --expect) {
        CHECK(static_cast<int>(std::ceil(e.getRemainingTime() - 1e-9)) == expect);
        e.update(1.0);
    }
    CHECK(e.getState() == LightState::YELLOW);
}

static void test_manual_and_pause() {
    TrafficEngine e = fresh();
    e.update(3);
    CHECK(e.setState(LightState::RED));
    CHECK(e.getState() == LightState::RED);  NEAR(e.getRemainingTime(), 10.0);   // timer restarts
    CHECK(!e.setState(static_cast<LightState>(9)));                              // invalid
    CHECK(e.getState() == LightState::RED);
    e.setPaused(true);  e.update(5);  NEAR(e.getRemainingTime(), 10.0);
    e.setPaused(false); e.setMode(Mode::MANUAL); e.update(5); NEAR(e.getRemainingTime(), 10.0);
    e.setMode(Mode::AUTO); e.update(10); CHECK(e.getState() == LightState::GREEN);  // manual change did not break the cycle
}

static void test_direction() {
    TrafficEngine e = fresh();
    CHECK(e.getDirection() == Direction::STRAIGHT);
    CHECK(e.setDirection(Direction::LEFT));
    CHECK(e.getState() == LightState::GREEN && e.getDirection() == Direction::LEFT);   // independent of colour
    e.update(10);
    CHECK(e.getState() == LightState::YELLOW && e.getDirection() == Direction::LEFT);  // survives phase change
    CHECK(!e.setDirection(static_cast<Direction>(8)));
    CHECK(e.getDirection() == Direction::LEFT);
    Direction d = Direction::NONE;
    CHECK(directionFromMask(7, d) && d == Direction::ALL);
    CHECK(!directionFromMask(8, d) && d == Direction::ALL);
    CHECK(parseDirection("straight_left", d) && d == Direction::STRAIGHT_LEFT);
    CHECK(!parseDirection("sideways", d));
    CHECK(nextDirection(Direction::ALL) == Direction::NONE);
}

static void test_reset_and_all_off() {
    SimulatedHardware hw;  hw.initialize();
    TrafficEngine e;  e.initialize();  e.setHardware(&hw);
    e.update(12);  e.setDirection(Direction::RIGHT);
    e.allOff();
    CHECK(e.getState() == LightState::OFF);
    CHECK(!hw.red() && !hw.yellow() && !hw.green() && hw.directionMask() == 0);
    e.update(100);  CHECK(e.getState() == LightState::OFF);   // OFF never advances
    e.reset();
    CHECK(e.getState() == LightState::GREEN);  NEAR(e.getRemainingTime(), 10.0);
    CHECK(hw.green() && !hw.red());
}

static void test_config() {
    TrafficConfig c;  CHECK(c.validate().empty());
    c.greenDuration = 0;   CHECK(!c.validate().empty());
    c = TrafficConfig();  c.yellowDuration = -2;  CHECK(!c.validate().empty());
    c = TrafficConfig();  c.redDuration = NAN;    CHECK(!c.validate().empty());
    c = TrafficConfig();  c.zoom = 9;             CHECK(!c.validate().empty());

    TrafficConfig bad;  bad.greenDuration = -1;
    TrafficEngine e(bad);  std::string why;
    CHECK(!e.initialize(&why) && !why.empty());                // reports the problem...
    NEAR(e.config().greenDuration, 10.0);                      // ...and falls back to defaults
    CHECK(e.getState() == LightState::GREEN);

    TrafficEngine e2 = fresh();
    CHECK(e2.setDuration(LightState::GREEN, 20));
    CHECK(!e2.setDuration(LightState::GREEN, 0) && !e2.setDuration(LightState::OFF, 5));
    e2.setState(LightState::GREEN);  NEAR(e2.getRemainingTime(), 20.0);

    std::istringstream ok("green = 15\nyellow=4 # c\nred = 12\nzoom = 1.5\ndirection = left_right\nmode = manual\n");
    TrafficConfig f;  std::string err;
    CHECK(loadConfigFromStream(ok, f, err));
    NEAR(f.greenDuration, 15);  NEAR(f.zoom, 1.5);
    CHECK(f.direction == Direction::LEFT_RIGHT && f.mode == Mode::MANUAL);
    std::istringstream b1("green = abc\n"), b2("speed = 3\n"), b3("green = 0\n"), b4("nonsense\n");
    TrafficConfig keep;
    CHECK(!loadConfigFromStream(b1, keep, err));  CHECK(!loadConfigFromStream(b2, keep, err));
    CHECK(!loadConfigFromStream(b3, keep, err));  CHECK(!loadConfigFromStream(b4, keep, err));
    NEAR(keep.greenDuration, 10);                              // untouched on failure
}

static void test_zoom() {
    ViewTransform v;
    NEAR(v.scale(), 1.0);
    v.zoomIn();   NEAR(v.scale(), 1.25);
    v.zoomOut();  v.zoomOut();  NEAR(v.scale(), 0.75);
    for (int i = 0; i < 50; ++i) { v.zoomOut(); }
    NEAR(v.scale(), kMinZoom);
    for (int i = 0; i < 50; ++i) { v.zoomIn(); }
    NEAR(v.scale(), kMaxZoom);
    v.resetZoom();  NEAR(v.scale(), 1.0);
    CHECK(!v.setScale(0.1) && !v.setScale(10) && !v.setScale(NAN));
    NEAR(v.scale(), 1.0);
    v.setOrigin(100, 50);  v.setScale(2.0);
    NEAR(v.sx(10), 120);  NEAR(v.sy(-5), 40);  NEAR(v.len(7), 14);

    // Everything scales proportionally: lamp radius in the SVG doubles with zoom.
    auto lampR = [](double zoom) {
        TrafficEngine e; e.initialize();
        SvgRenderer svg(400, 400);  ViewTransform vt;  vt.setScale(zoom);
        TrafficLightView view(svg, vt);
        view.renderFrame(e.state(), HudInfo{}, 400, 400);
        std::string s = svg.str();
        std::size_t p = s.find("r=\"", s.find("<circle"));
        return std::stod(s.substr(p + 3));            // first circle = first lamp bezel
    };
    NEAR(lampR(2.0) / lampR(1.0), 2.0);
}

static void test_8255() {
    using namespace i8255;
    Simulated8255 chip;
    I8255Backend be(chip);
    CHECK(be.initialize() && be.isAvailable());
    CHECK(chip.control == 0x80);                                  // original control word
    TrafficState s;  s.direction = Direction::LEFT;
    s.state = LightState::GREEN;   be.applyState(s);  CHECK(chip.portA == 0x01 && chip.portB == 0x01);
    s.state = LightState::YELLOW;  be.applyState(s);  CHECK(chip.portA == 0x02);
    s.state = LightState::RED;     s.direction = Direction::ALL;  be.applyState(s);
    CHECK(chip.portA == 0x04 && chip.portB == 0x07);
    be.allOff();  CHECK(chip.portA == 0 && chip.portB == 0);
    s.state = LightState::OFF;  be.applyState(s);  CHECK(chip.portA == 0 && chip.portB == 0);

    // Engine -> backend, end to end (engine never sees a port number).
    Simulated8255 chip2;  I8255Backend be2(chip2);  be2.initialize();
    TrafficEngine e = fresh();  e.setHardware(&be2);
    CHECK(chip2.portA == 0x01);
    e.update(10);  CHECK(chip2.portA == 0x02);
    e.update(3);   CHECK(chip2.portA == 0x04);
}

static void test_fallback() {
    Simulated8255 missing(/*present=*/false);
    I8255Backend real(missing);
    SimulatedHardware sim;
    std::string note;
    HardwareInterface* hw = selectHardware(&real, sim, &note);
    CHECK(hw == &sim && !note.empty());
    Simulated8255 closed(true, /*open=*/false);
    I8255Backend real2(closed);
    CHECK(selectHardware(&real2, sim, &note) == &sim);
    Simulated8255 good;  I8255Backend real3(good);
    CHECK(selectHardware(&real3, sim, &note) == &real3);
    CHECK(selectHardware(nullptr, sim, &note) == &sim);
    TrafficEngine e = fresh();  e.setHardware(hw = &sim);
    e.update(10);  CHECK(sim.yellow());                           // still runs
}

static void test_serial() {
    StringByteSink sink;
    SerialBackend sb(sink);
    CHECK(sb.initialize());
    TrafficState s;  s.state = LightState::RED;  s.direction = Direction::STRAIGHT_RIGHT;
    CHECK(SerialBackend::encode(s) == "T R 6\n");
    sb.applyState(s);
    CHECK(sink.data.find("T R 6\n") != std::string::npos);
    std::size_t before = sink.data.size();
    sb.applyState(s);                                             // duplicate within 1 s -> suppressed
    CHECK(sink.data.size() == before);
    s.state = LightState::OFF;  CHECK(SerialBackend::encode(s) == "T O 0\n");
}

int main() {
    test_state_machine();  test_multi_phase_and_carry();  test_countdown_values();
    test_manual_and_pause();  test_direction();  test_reset_and_all_off();
    test_config();  test_zoom();  test_8255();  test_fallback();  test_serial();
    std::cout << (g_run - g_fail) << "/" << g_run << " checks passed\n";
    return g_fail ? 1 : 0;
}
