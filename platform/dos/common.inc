;====================================================================
; COMMON.INC - shared equates and macros (no code, no data)
;====================================================================
; Light states (same numbering as core/traffic_state.hpp)
ST_GREEN    EQU 0
ST_YELLOW   EQU 1
ST_RED      EQU 2
ST_OFF      EQU 3

; Direction mask (same as core/direction.hpp)
DIR_NONE     EQU 0
DIR_LEFT     EQU 1
DIR_STRAIGHT EQU 2
DIR_RIGHT    EQU 4
DIR_ALL      EQU 7

MODE_AUTO   EQU 0
MODE_MANUAL EQU 1

SAVE_REGS MACRO
    push ax
    push bx
    push cx
    push dx
    push si
    push di
ENDM

REST_REGS MACRO
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
ENDM
