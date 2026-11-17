// platform/mobile/android/TrafficEngine.kt
// Kotlin wrapper over traffic_c_api (built with the NDK into libtraffic.so).
// The Activity/Compose UI only reads state from here and draws it; it never decides timing.
package edu.trafficsignal

class TrafficEngine(green: Double = 10.0, yellow: Double = 3.0, red: Double = 10.0) {
    enum class Light { GREEN, YELLOW, RED, OFF }

    private var handle: Long = nativeCreate(green, yellow, red)
    init { require(handle != 0L) { "invalid timing" } }

    fun update(dtSeconds: Double) = nativeUpdate(handle, dtSeconds)
    fun setState(l: Light) = nativeSetState(handle, l.ordinal)
    fun setDirection(mask: Int) = nativeSetDirection(handle, mask)     // LEFT 1 | STRAIGHT 2 | RIGHT 4
    fun setDuration(l: Light, seconds: Double) = nativeSetDuration(handle, l.ordinal, seconds)
    fun allOff() = nativeAllOff(handle)
    fun reset() = nativeReset(handle)
    fun setPaused(p: Boolean) = nativeSetPaused(handle, p)

    val state: Light get() = Light.values()[nativeGetState(handle)]
    val directionMask: Int get() = nativeGetDirection(handle)
    val remaining: Double get() = nativeGetRemaining(handle)
    val duration: Double get() = nativeGetDuration(handle)

    fun close() { if (handle != 0L) { nativeDestroy(handle); handle = 0 } }

    private external fun nativeCreate(g: Double, y: Double, r: Double): Long
    private external fun nativeDestroy(h: Long)
    private external fun nativeUpdate(h: Long, dt: Double)
    private external fun nativeSetState(h: Long, s: Int): Boolean
    private external fun nativeSetDirection(h: Long, m: Int): Boolean
    private external fun nativeSetDuration(h: Long, s: Int, sec: Double): Boolean
    private external fun nativeAllOff(h: Long)
    private external fun nativeReset(h: Long)
    private external fun nativeSetPaused(h: Long, p: Boolean)
    private external fun nativeGetState(h: Long): Int
    private external fun nativeGetDirection(h: Long): Int
    private external fun nativeGetRemaining(h: Long): Double
    private external fun nativeGetDuration(h: Long): Double

    companion object { init { System.loadLibrary("traffic") } }
}
