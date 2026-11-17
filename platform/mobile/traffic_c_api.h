/* platform/mobile/traffic_c_api.h
 *
 * Plain-C facade over the portable C++ traffic engine, for native mobile apps:
 *   Android: call through JNI (see android/TrafficEngine.kt)
 *   iOS:     import in a bridging header and call from Swift
 * Only ints/doubles/function pointers cross the boundary, so any FFI works.
 *
 * State codes: 0 GREEN, 1 YELLOW, 2 RED, 3 OFF.   Direction mask: LEFT 1 | STRAIGHT 2 | RIGHT 4.
 * Functions returning int: 1 = success, 0 = rejected (invalid input, state unchanged).
 */
#ifndef TRAFFIC_C_API_H
#define TRAFFIC_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TrafficHandle TrafficHandle;

/* Called whenever the output should change (phase change, manual change, heartbeat).
 * Use it to drive Bluetooth/USB LEDs, haptics, a widget...  `user` is passed through. */
typedef void (*TrafficOutputCallback)(int state, int directionMask, double remaining, void* user);

TrafficHandle* traffic_create(double green, double yellow, double red);   /* NULL if durations are invalid */
void           traffic_destroy(TrafficHandle* h);

void   traffic_set_output_callback(TrafficHandle* h, TrafficOutputCallback cb, void* user);
void   traffic_update(TrafficHandle* h, double deltaSeconds);             /* call from your frame/timer tick */

int    traffic_set_state(TrafficHandle* h, int state);
int    traffic_set_direction(TrafficHandle* h, int mask);
int    traffic_set_duration(TrafficHandle* h, int state, double seconds);
void   traffic_all_off(TrafficHandle* h);
void   traffic_reset(TrafficHandle* h);
void   traffic_set_paused(TrafficHandle* h, int paused);
void   traffic_set_manual(TrafficHandle* h, int manual);

int    traffic_get_state(const TrafficHandle* h);
int    traffic_get_direction(const TrafficHandle* h);
double traffic_get_remaining(const TrafficHandle* h);
double traffic_get_duration(const TrafficHandle* h);

#ifdef __cplusplus
}
#endif
#endif
