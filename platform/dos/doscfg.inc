;====================================================================
; DOSCFG.INC - command line:  TRAFFIC [green yellow red]   (1..99 s)
; Either no arguments (defaults 10 3 10) or exactly three numbers.
;====================================================================
.DATA
psp_seg   DW 0
msg_usage DB 'Usage: TRAFFIC [green yellow red]   each 1..99 seconds', 13, 10
          DB 'Example: TRAFFIC 15 4 12', 13, 10, '$'

.CODE
;--------------------------------------------------------------------
; CFG_Parse - reads the PSP command tail (PSP:81H). out: CF=1 if invalid
;--------------------------------------------------------------------
CFG_Parse PROC
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov es, psp_seg
    mov si, 81H
    xor di, di                  ; number of values read so far
cp_next:
cp_skip:
    mov al, es:[si]
    cmp al, ' '
    je  cp_adv
    cmp al, 09H
    je  cp_adv
    jmp cp_test
cp_adv:
    inc si
    jmp cp_skip
cp_test:
    cmp al, 0DH
    je  cp_end
    xor bx, bx                  ; value
    xor cx, cx                  ; digit count
cp_dig:
    mov al, es:[si]
    sub al, '0'
    cmp al, 9
    ja  cp_enddig               ; not a digit
    xor ah, ah
    push ax
    mov ax, 10
    mul bx                      ; DX:AX = BX*10
    mov bx, ax
    pop ax
    add bx, ax
    inc cx
    inc si
    cmp cx, 3
    ja  cp_bad
    jmp cp_dig
cp_enddig:
    cmp cx, 0
    je  cp_bad                  ; token did not start with a digit
    mov al, es:[si]
    cmp al, ' '
    je  cp_store
    cmp al, 09H
    je  cp_store
    cmp al, 0DH
    jne cp_bad                  ; trailing garbage such as "12abc"
cp_store:
    cmp di, 3
    jae cp_bad                  ; more than three numbers
    mov ax, di
    call TE_SetDuration         ; AL = state, BX = seconds; CF if out of range
    jc  cp_bad
    inc di
    jmp cp_next
cp_end:
    cmp di, 0
    je  cp_ok
    cmp di, 3
    jne cp_bad
cp_ok:
    clc
    jmp cp_ret
cp_bad:
    stc
cp_ret:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
CFG_Parse ENDP
