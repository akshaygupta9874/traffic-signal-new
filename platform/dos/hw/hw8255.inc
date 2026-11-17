;====================================================================
; HW\HW8255.INC - 8255 PPI driver. The ONLY file that knows port numbers.
;
;   Port A = 80H : PA0 GREEN, PA1 YELLOW, PA2 RED      (original mapping)
;   Port B = 82H : PB0 LEFT,  PB1 STRAIGHT, PB2 RIGHT  (new, optional wiring)
;   Port C = 84H : unused
;   Control= 86H : 80H = mode 0, all ports output      (original control word)
;====================================================================
HW_PORT_A     EQU 80H
HW_PORT_B     EQU 82H
HW_PORT_C     EQU 84H
HW_PORT_CTRL  EQU 86H
HW_CTRL_WORD  EQU 80H

; Port B drives the optional direction-arrow LEDs. Set to 0 if only the three
; lamps on Port A are wired: on an ordinary PC without an 8255, 82H is a DMA page
; register, so leave Port B alone unless a real/virtual 8255 sits at these ports.
HW_USE_PORT_B EQU 1

.DATA
hw_map      DB 01H, 02H, 04H, 00H      ; indexed by ST_GREEN, YELLOW, RED, OFF
hw_present  DB 0                       ; 1 = chip answered the read-back probe

.CODE
;--------------------------------------------------------------------
; HW_Init - program the control register, then probe for the chip.
; The probe only sets hw_present (shown on screen); outputs are ALWAYS
; sent, exactly like the original program, so DOSBox setups that
; capture OUT but ignore IN keep working.
;--------------------------------------------------------------------
HW_Init PROC
    push ax
    push dx
    mov dx, HW_PORT_CTRL
    mov al, HW_CTRL_WORD
    out dx, al
    mov dx, HW_PORT_A
    mov al, 0A5H
    out dx, al
    in  al, dx
    cmp al, 0A5H
    jne hi_done
    mov hw_present, 1
hi_done:
    xor al, al
    out dx, al                  ; lamps off until the first HW_Apply
    pop dx
    pop ax
    ret
HW_Init ENDP

;--------------------------------------------------------------------
; HW_Apply - translate te_state / te_dir into 8255 output
;--------------------------------------------------------------------
HW_Apply PROC
    push ax
    push bx
    push dx
    mov bl, te_state
    xor bh, bh
    mov al, hw_map[bx]
    mov dx, HW_PORT_A
    out dx, al
IF HW_USE_PORT_B
    mov al, te_dir
    cmp te_state, ST_OFF
    jne ha_dir
    xor al, al                  ; no arrows when everything is off
ha_dir:
    mov dx, HW_PORT_B
    out dx, al
ENDIF
    pop dx
    pop bx
    pop ax
    ret
HW_Apply ENDP

;--------------------------------------------------------------------
; HW_Shutdown - all lamps and arrows off
;--------------------------------------------------------------------
HW_Shutdown PROC
    push ax
    push dx
    xor al, al
    mov dx, HW_PORT_A
    out dx, al
IF HW_USE_PORT_B
    mov dx, HW_PORT_B
    out dx, al
ENDIF
    pop dx
    pop ax
    ret
HW_Shutdown ENDP
