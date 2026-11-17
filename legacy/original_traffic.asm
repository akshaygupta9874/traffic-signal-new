.MODEL SMALL
.STACK 100H

.DATA

    ;------------------------------------------------
    ; Text attributes (background 0 + foreground colour)
    ;------------------------------------------------
    ATT_WHITE   EQU 07H          ; housing / pole
    RED_ON      EQU 0CH          ; bright red
    RED_OFF     EQU 04H          ; dark red   (lamp off)
    YEL_ON      EQU 0EH          ; bright yellow
    YEL_OFF     EQU 06H          ; brown      (lamp off)
    GRN_ON      EQU 0AH          ; bright green
    GRN_OFF     EQU 02H          ; dark green (lamp off)

    ;------------------------------------------------
    ; Status messages
    ;------------------------------------------------
    MSG_GREEN  DB 'TRAFFIC LIGHT: GREEN   TIME: '
    LEN_GREEN  EQU $-MSG_GREEN

    MSG_YELLOW DB 'TRAFFIC LIGHT: YELLOW  TIME: '
    LEN_YELLOW EQU $-MSG_YELLOW

    MSG_RED    DB 'TRAFFIC LIGHT: RED     TIME: '
    LEN_RED    EQU $-MSG_RED

    ;------------------------------------------------
    ; Signal picture (RED on top, YELLOW middle, GREEN bottom)
    ; 0DBH = solid block character
    ;------------------------------------------------
    SIG_TOP    DB 13,10,'       +-------+',13,10
    LEN_TOP    EQU $-SIG_TOP

    SIG_GAP    DB '       |       |',13,10
    LEN_GAP    EQU $-SIG_GAP

    SIG_LEFT   DB '       |  '
    LEN_LEFT   EQU $-SIG_LEFT

    SIG_BULB   DB 0DBH,0DBH,0DBH
    LEN_BULB   EQU $-SIG_BULB

    SIG_RIGHT  DB '  |',13,10
    LEN_RIGHT  EQU $-SIG_RIGHT

    SIG_BOTTOM DB '       +-------+',13,10
    LEN_BOTTOM EQU $-SIG_BOTTOM

    SIG_POLE   DB '          |||',13,10
    LEN_POLE   EQU $-SIG_POLE

    ;------------------------------------------------
    ; Variables
    ;------------------------------------------------
    RED_ATT    DB RED_OFF        ; current colour of each lamp
    YEL_ATT    DB YEL_OFF
    GRN_ATT    DB GRN_OFF
    BULB_ATT   DB 07H
    CUR_ATTR   DB 07H
    NUM_ATT    DB 07H            ; colour used for the countdown digits
    NUM_ROW    DB 0              ; where the countdown digits are drawn
    NUM_COL    DB 0
    DIGIT_T    DB ' '
    DIGIT_U    DB '0'

.CODE

MAIN PROC

    MOV AX, @DATA
    MOV DS, AX

    ; 80x25 colour text mode, then hide the blinking cursor
    MOV AX, 0003H
    INT 10H
    MOV AH, 01H
    MOV CX, 2000H
    INT 10H

    ;================================================
    ; INITIALIZE 8255
    ; Port A   = 80H
    ; Port B   = 82H
    ; Port C   = 84H
    ; Control  = 86H
    ; Control word 80H -> Mode 0, ports as output
    ;================================================
    MOV DX, 86H
    MOV AL, 80H
    OUT DX, AL

TRAFFIC_LOOP:

;====================================================
; GREEN - 10 SECONDS
;====================================================
GREEN:
    MOV DX, 80H
    MOV AL, 01H          ; PA0 = 1 -> GREEN ON
    OUT DX, AL

    CALL SHOW_GREEN

    MOV CX, 10
GREEN_COUNT:
    CALL PRINT_NUMBER
    CALL DELAY_1SEC
    CALL CHECK_KEY
    LOOP GREEN_COUNT

;====================================================
; YELLOW - 3 SECONDS
;====================================================
YELLOW:
    MOV DX, 80H
    MOV AL, 02H          ; PA1 = 1 -> YELLOW ON
    OUT DX, AL

    CALL SHOW_YELLOW

    MOV CX, 3
YELLOW_COUNT:
    CALL PRINT_NUMBER
    CALL DELAY_1SEC
    CALL CHECK_KEY
    LOOP YELLOW_COUNT

;====================================================
; RED - 10 SECONDS
;====================================================
RED:
    MOV DX, 80H
    MOV AL, 04H          ; PA2 = 1 -> RED ON
    OUT DX, AL

    CALL SHOW_RED

    MOV CX, 10
RED_COUNT:
    CALL PRINT_NUMBER
    CALL DELAY_1SEC
    CALL CHECK_KEY
    LOOP RED_COUNT

    JMP TRAFFIC_LOOP

MAIN ENDP

;====================================================
; SHOW GREEN SCREEN
;====================================================
SHOW_GREEN PROC
    MOV RED_ATT, RED_OFF
    MOV YEL_ATT, YEL_OFF
    MOV GRN_ATT, GRN_ON
    CALL DRAW_SIGNAL

    MOV SI, OFFSET MSG_GREEN
    MOV CX, LEN_GREEN
    MOV BL, GRN_ON
    CALL PRINT_COLOR

    MOV BL, GRN_ON
    CALL SAVE_POS
    RET
SHOW_GREEN ENDP

;====================================================
; SHOW YELLOW SCREEN
;====================================================
SHOW_YELLOW PROC
    MOV RED_ATT, RED_OFF
    MOV YEL_ATT, YEL_ON
    MOV GRN_ATT, GRN_OFF
    CALL DRAW_SIGNAL

    MOV SI, OFFSET MSG_YELLOW
    MOV CX, LEN_YELLOW
    MOV BL, YEL_ON
    CALL PRINT_COLOR

    MOV BL, YEL_ON
    CALL SAVE_POS
    RET
SHOW_YELLOW ENDP

;====================================================
; SHOW RED SCREEN
;====================================================
SHOW_RED PROC
    MOV RED_ATT, RED_ON
    MOV YEL_ATT, YEL_OFF
    MOV GRN_ATT, GRN_OFF
    CALL DRAW_SIGNAL

    MOV SI, OFFSET MSG_RED
    MOV CX, LEN_RED
    MOV BL, RED_ON
    CALL PRINT_COLOR

    MOV BL, RED_ON
    CALL SAVE_POS
    RET
SHOW_RED ENDP

;====================================================
; DRAW_SIGNAL
; Draws the housing, the three lamps and the pole,
; using RED_ATT / YEL_ATT / GRN_ATT for the lamp colours.
;====================================================
DRAW_SIGNAL PROC
    CALL CLEAR_SCREEN

    MOV SI, OFFSET SIG_TOP
    MOV CX, LEN_TOP
    CALL PRINT_WHITE

    CALL DRAW_GAP
    MOV AL, RED_ATT
    CALL DRAW_BULB          ; RED    (top)
    CALL DRAW_GAP
    MOV AL, YEL_ATT
    CALL DRAW_BULB          ; YELLOW (middle)
    CALL DRAW_GAP
    MOV AL, GRN_ATT
    CALL DRAW_BULB          ; GREEN  (bottom)
    CALL DRAW_GAP

    MOV SI, OFFSET SIG_BOTTOM
    MOV CX, LEN_BOTTOM
    CALL PRINT_WHITE

    MOV SI, OFFSET SIG_POLE
    MOV CX, LEN_POLE
    CALL PRINT_WHITE
    MOV SI, OFFSET SIG_POLE
    MOV CX, LEN_POLE
    CALL PRINT_WHITE

    RET
DRAW_SIGNAL ENDP

;====================================================
; DRAW_GAP - empty row inside the housing
;====================================================
DRAW_GAP PROC
    MOV SI, OFFSET SIG_GAP
    MOV CX, LEN_GAP
    CALL PRINT_WHITE
    RET
DRAW_GAP ENDP

;====================================================
; DRAW_BULB - two rows of one lamp
; AL = lamp colour attribute
;====================================================
DRAW_BULB PROC
    MOV BULB_ATT, AL
    CALL BULB_ROW
    CALL BULB_ROW
    RET
DRAW_BULB ENDP

BULB_ROW PROC
    MOV SI, OFFSET SIG_LEFT
    MOV CX, LEN_LEFT
    CALL PRINT_WHITE

    MOV SI, OFFSET SIG_BULB
    MOV CX, LEN_BULB
    MOV BL, BULB_ATT
    CALL PRINT_COLOR

    MOV SI, OFFSET SIG_RIGHT
    MOV CX, LEN_RIGHT
    CALL PRINT_WHITE
    RET
BULB_ROW ENDP

;====================================================
; PRINT_COLOR
; DS:SI -> string
; CX    -> number of characters
; BL    -> text attribute
; INT 10H/09H does NOT move the cursor, so the cursor
; is advanced by hand after each character.
; CR / LF are sent through DOS so they act as controls.
;====================================================
PRINT_COLOR PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH BP

    MOV CUR_ATTR, BL
    MOV BP, CX

PC_LOOP:
    MOV AL, [SI]
    CMP AL, 13
    JE PC_CTRL
    CMP AL, 10
    JE PC_CTRL

    MOV AH, 09H             ; write char + colour at cursor
    MOV BH, 00H
    MOV BL, CUR_ATTR
    MOV CX, 1
    INT 10H

    MOV AH, 03H             ; read cursor -> DH=row, DL=col
    MOV BH, 00H
    INT 10H
    INC DL
    MOV AH, 02H             ; move cursor one column right
    MOV BH, 00H
    INT 10H
    JMP PC_NEXT

PC_CTRL:
    MOV DL, AL
    MOV AH, 02H
    INT 21H

PC_NEXT:
    INC SI
    DEC BP
    JNZ PC_LOOP

    POP BP
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
PRINT_COLOR ENDP

;====================================================
; PRINT_WHITE
; DS:SI -> string, CX -> number of characters
;====================================================
PRINT_WHITE PROC
    MOV BL, ATT_WHITE
    CALL PRINT_COLOR
    RET
PRINT_WHITE ENDP

;====================================================
; SAVE_POS
; Remembers the current cursor position (where the
; countdown digits go) and their colour (BL).
;====================================================
SAVE_POS PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX

    MOV NUM_ATT, BL

    MOV AH, 03H
    MOV BH, 00H
    INT 10H
    MOV NUM_ROW, DH
    MOV NUM_COL, DL

    POP DX
    POP CX
    POP BX
    POP AX
    RET
SAVE_POS ENDP

;====================================================
; PRINT_NUMBER
; CX = remaining seconds (10..1)
; Draws the number in the same colour as the lamp,
; always at the saved position (no cursor juggling).
;====================================================
PRINT_NUMBER PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX

    MOV AX, CX
    MOV BL, 10
    DIV BL                  ; AL = tens, AH = units
    ADD AL, '0'
    ADD AH, '0'
    MOV DIGIT_T, AL
    MOV DIGIT_U, AH
    CMP DIGIT_T, '0'
    JNE PN_DRAW
    MOV DIGIT_T, ' '        ; no leading zero

PN_DRAW:
    ; tens digit
    MOV AH, 02H
    MOV BH, 00H
    MOV DH, NUM_ROW
    MOV DL, NUM_COL
    INT 10H
    MOV AH, 09H
    MOV AL, DIGIT_T
    MOV BH, 00H
    MOV BL, NUM_ATT
    MOV CX, 1
    INT 10H

    ; units digit
    MOV AH, 02H
    MOV BH, 00H
    MOV DH, NUM_ROW
    MOV DL, NUM_COL
    INC DL
    INT 10H
    MOV AH, 09H
    MOV AL, DIGIT_U
    MOV BH, 00H
    MOV BL, NUM_ATT
    MOV CX, 1
    INT 10H

    POP DX
    POP CX
    POP BX
    POP AX
    RET
PRINT_NUMBER ENDP

;====================================================
; CLEAR SCREEN (scroll window) and home the cursor.
; Unlike a mode set, this keeps the cursor hidden.
;====================================================
CLEAR_SCREEN PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX

    MOV AX, 0600H
    MOV BH, 07H
    MOV CX, 0000H
    MOV DX, 184FH
    INT 10H

    MOV AH, 02H
    MOV BH, 00H
    MOV DX, 0000H
    INT 10H

    POP DX
    POP CX
    POP BX
    POP AX
    RET
CLEAR_SCREEN ENDP

;====================================================
; 1 SECOND DELAY USING DOS SYSTEM TIME
; INT 21H / AH=2CH returns current time:
; CH = hour, CL = minute, DH = second, DL = 1/100 sec
; We wait until the seconds field changes.
;====================================================
DELAY_1SEC PROC
    PUSH AX
    PUSH BX
    PUSH CX                 ; INT 21H/2CH overwrites CX (hour/minute)!
    PUSH DX

    MOV AH, 2CH
    INT 21H
    MOV BL, DH

WAIT_NEXT_SECOND:
    MOV AH, 2CH
    INT 21H
    CMP DH, BL
    JE WAIT_NEXT_SECOND

    POP DX
    POP CX
    POP BX
    POP AX
    RET
DELAY_1SEC ENDP

;====================================================
; CHECK_KEY - exit cleanly if any key is pressed
;====================================================
CHECK_KEY PROC
    PUSH AX
    MOV AH, 01H
    INT 16H                 ; ZF=1 -> no key waiting
    JZ CK_DONE
    MOV AH, 00H
    INT 16H                 ; consume the key
    CALL EXIT_PROGRAM       ; never returns
CK_DONE:
    POP AX
    RET
CHECK_KEY ENDP

;====================================================
; EXIT_PROGRAM - all lamps off, cursor back, return to DOS
;====================================================
EXIT_PROGRAM PROC
    MOV DX, 80H
    MOV AL, 00H             ; all lamps off on the 8255
    OUT DX, AL

    MOV AH, 01H             ; restore normal cursor
    MOV CX, 0607H
    INT 10H

    CALL CLEAR_SCREEN

    MOV AX, 4C00H
    INT 21H
EXIT_PROGRAM ENDP

END MAIN
