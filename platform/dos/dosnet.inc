;====================================================================
; DOSNET.INC - network link (8086 / MASM). The Guard and Controller apps
; talk to THIS program; it is the single authority for the signal.
;
;   Transport : COM1 (8250 UART, ports 3F8H-3FFH, 115200 8N1, polled).
;               DOSBox maps COM1 to a TCP port (see dosbox-traffic.conf);
;               a tiny relay forwards that TCP stream to WebSockets.
;   Protocol  : ASCII lines ending in LF (CR is accepted too).
;
;   MASM -> apps   S <st> <dir> <rem> <dur> <mode> <pause> <seq> <ack> <g> <y> <r>
;                    st   G Y R or O (off)       dir  mask 0..7
;                    rem  seconds left           dur  length of this phase
;                    mode A auto / M manual      pause 0/1
;                    seq  publish counter        ack  last "? n" id received
;                    g y r configured durations
;                  E <code>      1 = syntax error   2 = value out of range
;
;   apps -> MASM   G | Y | R     set lamp (restarts that phase)
;                  X             all off            Z   reset
;                  D <0..7>      direction mask     A / M   auto / manual
;                  P0 / P1       resume / pause (explicit, never a toggle)
;                  T g y r       durations 1..99 s
;                  ? [n]         publish now; n is echoed back as <ack>
;
; Every command goes through the same TE_* procedures as the keyboard, so
; the keyboard, the apps and the automatic cycle can never disagree.
; If no UART answers the scratch-register probe, the link stays disabled
; and the program behaves exactly as before.
;====================================================================
NET_BASE EQU 3F8H               ; COM1
NET_IER  EQU NET_BASE + 1
NET_FCR  EQU NET_BASE + 2
NET_LCR  EQU NET_BASE + 3
NET_MCR  EQU NET_BASE + 4
NET_LSR  EQU NET_BASE + 5
NET_SCR  EQU NET_BASE + 7
NET_BUFSZ EQU 24

.DATA
net_on    DB 0                  ; 1 = UART present and programmed
net_len   DB 0
net_buf   DB NET_BUFSZ DUP(0)
net_got   DB 0                  ; Poll result: a complete line was handled
net_seq   DW 0
net_ack   DW 0
net_t1    DW 0
net_t2    DW 0
net_t3    DW 0
net_stch  DB 'G', 'Y', 'R', 'O' ; indexed by ST_*

.CODE
;--------------------------------------------------------------------
; NET_Init - probe the UART (scratch register), then 115200 8N1, no IRQ
;--------------------------------------------------------------------
NET_Init PROC
    push ax
    push dx
    mov dx, NET_SCR
    mov al, 0A5H
    out dx, al
    in  al, dx
    cmp al, 0A5H
    jne ni_done                 ; nothing at 3F8H: stay offline
    mov dx, NET_LCR
    mov al, 80H                 ; DLAB = 1
    out dx, al
    mov dx, NET_BASE
    mov al, 1                   ; divisor 1 = 115200 baud
    out dx, al
    inc dx
    xor al, al
    out dx, al
    mov dx, NET_LCR
    mov al, 03H                 ; 8 data bits, no parity, 1 stop, DLAB = 0
    out dx, al
    mov dx, NET_IER
    xor al, al                  ; polled: no interrupts
    out dx, al
    mov dx, NET_FCR
    mov al, 07H                 ; enable + clear FIFOs (ignored by an 8250)
    out dx, al
    mov dx, NET_MCR
    mov al, 03H                 ; DTR + RTS
    out dx, al
    mov dx, NET_BASE
    in  al, dx                  ; discard any stale byte
    mov net_on, 1
ni_done:
    pop dx
    pop ax
    ret
NET_Init ENDP

;--------------------------------------------------------------------
; NET_PutCh   in: AL = byte. Waits for the transmitter, bounded so a
; dead link can never hang the signal (the byte is dropped on timeout).
;--------------------------------------------------------------------
NET_PutCh PROC
    push ax
    push cx
    push dx
    mov ah, al
    mov cx, 0FFFFH
npc_wait:
    mov dx, NET_LSR
    in  al, dx
    test al, 20H                ; THRE: holding register empty
    jnz npc_send
    loop npc_wait
    jmp npc_end
npc_send:
    mov dx, NET_BASE
    mov al, ah
    out dx, al
npc_end:
    pop dx
    pop cx
    pop ax
    ret
NET_PutCh ENDP

NET_Sp PROC
    push ax
    mov al, ' '
    call NET_PutCh
    pop ax
    ret
NET_Sp ENDP

;--------------------------------------------------------------------
; NET_PutNum   in: AX = unsigned 16-bit value -> decimal text
;--------------------------------------------------------------------
NET_PutNum PROC
    push ax
    push bx
    push cx
    push dx
    mov bx, 10
    xor cx, cx
pn_div:
    xor dx, dx
    div bx
    push dx
    inc cx
    or  ax, ax
    jnz pn_div
pn_out:
    pop ax
    add al, '0'
    call NET_PutCh
    loop pn_out
    pop dx
    pop cx
    pop bx
    pop ax
    ret
NET_PutNum ENDP

;--------------------------------------------------------------------
; NET_Publish - send one S line with the full state. Called BEFORE the
; (slow) screen redraw so apps see a change as early as possible.
;--------------------------------------------------------------------
NET_Publish PROC
    cmp net_on, 0
    jne np_go
    ret
np_go:
    push ax
    push bx
    mov al, 'S'
    call NET_PutCh
    call NET_Sp
    mov bl, te_state
    xor bh, bh
    mov al, net_stch[bx]
    call NET_PutCh
    call NET_Sp
    mov al, te_dir
    xor ah, ah
    call NET_PutNum
    call NET_Sp
    mov ax, te_remain
    call NET_PutNum
    call NET_Sp
    mov ax, te_duration
    call NET_PutNum
    call NET_Sp
    mov al, 'A'
    cmp te_mode, MODE_AUTO
    je  np_mode
    mov al, 'M'
np_mode:
    call NET_PutCh
    call NET_Sp
    mov al, te_paused
    add al, '0'
    call NET_PutCh
    call NET_Sp
    inc net_seq
    mov ax, net_seq
    call NET_PutNum
    call NET_Sp
    mov ax, net_ack
    call NET_PutNum
    call NET_Sp
    mov ax, te_dur              ; configured green
    call NET_PutNum
    call NET_Sp
    mov ax, te_dur + 2          ; yellow
    call NET_PutNum
    call NET_Sp
    mov ax, te_dur + 4          ; red
    call NET_PutNum
    mov al, 0AH
    call NET_PutCh
    pop bx
    pop ax
    ret
NET_Publish ENDP

;--------------------------------------------------------------------
; NET_Err   in: AL = error digit '1' or '2'  -> "E n"
;--------------------------------------------------------------------
NET_Err PROC
    push ax
    mov ah, al
    mov al, 'E'
    call NET_PutCh
    call NET_Sp
    mov al, ah
    call NET_PutCh
    mov al, 0AH
    call NET_PutCh
    pop ax
    ret
NET_Err ENDP

;--------------------------------------------------------------------
; NET_Num   in: SI -> text   out: AX = value, SI advanced, CF=1 if there
; were no digits. Values above ~6000 saturate to 0FFFFH (range error later).
;--------------------------------------------------------------------
NET_Num PROC
    push bx
    push cx
    push dx
    xor bx, bx
    xor cx, cx
nn_skip:
    cmp BYTE PTR [si], ' '
    jne nn_dig
    inc si
    jmp nn_skip
nn_dig:
    mov al, [si]
    sub al, '0'
    cmp al, 9
    ja  nn_end                  ; not a digit (unsigned compare)
    cmp bx, 6000
    ja  nn_big
    mov dx, bx
    shl bx, 1
    shl bx, 1
    add bx, dx                  ; x5
    shl bx, 1                   ; x10
    xor ah, ah
    add bx, ax
    jmp nn_next
nn_big:
    mov bx, 0FFFFH
nn_next:
    inc si
    inc cx
    jmp nn_dig
nn_end:
    mov ax, bx
    cmp cx, 1                   ; CF=1 when no digit was read
    pop dx
    pop cx
    pop bx
    ret
NET_Num ENDP

;--------------------------------------------------------------------
; NET_Exec - run the command line in net_buf (upper-cased, length net_len)
;--------------------------------------------------------------------
NET_Exec PROC
    push ax
    push bx
    push si
    mov bl, net_len
    xor bh, bh
    mov net_buf[bx], 0
    mov si, OFFSET net_buf
    mov al, [si]
    inc si
    cmp al, 'G'
    jne ne_y
    mov al, ST_GREEN
    jmp ne_state
ne_y:
    cmp al, 'Y'
    jne ne_r
    mov al, ST_YELLOW
    jmp ne_state
ne_r:
    cmp al, 'R'
    jne ne_x
    mov al, ST_RED
    jmp ne_state
ne_x:
    cmp al, 'X'
    jne ne_z
    cmp BYTE PTR [si], 0
    jne ne_syntax
    call TE_AllOff
    jmp ne_done
ne_z:
    cmp al, 'Z'
    jne ne_a
    cmp BYTE PTR [si], 0
    jne ne_syntax
    call TE_Reset
    jmp ne_done
ne_a:
    cmp al, 'A'
    jne ne_m
    cmp BYTE PTR [si], 0
    jne ne_syntax
    mov te_mode, MODE_AUTO
    jmp ne_done
ne_m:
    cmp al, 'M'
    jne ne_p
    cmp BYTE PTR [si], 0
    jne ne_syntax
    mov te_mode, MODE_MANUAL
    jmp ne_done
ne_p:
    cmp al, 'P'
    jne ne_d
    mov al, [si]
    inc si
    sub al, '0'
    cmp al, 1
    ja  ne_syntax
    cmp BYTE PTR [si], 0
    jne ne_syntax
    mov te_paused, al
    jmp ne_done
ne_d:
    cmp al, 'D'
    jne ne_t
    mov al, [si]
    inc si
    sub al, '0'
    cmp al, 9
    ja  ne_syntax
    cmp BYTE PTR [si], 0
    jne ne_syntax
    call TE_SetDirection        ; CF=1 if > 7
    jc  ne_range
    jmp ne_done
ne_t:
    cmp al, 'T'
    jne ne_q
    call NET_Num
    jc  ne_syntax
    mov net_t1, ax
    call NET_Num
    jc  ne_syntax
    mov net_t2, ax
    call NET_Num
    jc  ne_syntax
    mov net_t3, ax
    cmp BYTE PTR [si], 0
    jne ne_syntax
    mov ax, net_t1              ; validate all three BEFORE applying any
    cmp ax, 1
    jb  ne_range
    cmp ax, 99
    ja  ne_range
    mov ax, net_t2
    cmp ax, 1
    jb  ne_range
    cmp ax, 99
    ja  ne_range
    mov ax, net_t3
    cmp ax, 1
    jb  ne_range
    cmp ax, 99
    ja  ne_range
    mov al, ST_GREEN
    mov bx, net_t1
    call TE_SetDuration
    mov al, ST_YELLOW
    mov bx, net_t2
    call TE_SetDuration
    mov al, ST_RED
    mov bx, net_t3
    call TE_SetDuration
    jmp ne_done
ne_q:
    cmp al, '?'
    jne ne_syntax
    cmp BYTE PTR [si], 0
    je  ne_done                 ; bare "?" = just publish
    call NET_Num
    jc  ne_syntax
    cmp BYTE PTR [si], 0
    jne ne_syntax
    mov net_ack, ax
    jmp ne_done
ne_state:                       ; AL = ST_*
    cmp BYTE PTR [si], 0
    jne ne_syntax
    call TE_SetState
    jc  ne_range
    jmp ne_done
ne_syntax:
    mov al, '1'
    call NET_Err
    jmp ne_done
ne_range:
    mov al, '2'
    call NET_Err
ne_done:
    pop si
    pop bx
    pop ax
    ret
NET_Exec ENDP

;--------------------------------------------------------------------
; NET_Poll - drain the UART. out: AL = 1 if at least one line was handled
; (caller must then publish). Bounded to 64 bytes per call.
;--------------------------------------------------------------------
NET_Poll PROC
    push bx
    push cx
    push dx
    mov net_got, 0
    cmp net_on, 0
    je  nq_end
    mov cx, 64
nq_next:
    mov dx, NET_LSR
    in  al, dx
    test al, 01H                ; data ready?
    jz  nq_end
    mov dx, NET_BASE
    in  al, dx
    cmp al, 0DH
    je  nq_eol
    cmp al, 0AH
    je  nq_eol
    cmp al, 'a'
    jb  nq_store
    cmp al, 'z'
    ja  nq_store
    sub al, 20H                 ; upper-case
nq_store:
    mov bl, net_len
    cmp bl, NET_BUFSZ - 2
    jae nq_more                 ; line too long: drop extra bytes
    xor bh, bh
    mov net_buf[bx], al
    inc net_len
    jmp nq_more
nq_eol:
    cmp net_len, 0
    je  nq_more                 ; empty line (CR+LF pair)
    call NET_Exec
    mov net_len, 0
    mov net_got, 1
nq_more:
    loop nq_next
nq_end:
    mov al, net_got
    pop dx
    pop cx
    pop bx
    ret
NET_Poll ENDP
