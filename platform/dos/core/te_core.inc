;====================================================================
; CORE\TE_CORE.INC - platform-independent traffic engine (8086)
;
; Mirror of core/traffic_engine.cpp. It contains NO port addresses,
; NO INT instructions and NO drawing. Backends read the te_* variables.
;
; Time is counted in whole seconds: TE_Tick is called once per second
; by whoever owns a clock (here: DOSTIMER.INC).
;
; Error convention: procedures that validate input return CF=1 and
; leave the state unchanged when the input is invalid.
;====================================================================
.DATA
te_dur       DW 10, 3, 10        ; configured seconds: green, yellow, red
te_state     DB ST_GREEN
te_dir       DB DIR_STRAIGHT
te_remain    DW 10              ; seconds left in this phase (shown as countdown)
te_duration  DW 10              ; full length of the current phase
te_paused    DB 0
te_mode      DB MODE_AUTO
te_changed   DB 1               ; 1 = state/direction changed, outputs must refresh

.CODE
;--------------------------------------------------------------------
; TE_Reset - GREEN, full time, auto mode, not paused, default direction
;--------------------------------------------------------------------
TE_Reset PROC
    mov te_paused, 0
    mov te_mode, MODE_AUTO
    mov te_dir, DIR_STRAIGHT
    mov al, ST_GREEN
    call TE_SetState
    ret
TE_Reset ENDP

;--------------------------------------------------------------------
; TE_Init - (durations may already have been set by the command line)
;--------------------------------------------------------------------
TE_Init PROC
    call TE_Reset
    ret
TE_Init ENDP

;--------------------------------------------------------------------
; TE_SetState   in: AL = ST_GREEN/YELLOW/RED     out: CF=1 if invalid
; Restarts the phase timer. Used by the automatic cycle AND by manual keys,
; so a manual change can never leave the machine half-updated.
;--------------------------------------------------------------------
TE_SetState PROC
    cmp al, ST_RED
    ja  tss_bad
    push bx
    mov te_state, al
    xor ah, ah
    mov bx, ax
    shl bx, 1
    mov ax, te_dur[bx]
    mov te_duration, ax
    mov te_remain, ax
    mov te_changed, 1
    pop bx
    clc
    ret
tss_bad:
    stc
    ret
TE_SetState ENDP

;--------------------------------------------------------------------
; TE_AllOff - every lamp dark; the automatic cycle stops until a new state is set
;--------------------------------------------------------------------
TE_AllOff PROC
    mov te_state, ST_OFF
    mov te_remain, 0
    mov te_duration, 0
    mov te_changed, 1
    ret
TE_AllOff ENDP

;--------------------------------------------------------------------
; TE_SetDirection   in: AL = mask 0..7           out: CF=1 if invalid
;--------------------------------------------------------------------
TE_SetDirection PROC
    cmp al, DIR_ALL
    ja  tsd_bad
    mov te_dir, al
    mov te_changed, 1
    clc
    ret
tsd_bad:
    stc
    ret
TE_SetDirection ENDP

;--------------------------------------------------------------------
; TE_SetDuration   in: AL = ST_GREEN/YELLOW/RED, BX = seconds 1..99
; out: CF=1 if invalid. Takes effect the next time that phase is entered.
;--------------------------------------------------------------------
TE_SetDuration PROC
    cmp al, ST_RED
    ja  tsu_bad
    cmp bx, 1
    jb  tsu_bad
    cmp bx, 99
    ja  tsu_bad
    push si
    xor ah, ah
    mov si, ax
    shl si, 1
    mov te_dur[si], bx
    pop si
    clc
    ret
tsu_bad:
    stc
    ret
TE_SetDuration ENDP

;--------------------------------------------------------------------
; TE_Tick - call once per second. Counts down and advances G -> Y -> R -> G.
; Does nothing while paused, in manual mode, or when all lamps are off.
;--------------------------------------------------------------------
TE_Tick PROC
    cmp te_paused, 0
    jne tt_done
    cmp te_mode, MODE_AUTO
    jne tt_done
    mov al, te_state
    cmp al, ST_OFF
    jae tt_done
    dec te_remain
    jnz tt_done
    inc al                      ; next phase
    cmp al, ST_RED
    jbe tt_go
    xor al, al                  ; after RED comes GREEN
tt_go:
    call TE_SetState
tt_done:
    ret
TE_Tick ENDP

TE_TogglePause PROC
    xor te_paused, 1
    ret
TE_TogglePause ENDP

TE_ToggleMode PROC
    xor te_mode, 1
    ret
TE_ToggleMode ENDP

TE_CycleDirection PROC
    mov al, te_dir
    inc al
    and al, 7
    call TE_SetDirection
    ret
TE_CycleDirection ENDP
