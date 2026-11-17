;====================================================================
; MAIN.ASM - DOS / 8086 application: wires the modules together.
;
;   CORE\TE_CORE.INC  platform-independent state machine
;   HW\HW8255.INC     8255 driver (only place with port numbers)
;   DOSNET.INC       COM1 link (apps <-> MASM, ASCII line protocol)
;   DOSTIMER.INC      INT 21H clock         DOSINPUT.INC  INT 16H keyboard
;   DOSGFX.INC        INT 10H mode 13H      DOSREND.INC   signal drawing + zoom
;   DOSCFG.INC        command-line timing
;
; Event loop:  poll clock -> TE_Tick   poll keyboard -> manual control
;              state changed -> HW_Apply     anything changed -> REN_Draw
;
; Keys: G Y R lamp | L S D direction | N none  A all  C cycle | + - zoom  0 reset zoom
;       SPACE pause | M auto/manual | ESC exit
; (R is RED; zoom reset is 0 because R cannot mean both.)
;====================================================================
.8086
.MODEL SMALL
.STACK 200H

INCLUDE common.inc
INCLUDE te_core.inc
INCLUDE hw8255.inc
INCLUDE dosnet.inc
INCLUDE dostimer.inc
INCLUDE dosinput.inc
INCLUDE dosgfx.inc
INCLUDE dosrend.inc
INCLUDE doscfg.inc

.DATA
dirty  DB 1
quit   DB 0

.CODE
;--------------------------------------------------------------------
; HandleKey   in: AL = upper-cased key. Every manual action goes through
; the TE_* procedures, which validate input, so keys cannot corrupt state.
;--------------------------------------------------------------------
HandleKey PROC
    mov dirty, 1
    cmp al, 1BH
    jne hk_space
    mov quit, 1
    ret
hk_space:
    cmp al, ' '
    jne hk_g
    call TE_TogglePause
    ret
hk_g:
    cmp al, 'G'
    jne hk_y
    mov al, ST_GREEN
    call TE_SetState
    ret
hk_y:
    cmp al, 'Y'
    jne hk_r
    mov al, ST_YELLOW
    call TE_SetState
    ret
hk_r:
    cmp al, 'R'
    jne hk_l
    mov al, ST_RED
    call TE_SetState
    ret
hk_l:
    cmp al, 'L'
    jne hk_s
    mov al, DIR_LEFT
    call TE_SetDirection
    ret
hk_s:
    cmp al, 'S'
    jne hk_d
    mov al, DIR_STRAIGHT
    call TE_SetDirection
    ret
hk_d:
    cmp al, 'D'
    jne hk_n
    mov al, DIR_RIGHT
    call TE_SetDirection
    ret
hk_n:
    cmp al, 'N'
    jne hk_a
    mov al, DIR_NONE
    call TE_SetDirection
    ret
hk_a:
    cmp al, 'A'
    jne hk_c
    mov al, DIR_ALL
    call TE_SetDirection
    ret
hk_c:
    cmp al, 'C'
    jne hk_m
    call TE_CycleDirection
    ret
hk_m:
    cmp al, 'M'
    jne hk_plus
    call TE_ToggleMode
    ret
hk_plus:
    cmp al, '+'
    je  hk_zin
    cmp al, '='
    jne hk_minus
hk_zin:
    call RD_ZoomIn
    ret
hk_minus:
    cmp al, '-'
    je  hk_zout
    cmp al, '_'
    jne hk_zero
hk_zout:
    call RD_ZoomOut
    ret
hk_zero:
    cmp al, '0'
    jne hk_end
    call RD_ZoomReset
hk_end:
    ret                         ; unknown keys are ignored
HandleKey ENDP

MAIN PROC
    mov ax, ds                  ; DS = PSP at entry; keep it for the command tail
    mov bx, @data
    mov ds, bx
    mov psp_seg, ax

    call CFG_Parse              ; invalid timing -> message, exit code 1
    jnc m_cfg_ok
    mov dx, OFFSET msg_usage
    mov ah, 09H
    int 21H
    mov ax, 4C01H
    int 21H
m_cfg_ok:
    call TE_Init
    call HW_Init
    call NET_Init               ; COM1 link to the Guard / Controller apps (optional)
    call GFX_Init
    call DT_Init
    mov te_changed, 1

m_loop:
    call DT_PollSecond          ; CF = a second passed
    jnc m_keys
    call TE_Tick
    mov dirty, 1
m_keys:
    call NET_Poll               ; commands from the apps (same TE_* path as the keys)
    or  al, al
    jz  m_kbd
    mov dirty, 1
m_kbd:
    call DI_Poll
    or  al, al
    jz  m_hw
    call HandleKey
    cmp quit, 0
    jne m_exit
m_hw:
    cmp te_changed, 0
    je  m_draw
    call HW_Apply
    mov te_changed, 0
m_draw:
    cmp dirty, 0
    je  m_loop
    call NET_Publish            ; tell the apps FIRST, then do the slow redraw
    call REN_Draw
    mov dirty, 0
    jmp m_loop

m_exit:
    call TE_AllOff              ; apps see OFF before the link goes quiet
    call NET_Publish
    call HW_Shutdown            ; all lamps off, like the original EXIT_PROGRAM
    call GFX_Exit               ; restore text mode
    mov ax, 4C00H
    int 21H
MAIN ENDP

END MAIN
