# State machine

```
              +---------+
        +---> |  GREEN  |  duration = greenDuration  (default 10 s)
        |     +----+----+
        |          | remainingTime reaches 0
        |          v
        |     +---------+
        |     | YELLOW  |  duration = yellowDuration (default 3 s)
        |     +----+----+
        |          | remainingTime reaches 0
        |          v
        |     +---------+
        +-----+   RED   |  duration = redDuration    (default 10 s)
              +---------+

   any state --setState(s)--> s   (restarts that phase's timer)
   any state --allOff()-----> OFF (no lamps, no arrows; cycle frozen)
   OFF --setState / reset()--> GREEN/..., cycle resumes
```

## Data

| Field | Meaning |
|---|---|
| `state` | GREEN, YELLOW, RED, or OFF |
| `duration` | full length of the current phase, seconds |
| `remainingTime` | seconds left; counts 10 -> 1 in GREEN, 3 -> 1 in YELLOW, etc. (the display shows `ceil`) |
| `direction` | mask NONE/LEFT/STRAIGHT/RIGHT combinations/ALL; independent of `state` and kept across phase changes |
| output state | derived: PA0/1/2 from `state`, PB0/1/2 from `direction` (0 when OFF) |

## Operations (all platforms)

| Operation | Behaviour |
|---|---|
| `initialize()` | validate config; invalid -> defaults + error; start GREEN; publish |
| `update(dt)` | advance time; carries leftover time across phases; ignores dt <= 0 / NaN; no-op while paused, in MANUAL mode, or OFF |
| `setState(s)` | validate, restart that phase's timer, publish |
| `setDirection(d)` | validate 0..7, publish; does not touch the timer |
| `setDuration(s, sec)` | validate 1..3600 (DOS 1..99); applies next time that phase is entered |
| `allOff()` / `reset()` | OFF, or GREEN with full time, not paused, configured mode/direction |
| `setPaused`, `setMode` | freeze / switch AUTO<->MANUAL |

Manual operations go through the same `enter()` that the automatic cycle uses, so
a key press can never leave `state`, `duration` and `remainingTime` inconsistent.
Renderers and hardware backends only receive a `const TrafficState&`.

## Time source

The engine never reads a clock. The platform calls `update(dt)`:
`requestAnimationFrame` (web), a timer (mobile), `TE_Tick` once per DOS-clock second
(DOS). That is what makes it testable: tests feed any `dt` they like.
