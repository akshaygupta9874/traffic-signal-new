#include "traffic_config.hpp"

#include <cmath>
#include <fstream>
#include <istream>
#include <sstream>

namespace traffic {

double TrafficConfig::durationFor(LightState s) const {
    switch (s) {
        case LightState::GREEN:  return greenDuration;
        case LightState::YELLOW: return yellowDuration;
        case LightState::RED:    return redDuration;
        default:                 return 0.0;
    }
}

static bool durationOk(double d) {
    return std::isfinite(d) && d >= kMinDuration && d <= kMaxDuration;
}

std::string TrafficConfig::validate() const {
    std::ostringstream err;
    if (!durationOk(greenDuration))  err << "green duration must be " << kMinDuration << ".." << kMaxDuration << " s; ";
    if (!durationOk(yellowDuration)) err << "yellow duration must be " << kMinDuration << ".." << kMaxDuration << " s; ";
    if (!durationOk(redDuration))    err << "red duration must be " << kMinDuration << ".." << kMaxDuration << " s; ";
    if (!std::isfinite(zoom) || zoom < kMinZoom || zoom > kMaxZoom)
        err << "zoom must be " << kMinZoom << ".." << kMaxZoom << "; ";
    if (!traffic::isValid(direction)) err << "invalid direction; ";
    return err.str();
}

static std::string trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool parseNumber(const std::string& text, double& out) {
    try {
        std::size_t used = 0;
        double v = std::stod(text, &used);
        if (used != text.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool loadConfigFromStream(std::istream& in, TrafficConfig& cfg, std::string& error) {
    TrafficConfig work = cfg;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty()) continue;
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            error = "line " + std::to_string(lineNo) + ": expected key = value";
            return false;
        }
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        double num = 0;
        if (key == "green" || key == "yellow" || key == "red" || key == "zoom") {
            if (!parseNumber(val, num)) {
                error = "line " + std::to_string(lineNo) + ": '" + val + "' is not a number";
                return false;
            }
            if (key == "green") work.greenDuration = num;
            else if (key == "yellow") work.yellowDuration = num;
            else if (key == "red") work.redDuration = num;
            else work.zoom = num;
        } else if (key == "direction") {
            if (!parseDirection(val, work.direction)) {
                error = "line " + std::to_string(lineNo) + ": unknown direction '" + val + "'";
                return false;
            }
        } else if (key == "mode") {
            std::string v;
            for (char c : val) v += static_cast<char>((c >= 'a' && c <= 'z') ? c - 32 : c);
            if (v == "AUTO") work.mode = Mode::AUTO;
            else if (v == "MANUAL") work.mode = Mode::MANUAL;
            else { error = "line " + std::to_string(lineNo) + ": mode must be auto or manual"; return false; }
        } else {
            error = "line " + std::to_string(lineNo) + ": unknown key '" + key + "'";
            return false;
        }
    }
    std::string why = work.validate();
    if (!why.empty()) { error = why; return false; }
    cfg = work;
    return true;
}

bool loadConfigFile(const std::string& path, TrafficConfig& cfg, std::string& error) {
    std::ifstream f(path);
    if (!f) { error = "cannot open '" + path + "'"; return false; }
    return loadConfigFromStream(f, cfg, error);
}

}  // namespace traffic
