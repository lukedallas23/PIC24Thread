.include "xc.inc"

.text                       ;BP (put the following data in ROM(program memory))

; This is a library, thus it can *not* contain a _main function: the C file will
; define main().  However, we will need a .global statement to make available ASM
; functions to C code.
; All functions utilized outside of this file will need to have a leading 
; underscore (_) and be included in a comment delimited list below.
.global _push_to_stack, _pop_from_stack, _bra_to_address

; Places registers on the stack
_push_to_stack:
    PUSH W0
    PUSH W1
    PUSH W2
    MOV W15, W1	       ; Move SP to W1
    MOV W15, W2	       ; Move SP to W2
    SUB #10, W1        ; To get PC <15:0>
    ADD #28, W2	       ; To get new memory location on PC <15:0>
    MOV [W1++], [W2++] ; Move PC <15:0> to new TOS
    MOV [W1], [W2]     ; Move PC <22:16> to new TOS
    SUB #10, W15       ; New Stack Value
    PUSH W3
    PUSH W4
    ADD #6, W15	       ; Add 6 to SP, to bypass W0,1,2
    PUSH W5
    PUSH W6
    PUSH W7
    PUSH W8
    PUSH W9
    PUSH W10
    PUSH W11
    PUSH W12
    PUSH W13
    PUSH W14
    PUSH 0x0020     ; SPLIM
    PUSH 0x0032     ; TBLPAG
    PUSH 0x0034     ; RCOUNT
    PUSH 0x0042     ; SR
    ADD #4, W15	    ; Get PC at TOS 
    return

; Pops registers from the stack
_pop_from_stack:
    SUB #4, W15      ; To not POP PC
    POP 0x0042      ; SR
    POP 0x0034      ; RCOUNT
    POP 0x0032      ; TBLPAG
    POP 0x0020      ; SPLIM
    POP W14
    POP W13
    POP W12
    POP W11
    POP W10
    POP W9
    POP W8
    POP W7
    POP W6
    POP W5
    SUB #6, W15	    ; SKIP W0-W2
    POP W4
    POP W3
    ADD #10, W15    ; Move SP back
    MOV W15, W1	    ; Move SP to W1
    MOV W15, W2     ; Move SP to W2
    SUB #10, W1	    ; Move W1 to bottom of stack
    ADD #28, W2	    ; Move W2 to PC <15:0> 
    MOV [W2++], [W1++] ; Move PC <15:0>
    MOV [W2], [W1]  ; Move PC <22:16>
    POP W2
    POP W1
    POP W0
    return	    

; Branches to address in W13. This will be the PC of when the context was saved
; This register should be reverted using POP after context switch.
_bra_to_address:
    SUB #4, W15        ; This routine never returns, so the return address can be taken off the stack
    MOV 0x2E, W12      ; Move the PC into W12
    ADD #8, W12        ; Add to PC to account for offset
    SUB W13, W12, W13  ; Get the difference between new addr and PC
    ASR W13, #1, W13   ; Devide by two
    BRA W13
