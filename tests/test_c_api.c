/* Pure-C test: proves the mobile adapter works from a C (FFI-style) caller. */
#include <math.h>
#include <stdio.h>
#include "../platform/mobile/traffic_c_api.h"

static int fails = 0, runs = 0;
#define CHECK(c) do { ++runs; if (!(c)) { ++fails; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static int calls = 0, lastState = -1, lastMask = -1;
static void on_out(int s, int m, double rem, void* user) { (void)rem; ++calls; lastState = s; lastMask = m; *(int*)user += 1; }

int main(void) {
    CHECK(traffic_create(0, 3, 10) == NULL);          /* invalid timing rejected */
    CHECK(traffic_create(10, -1, 10) == NULL);
    TrafficHandle* h = traffic_create(10, 3, 10);
    CHECK(h != NULL);
    int user = 0;
    traffic_set_output_callback(h, on_out, &user);
    CHECK(calls == 1 && lastState == 0);               /* initial state published */
    CHECK(traffic_get_state(h) == 0 && fabs(traffic_get_remaining(h) - 10) < 1e-9);
    traffic_update(h, 10);   CHECK(traffic_get_state(h) == 1 && lastState == 1);
    traffic_update(h, 3);    CHECK(traffic_get_state(h) == 2);
    traffic_update(h, 10);   CHECK(traffic_get_state(h) == 0);
    CHECK(traffic_set_direction(h, 1) == 1 && lastMask == 1);
    CHECK(traffic_set_direction(h, 9) == 0 && traffic_get_direction(h) == 1);
    CHECK(traffic_set_state(h, 7) == 0 && traffic_set_state(h, 2) == 1 && traffic_get_state(h) == 2);
    CHECK(traffic_set_duration(h, 0, 20) == 1 && traffic_set_duration(h, 0, 0) == 0);
    traffic_set_paused(h, 1); traffic_update(h, 5); CHECK(fabs(traffic_get_remaining(h) - 10) < 1e-9);
    traffic_set_paused(h, 0);
    traffic_all_off(h);      CHECK(traffic_get_state(h) == 3 && lastState == 3 && lastMask == 0);
    traffic_reset(h);        CHECK(traffic_get_state(h) == 0);
    CHECK(user == calls);
    traffic_destroy(h);
    traffic_destroy(NULL); traffic_update(NULL, 1.0); CHECK(traffic_get_state(NULL) == 3);
    printf("%d/%d C-API checks passed\n", runs - fails, runs);
    return fails != 0;
}
