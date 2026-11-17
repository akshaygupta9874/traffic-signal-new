;====================================================================
; DOSINPUT.INC - keyboard (INT 16H). Returns upper-cased ASCII.
;====================================================================
.CODE
;--------------------------------------------------------------------
; DI_Poll   out: AL = key (letters upper-cased), 0 = none / extended key
;--------------------------------------------------------------------
DI_Poll PROC
    mov ah, 01H
    int 16H                     ; ZF=1 -> nothing waiting
    jz  dip_none
    mov ah, 00H
    int 16H                     ; consume
    cmp al, 'a'
    jb  dip_ret
    cmp al, 'z'
    ja  dip_ret
    sub al, 20H
dip_ret:
    ret
dip_none:
    xor al, al
    ret
DI_Poll ENDP
