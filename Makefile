CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CORE     = core/traffic_config.cpp core/traffic_engine.cpp
HW       = hardware/i8255_backend.cpp hardware/serial_backend.cpp hardware/dev_port_io.cpp
RENDER   = rendering/svg_renderer.cpp rendering/traffic_light_view.cpp

all: build/test_main build/test_c_api build/headless_demo build/render_frames

build:
	mkdir -p build

build/test_main: tests/test_main.cpp $(CORE) $(HW) $(RENDER) | build
	$(CXX) $(CXXFLAGS) -o $@ $^
build/headless_demo: examples/headless_demo.cpp $(CORE) $(HW) | build
	$(CXX) $(CXXFLAGS) -o $@ $^
build/render_frames: examples/render_frames.cpp $(CORE) $(RENDER) | build
	$(CXX) $(CXXFLAGS) -o $@ $^

build/test_c_api: tests/test_c_api.c platform/mobile/traffic_c_api.cpp $(CORE) | build
	gcc -std=c11 -Wall -Wextra -c -o build/test_c_api.o tests/test_c_api.c
	$(CXX) $(CXXFLAGS) -o $@ build/test_c_api.o platform/mobile/traffic_c_api.cpp $(CORE)

test: build/test_main build/test_c_api
	./build/test_main
	./build/test_c_api
demo: build/headless_demo
	./build/headless_demo examples/default.cfg
frames: build/render_frames
	./build/render_frames build
clean:
	rm -rf build
.PHONY: all test demo frames clean
