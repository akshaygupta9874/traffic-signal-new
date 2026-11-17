;====================================================================
; DOSTIMER.INC - DOS clock (INT 21H / AH=2CH), same method as the
; original DELAY_1SEC but non-blocking: the main loop polls it so keys
; are handled immediately instead of only once per second.
;====================================================================
.DATA
dt_last  DB 0

.CODE
DT_Init PROC
    push ax
    push cx
    push dx
    mov ah, 2CH
    int 21H                     ; DH = seconds
    mov dt_last, dh
    pop dx
    pop cx
    pop ax
    ret
DT_Init ENDP

;--------------------------------------------------------------------
; DT_PollSecond   out: CF=1 if the seconds field changed since last call
;--------------------------------------------------------------------
DT_PollSecond PROC
    push ax
    push cx
    push dx                     ; INT 21H/2CH overwrites CX and DX
    mov ah, 2CH
    int 21H
    cmp dh, dt_last
    je  dp_none
    mov dt_last, dh
    pop dx
    pop cx
    pop ax
    stc
    ret
dp_none:
    pop dx
    pop cx
    pop ax
    clc
    ret
DT_PollSecond ENDP
