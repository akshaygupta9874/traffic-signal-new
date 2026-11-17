;====================================================================
; DOSGFX.INC - VGA mode 13H (320x200, 256 colours) primitives.
; Pure drawing: no knowledge of traffic lights.
; Colours are the default VGA palette indexes.
;====================================================================
GFX_W EQU 320
GFX_H EQU 200

.DATA
gfx_color  DB 0               ; colour used by every fill primitive
gc_cx DW 0
gc_cy DW 0
gc_r  DW 0
gc_dy DW 0
gc_t  DW 0

.CODE
GFX_Init PROC                 ; enter mode 13H
    mov ax, 0013H
    int 10H
    ; Dim "lamp off" shades: the default VGA entries 2/4/6 are too bright next to
    ; the lit colours. Entries are not used by any text, so redefining is safe.
    mov al, 2                   ; dark green
    mov dh, 0
    mov ch, 14
    mov cl, 3
    call GFX_SetDac
    mov al, 4                   ; dark red
    mov dh, 16
    mov ch, 2
    mov cl, 2
    call GFX_SetDac
    mov al, 6                   ; dark amber
    mov dh, 20
    mov ch, 14
    mov cl, 0
    call GFX_SetDac
    ret
GFX_Init ENDP

;   GFX_SetDac   in: AL = colour index, DH = red, CH = green, CL = blue (each 0..63)
GFX_SetDac PROC
    push ax
    push bx
    xor bh, bh
    mov bl, al
    mov ax, 1010H
    int 10H
    pop bx
    pop ax
    ret
GFX_SetDac ENDP

GFX_Exit PROC                 ; back to 80x25 text
    mov ax, 0003H
    int 10H
    ret
GFX_Exit ENDP

;--------------------------------------------------------------------
; GFX_Clear - fill the whole screen with gfx_color
;--------------------------------------------------------------------
GFX_Clear PROC
    push ax
    push cx
    push di
    push es
    cld
    mov ax, 0A000H
    mov es, ax
    xor di, di
    mov al, gfx_color
    mov ah, al
    mov cx, 32000
    rep stosw
    pop es
    pop di
    pop cx
    pop ax
    ret
GFX_Clear ENDP

;--------------------------------------------------------------------
; GFX_FillRect   in: AX = x, BX = y, CX = width, DX = height (signed pixels)
; Clipped to the screen, so partly off-screen shapes (large zoom) are safe.
;--------------------------------------------------------------------
GFX_FillRect PROC
    SAVE_REGS
    push es
    cld
    cmp ax, 0                   ; clip left
    jge fr_xl
    add cx, ax
    xor ax, ax
fr_xl:
    mov si, ax                  ; clip right
    add si, cx
    cmp si, GFX_W
    jle fr_xr
    mov cx, GFX_W
    sub cx, ax
fr_xr:
    cmp cx, 0
    jle fr_done
    cmp bx, 0                   ; clip top
    jge fr_yt
    add dx, bx
    xor bx, bx
fr_yt:
    mov si, bx                  ; clip bottom
    add si, dx
    cmp si, GFX_H
    jle fr_yb
    mov dx, GFX_H
    sub dx, bx
fr_yb:
    cmp dx, 0
    jle fr_done
    mov si, ax                  ; SI = x
    push dx
    mov ax, GFX_W
    mul bx                      ; AX = y * 320
    pop dx
    add ax, si
    mov di, ax                  ; DI = y*320 + x
    mov ax, 0A000H
    mov es, ax
    mov al, gfx_color
fr_row:
    push di
    push cx
    rep stosb
    pop cx
    pop di
    add di, GFX_W
    dec dx
    jnz fr_row
fr_done:
    pop es
    REST_REGS
    ret
GFX_FillRect ENDP

;--------------------------------------------------------------------
; GFX_FillCircle   in: AX = centre x, BX = centre y, CX = radius (pixels)
; One horizontal span per scan line; span half-width = isqrt(r*r - dy*dy).
;--------------------------------------------------------------------
GFX_FillCircle PROC
    SAVE_REGS
    mov gc_cx, ax
    mov gc_cy, bx
    mov gc_r, cx
    mov ax, cx
    neg ax
    mov gc_dy, ax
gc_row:
    mov ax, gc_dy
    imul ax                     ; AX = dy*dy (small)
    mov bx, ax
    mov ax, gc_r
    imul ax                     ; AX = r*r
    sub ax, bx
    mov gc_t, ax                ; t = r*r - dy*dy  (>= 0)
    mov bx, gc_r                ; find largest dx with dx*dx <= t
gc_sq:
    mov ax, bx
    imul ax
    cmp ax, gc_t
    jle gc_found
    dec bx
    jmp gc_sq
gc_found:
    mov ax, gc_cx
    sub ax, bx                  ; x = cx - dx
    mov cx, bx
    shl cx, 1
    inc cx                      ; width = 2*dx + 1
    mov bx, gc_cy
    add bx, gc_dy               ; y = cy + dy
    mov dx, 1
    call GFX_FillRect
    inc gc_dy
    mov ax, gc_dy
    cmp ax, gc_r
    jle gc_row
    REST_REGS
    ret
GFX_FillCircle ENDP

;--------------------------------------------------------------------
; Text through the BIOS (fixed-size font; the scalable countdown is drawn
; with rectangles in DOSREND.INC instead).
;--------------------------------------------------------------------
;   GFX_Print   in: DH = row, DL = column, SI -> zero-terminated string, BL = colour
GFX_Print PROC
    push ax
    push bx
    push si
    mov ah, 02H
    mov bh, 0
    int 10H
gp_loop:
    lodsb
    or  al, al
    jz  gp_done
    mov ah, 0EH
    mov bh, 0
    int 10H
    jmp gp_loop
gp_done:
    pop si
    pop bx
    pop ax
    ret
GFX_Print ENDP

;   GFX_PutChar  in: AL = character at the cursor, BL = colour
GFX_PutChar PROC
    push ax
    push bx
    mov ah, 0EH
    mov bh, 0
    int 10H
    pop bx
    pop ax
    ret
GFX_PutChar ENDP

;   GFX_PutStr - print zero-terminated string at the current cursor. in: SI, BL = colour
GFX_PutStr PROC
    push ax
    push si
gps_loop:
    lodsb
    or  al, al
    jz  gps_done
    call GFX_PutChar
    jmp gps_loop
gps_done:
    pop si
    pop ax
    ret
GFX_PutStr ENDP

;   GFX_PrintU16 in: AX = unsigned number at the cursor, BL = colour
GFX_PrintU16 PROC
    push ax
    push cx
    push dx
    push si
    mov si, 10
    xor cx, cx
pu_div:
    xor dx, dx
    div si
    push dx
    inc cx
    or  ax, ax
    jnz pu_div
pu_out:
    pop ax
    add al, '0'
    call GFX_PutChar
    loop pu_out
    pop si
    pop dx
    pop cx
    pop ax
    ret
GFX_PrintU16 ENDP
