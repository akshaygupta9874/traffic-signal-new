;====================================================================
; DOSREND.INC - draws the signal from the te_* state.
;
; All geometry is in LOGICAL units; RD_SL converts with the zoom factor
; (only here: rd_scale). There are no per-zoom coordinates.
;   pixels = logical * rd_scale / 256        (rd_scale is 8.8 fixed point)
; The DOS renderer is read-only on te_*: it never changes the state.
;====================================================================
ORIGIN_X   EQU 160
ORIGIN_Y   EQU 88
RD_ONE     EQU 256            ; 1.0
RD_MIN     EQU 128            ; 0.5x
RD_MAX     EQU 384            ; 1.5x  (screen is only 200 px tall)
RD_STEP    EQU 32             ; 0.125x per key press

LAMP_R     EQU 10
ARROW_Y    EQU 40
ARROW_DX   EQU 16

; VGA default palette indexes
C_BLACK    EQU 0
C_GRAY     EQU 7
C_DKGRAY   EQU 8
C_WHITE    EQU 15

.DATA
rd_scale   DW RD_ONE
lamp_y     DW -40, -14, 12            ; red, yellow, green (logical Y)
lamp_on    DB 12, 14, 10              ; bright red, yellow, green
lamp_off   DB 4, 6, 2                 ; dark red, brown, dark green
state_col  DB 10, 14, 12, 7           ; text/arrow colour per state G,Y,R,OFF
digit_mask DB 3FH,06H,5BH,4FH,66H,6DH,7DH,07H,7FH,6FH    ; 7-segment a..g = bit0..6
seg_rects  DB 1,0,8,2,  8,1,2,8,  8,9,2,8,  1,16,8,2,  0,9,2,8,  0,1,2,8,  1,8,8,2   ; x,y,w,h per segment a..g

rr_lx DW 0
rr_ly DW 0
rr_lw DW 0
rr_lh DW 0
rr_px DW 0
rr_py DW 0
rr_pw DW 0
rr_ph DW 0
ga_cx DW 0
ga_cy DW 0
dg_x  DW 0
dg_y  DW 0
dg_mask DB 0
cd_t  DB 0
cd_u  DB 0
zm_f  DB 0

s_title   DB 'TRAFFIC SIGNAL', 0
s_state   DB 'STATE: ', 0
s_dir     DB 'DIR: ', 0
s_time    DB 'TIME: ', 0
s_zoom    DB 'ZOOM: ', 0
s_mode    DB 'MODE: ', 0
s_hw      DB 'OUT: ', 0
s_auto    DB 'AUTO', 0
s_manual  DB 'MANUAL', 0
s_paused  DB 'PAUSED', 0
s_8255    DB '8255', 0
s_sim     DB 'SCREEN ONLY', 0
s_net     DB 'NET: ', 0
s_net_on  DB 'COM1 LINK', 0
s_net_off DB 'OFFLINE', 0
nm_green  DB 'GREEN', 0
nm_yellow DB 'YELLOW', 0
nm_red    DB 'RED', 0
nm_off    DB 'OFF', 0
dn_0      DB 'NONE', 0
dn_1      DB 'LEFT', 0
dn_2      DB 'STRAIGHT', 0
dn_3      DB 'ST+LEFT', 0
dn_4      DB 'RIGHT', 0
dn_5      DB 'L+R', 0
dn_6      DB 'ST+RIGHT', 0
dn_7      DB 'ALL', 0
state_names DW OFFSET nm_green, OFFSET nm_yellow, OFFSET nm_red, OFFSET nm_off
dir_names   DW OFFSET dn_0, OFFSET dn_1, OFFSET dn_2, OFFSET dn_3
            DW OFFSET dn_4, OFFSET dn_5, OFFSET dn_6, OFFSET dn_7
h1 DB '+ -  ZOOM', 0
h2 DB '0    RESET ZOOM', 0
h3 DB 'SPACE PAUSE', 0
h4 DB 'G Y R LAMP', 0
h5 DB 'L S D DIR', 0
h6 DB 'C CYCLE DIR', 0
h7 DB 'N NONE A ALL', 0
h8 DB 'M AUTO/MANUAL', 0
h9 DB 'ESC EXIT', 0
help_lines DW OFFSET h1, OFFSET h2, OFFSET h3, OFFSET h4, OFFSET h5
           DW OFFSET h6, OFFSET h7, OFFSET h8, OFFSET h9

.CODE
;--------------------------------------------------------------------
; Zoom control (clamped; invalid scale can never be stored)
;--------------------------------------------------------------------
RD_ZoomIn PROC
    add rd_scale, RD_STEP
    cmp rd_scale, RD_MAX
    jbe rz_in_ok
    mov rd_scale, RD_MAX
rz_in_ok:
    ret
RD_ZoomIn ENDP

RD_ZoomOut PROC
    cmp rd_scale, RD_MIN + RD_STEP
    jae rz_out_ok
    mov rd_scale, RD_MIN
    ret
rz_out_ok:
    sub rd_scale, RD_STEP
    ret
RD_ZoomOut ENDP

RD_ZoomReset PROC
    mov rd_scale, RD_ONE
    ret
RD_ZoomReset ENDP

;--------------------------------------------------------------------
; RD_SL   in: AX = signed logical length   out: AX = pixels (rounded)
;--------------------------------------------------------------------
RD_SL PROC
    push dx
    imul rd_scale               ; DX:AX = AX * scale
    add ax, 128
    adc dx, 0                   ; rounding
    mov al, ah                  ; result = (DX:AX) >> 8
    mov ah, dl
    pop dx
    ret
RD_SL ENDP

;--------------------------------------------------------------------
; RD_Rect   in: AX = x, BX = y, CX = w, DX = h (logical), gfx_color set
; Edges are scaled separately, so adjacent shapes never leave gaps.
;--------------------------------------------------------------------
RD_Rect PROC
    SAVE_REGS
    mov rr_lx, ax
    mov rr_ly, bx
    mov rr_lw, cx
    mov rr_lh, dx
    mov ax, rr_lx               ; --- X ---
    call RD_SL
    mov si, ax
    mov ax, rr_lx
    add ax, rr_lw
    call RD_SL
    sub ax, si
    cmp ax, 1
    jge rr_wok
    mov ax, 1
rr_wok:
    mov rr_pw, ax
    add si, ORIGIN_X
    mov rr_px, si
    mov ax, rr_ly               ; --- Y ---
    call RD_SL
    mov si, ax
    mov ax, rr_ly
    add ax, rr_lh
    call RD_SL
    sub ax, si
    cmp ax, 1
    jge rr_hok
    mov ax, 1
rr_hok:
    mov rr_ph, ax
    add si, ORIGIN_Y
    mov rr_py, si
    mov ax, rr_px
    mov bx, rr_py
    mov cx, rr_pw
    mov dx, rr_ph
    call GFX_FillRect
    REST_REGS
    ret
RD_Rect ENDP

;--------------------------------------------------------------------
; RD_Circle   in: AX = centre x, BX = centre y, CX = radius (logical)
;--------------------------------------------------------------------
RD_Circle PROC
    SAVE_REGS
    mov si, cx                  ; radius
    call RD_SL                  ; AX = scaled x
    add ax, ORIGIN_X
    mov di, ax
    mov ax, bx
    call RD_SL
    add ax, ORIGIN_Y
    mov bx, ax                  ; BX = pixel y
    mov ax, si
    call RD_SL
    cmp ax, 1
    jge rc_rok
    mov ax, 1
rc_rok:
    mov cx, ax                  ; CX = pixel radius
    mov ax, di                  ; AX = pixel x
    call GFX_FillCircle
    REST_REGS
    ret
RD_Circle ENDP

;--------------------------------------------------------------------
; Parts of the signal
;--------------------------------------------------------------------
RD_Housing PROC
    SAVE_REGS
    mov gfx_color, C_GRAY       ; edge
    mov ax, -28
    mov bx, -56
    mov cx, 56
    mov dx, 112
    call RD_Rect
    mov gfx_color, C_DKGRAY     ; body
    mov ax, -26
    mov bx, -54
    mov cx, 52
    mov dx, 108
    call RD_Rect
    REST_REGS
    ret
RD_Housing ENDP

RD_Pole PROC
    SAVE_REGS
    mov gfx_color, C_GRAY
    mov ax, -4
    mov bx, 56
    mov cx, 8
    mov dx, 60
    call RD_Rect
    REST_REGS
    ret
RD_Pole ENDP

;--------------------------------------------------------------------
; RD_Lamps - three round lamps; the one matching te_state is bright,
; the others use the dark "off" colour (as in the original).
; Lamp index i: 0 = red, 1 = yellow, 2 = green  ->  lit when te_state == 2 - i
;--------------------------------------------------------------------
RD_Lamps PROC
    SAVE_REGS
    xor si, si
rl_next:
    mov di, si
    shl di, 1
    mov gfx_color, C_BLACK      ; bezel
    xor ax, ax
    mov bx, lamp_y[di]
    mov cx, LAMP_R + 2
    call RD_Circle
    mov ax, 2
    sub ax, si
    mov dl, lamp_off[si]
    cmp al, te_state
    jne rl_set
    mov dl, lamp_on[si]
rl_set:
    mov gfx_color, dl
    xor ax, ax
    mov bx, lamp_y[di]
    mov cx, LAMP_R
    call RD_Circle
    inc si
    cmp si, 3
    jb  rl_next
    REST_REGS
    ret
RD_Lamps ENDP

;--------------------------------------------------------------------
; Arrow glyphs. in: AX = centre x, BX = centre y (logical). Head is built
; from 1-unit rows/columns, so it scales with everything else.
;--------------------------------------------------------------------
RD_ArrowUp PROC
    SAVE_REGS
    mov ga_cx, ax
    mov ga_cy, bx
    xor si, si
au_h:
    mov ax, ga_cx
    sub ax, si
    mov bx, ga_cy
    sub bx, 8
    add bx, si
    mov cx, si
    shl cx, 1
    inc cx
    mov dx, 1
    call RD_Rect
    inc si
    cmp si, 6
    jb  au_h
    mov ax, ga_cx               ; shaft
    dec ax
    mov bx, ga_cy
    sub bx, 2
    mov cx, 3
    mov dx, 9
    call RD_Rect
    REST_REGS
    ret
RD_ArrowUp ENDP

RD_ArrowLeft PROC
    SAVE_REGS
    mov ga_cx, ax
    mov ga_cy, bx
    xor si, si
alf_h:
    mov ax, ga_cx
    sub ax, 8
    add ax, si
    mov bx, ga_cy
    sub bx, si
    mov cx, 1
    mov dx, si
    shl dx, 1
    inc dx
    call RD_Rect
    inc si
    cmp si, 6
    jb  alf_h
    mov ax, ga_cx               ; shaft
    sub ax, 2
    mov bx, ga_cy
    dec bx
    mov cx, 9
    mov dx, 3
    call RD_Rect
    REST_REGS
    ret
RD_ArrowLeft ENDP

RD_ArrowRight PROC
    SAVE_REGS
    mov ga_cx, ax
    mov ga_cy, bx
    xor si, si
ar_h:
    mov ax, ga_cx
    add ax, 7
    sub ax, si
    mov bx, ga_cy
    sub bx, si
    mov cx, 1
    mov dx, si
    shl dx, 1
    inc dx
    call RD_Rect
    inc si
    cmp si, 6
    jb  ar_h
    mov ax, ga_cx               ; shaft
    sub ax, 7
    mov bx, ga_cy
    dec bx
    mov cx, 9
    mov dx, 3
    call RD_Rect
    REST_REGS
    ret
RD_ArrowRight ENDP

;--------------------------------------------------------------------
; RD_Arrows - direction arrows in the colour of the current phase.
; Nothing is drawn for DIR_NONE or when all lamps are off.
;--------------------------------------------------------------------
RD_Arrows PROC
    SAVE_REGS
    cmp te_dir, DIR_NONE
    je  ra_done
    cmp te_state, ST_OFF
    jae ra_done
    mov bl, te_state
    xor bh, bh
    mov al, state_col[bx]
    mov gfx_color, al
    test te_dir, DIR_LEFT
    jz  ra_s
    mov ax, -ARROW_DX
    mov bx, ARROW_Y
    call RD_ArrowLeft
ra_s:
    test te_dir, DIR_STRAIGHT
    jz  ra_r
    xor ax, ax
    mov bx, ARROW_Y
    call RD_ArrowUp
ra_r:
    test te_dir, DIR_RIGHT
    jz  ra_done
    mov ax, ARROW_DX
    mov bx, ARROW_Y
    call RD_ArrowRight
ra_done:
    REST_REGS
    ret
RD_Arrows ENDP

;--------------------------------------------------------------------
; RD_Digit - one scalable 7-segment digit.
;   in: AL = digit 0..9, BX = logical x, CX = logical y
;--------------------------------------------------------------------
RD_Digit PROC
    SAVE_REGS
    mov dg_x, bx
    mov dg_y, cx
    xor ah, ah
    mov si, ax
    mov al, digit_mask[si]
    mov dg_mask, al
    xor si, si
dg_loop:
    mov al, dg_mask
    shr al, 1
    mov dg_mask, al
    jnc dg_skip                 ; segment not lit
    mov di, si
    shl di, 1
    shl di, 1                   ; DI = segment * 4
    mov al, seg_rects[di]
    xor ah, ah
    add ax, dg_x
    mov bl, seg_rects[di+1]
    xor bh, bh
    add bx, dg_y
    mov cl, seg_rects[di+2]
    xor ch, ch
    mov dl, seg_rects[di+3]
    xor dh, dh
    call RD_Rect
dg_skip:
    inc si
    cmp si, 7
    jb  dg_loop
    REST_REGS
    ret
RD_Digit ENDP

;--------------------------------------------------------------------
; RD_Countdown - te_remain as two digits right of the housing
; (the engine computes the value; this only displays it)
;--------------------------------------------------------------------
RD_Countdown PROC
    SAVE_REGS
    cmp te_state, ST_OFF
    jae rcd_done
    mov bl, te_state
    xor bh, bh
    mov al, state_col[bx]
    mov gfx_color, al
    mov ax, te_remain
    mov bl, 10
    div bl                      ; AL = tens, AH = units
    mov cd_t, al
    mov cd_u, ah
    cmp cd_t, 0
    je  rcd_units               ; no leading zero
    mov al, cd_t
    mov bx, 36
    mov cx, -50
    call RD_Digit
rcd_units:
    mov al, cd_u
    mov bx, 48
    mov cx, -50
    call RD_Digit
rcd_done:
    REST_REGS
    ret
RD_Countdown ENDP

;--------------------------------------------------------------------
; RD_PrintZoom - "n.nn" from the 8.8 fixed-point scale, rounded
;--------------------------------------------------------------------
RD_PrintZoom PROC
    push ax
    push cx
    push dx
    mov ax, rd_scale
    mov cx, 100
    mul cx
    add ax, 128
    adc dx, 0
    mov cx, 256
    div cx                      ; AX = zoom * 100
    mov cl, 100
    div cl                      ; AL = integer part, AH = hundredths
    mov zm_f, ah
    xor ah, ah
    mov bl, C_WHITE
    call GFX_PrintU16
    mov al, '.'
    call GFX_PutChar
    mov al, zm_f
    xor ah, ah
    mov cl, 10
    div cl
    add al, '0'
    call GFX_PutChar
    mov al, ah
    add al, '0'
    call GFX_PutChar
    pop dx
    pop cx
    pop ax
    ret
RD_PrintZoom ENDP

;--------------------------------------------------------------------
; RD_Text - fixed-size status panel (left) and key help (right)
;--------------------------------------------------------------------
RD_Text PROC
    SAVE_REGS
    mov bl, C_WHITE
    mov dh, 0
    mov dl, 0
    mov si, OFFSET s_title
    call GFX_Print
    mov bl, C_GRAY              ; STATE: <name>
    mov dh, 2
    mov dl, 0
    mov si, OFFSET s_state
    call GFX_Print
    mov bl, te_state
    xor bh, bh
    mov al, state_col[bx]
    mov cl, al                  ; keep colour for the value
    mov si, bx
    shl si, 1
    mov si, state_names[si]
    mov bl, cl
    call GFX_PutStr
    mov bl, C_GRAY              ; DIR: <name>
    mov dh, 3
    mov dl, 0
    mov si, OFFSET s_dir
    call GFX_Print
    mov bl, te_dir
    xor bh, bh
    mov si, bx
    shl si, 1
    mov si, dir_names[si]
    mov bl, C_WHITE
    call GFX_PutStr
    mov bl, C_GRAY              ; TIME: <n>
    mov dh, 4
    mov dl, 0
    mov si, OFFSET s_time
    call GFX_Print
    mov ax, te_remain
    mov bl, C_WHITE
    call GFX_PrintU16
    mov bl, C_GRAY              ; ZOOM: n.nn
    mov dh, 5
    mov dl, 0
    mov si, OFFSET s_zoom
    call GFX_Print
    call RD_PrintZoom
    mov bl, C_GRAY              ; MODE: AUTO / MANUAL
    mov dh, 6
    mov dl, 0
    mov si, OFFSET s_mode
    call GFX_Print
    mov si, OFFSET s_auto
    cmp te_mode, MODE_AUTO
    je  rt_mode
    mov si, OFFSET s_manual
rt_mode:
    mov bl, C_WHITE
    call GFX_PutStr
    cmp te_paused, 0
    je  rt_hw
    mov dh, 7
    mov dl, 0
    mov si, OFFSET s_paused
    mov bl, 14
    call GFX_Print
rt_hw:
    mov bl, C_GRAY              ; OUT: 8255 / SCREEN ONLY
    mov dh, 8
    mov dl, 0
    mov si, OFFSET s_hw
    call GFX_Print
    mov si, OFFSET s_sim
    cmp hw_present, 0
    je  rt_hwp
    mov si, OFFSET s_8255
rt_hwp:
    mov bl, C_WHITE
    call GFX_PutStr
    mov bl, C_GRAY              ; NET: COM1 LINK / OFFLINE
    mov dh, 9
    mov dl, 0
    mov si, OFFSET s_net
    call GFX_Print
    mov si, OFFSET s_net_off
    cmp net_on, 0
    je  rt_netp
    mov si, OFFSET s_net_on
rt_netp:
    mov bl, C_WHITE
    call GFX_PutStr
    xor di, di                  ; key help, right column
    mov dh, 10
rt_help:
    mov dl, 26
    mov si, di
    shl si, 1
    mov si, help_lines[si]
    mov bl, C_GRAY
    call GFX_Print
    inc dh
    inc di
    cmp di, 9
    jb  rt_help
    REST_REGS
    ret
RD_Text ENDP

;--------------------------------------------------------------------
; REN_Draw - one complete frame
;--------------------------------------------------------------------
REN_Draw PROC
    push ax
    mov gfx_color, C_BLACK
    call GFX_Clear
    call RD_Pole
    call RD_Housing
    call RD_Lamps
    call RD_Arrows
    call RD_Countdown
    call RD_Text
    pop ax
    ret
REN_Draw ENDP
