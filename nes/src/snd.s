; Moat NES: sound. Effects on pulse 2 and noise; a generative score (pulse 1 melody over a triangle bass) that follows the seasons.
        .export _snd_init, _snd_play, snd_tick, _mus_on, _mus_season
        .import _frames

        .segment "ZEROPAGE"
_mus_on:     .res 1
_mus_season: .res 1
mus_t:       .res 1     ; frames until the next step
mus_step:    .res 1
mus_deg:     .res 1
mus_vol:     .res 1
mus_seed:    .res 1
mus_bass:    .res 1
sfx_p:       .res 2     ; pointer into the running effect
sfx_t:       .res 1     ; frames left in the current step; 0 = idle
sfx_n:       .res 1
tmp1:        .res 1

        .segment "CODE"

_snd_init:
        lda #$0F
        sta $4015
        lda #$40
        sta $4017
        lda #$30
        sta $4000
        sta $4004
        sta $400C
        lda #$08
        sta $4001
        sta $4005
        lda #0
        sta sfx_t
        sta _mus_on
        sta mus_step
        sta mus_vol
        lda #$A5
        sta mus_seed
        lda #5
        sta mus_deg
        lda #1
        sta mus_t
        rts

; effect ids: 1 click, 2 build, 3 coin, 4 error, 5 bell, 6 horn, 7 fanfare, 8 chop
_snd_play:
        cmp #9
        bcs @out
        tax
        lda sfx_lo,x
        sta sfx_p
        lda sfx_hi,x
        sta sfx_p+1
        lda #1
        sta sfx_t
        lda #0
        sta sfx_n
@out:   rts

; an effect is: duty/volume byte, then steps (frames, period lo, period hi) ending with frames = 0; frames $F0+ means noise (period index in lo)
sfx_step:
        ldy sfx_n
        lda (sfx_p),y
        bne @go
        ; end
        lda #$30
        sta $4004
        lda #$30
        sta $400C
        lda #0
        sta sfx_t
        rts
@go:    sta sfx_t
        iny
        lda (sfx_p),y
        sta $4006
        iny
        lda (sfx_p),y
        iny
        sty sfx_n
        cmp #$FF
        beq @noise
        ora #$08
        sta $4007
        lda #$3A
        sta $4004
        rts
@noise: lda #$3C
        sta $400C
        lda $4006
        lda sfx_n
        sec
        sbc #2
        tay
        dey
        lda (sfx_p),y
        sta $400E
        lda #$08
        sta $400F
        rts

snd_tick:
        lda sfx_t
        beq @music
        dec sfx_t
        bne @music
        jsr sfx_step
@music:
        lda _mus_on
        beq @quiet
        ; the envelope of the melody note
        lda mus_vol
        beq @nov
        lda _frames
        and #1
        bne @nov
        dec mus_vol
@nov:   lda mus_vol
        ora #$B0
        sta $4000
        dec mus_t
        bne @done
        jsr mus_next
@done:  rts
@quiet: lda #$30
        sta $4000
        lda #$00
        sta $4008
        rts

; the next step of the score
mus_next:
        ldx _mus_season
        lda tempo,x
        sta mus_t
        ; random byte
        lda mus_seed
        asl a
        bcc @nx
        eor #$1D
@nx:    sta mus_seed
        sta tmp1
        ; the melody walks the scale, and rests now and then
        and #$07
        cmp #6
        bcs @rest
        lda tmp1
        lsr a
        lsr a
        lsr a
        and #3
        tay
        lda mus_deg
        clc
        adc walk,y
        bpl @pos
        lda #0
@pos:   cmp #10
        bcc @ok
        lda #9
@ok:    sta mus_deg
        ; degree -> semitones: scale[deg mod 5] + 12 * (deg div 5)
        ldy #0
@div:   cmp #5
        bcc @dd
        sbc #5
        iny
        bcs @div
@dd:    sta tmp1
        ; scale offset
        lda _mus_season
        and #2
        beq @major
        ldx tmp1
        lda minor,x
        jmp @addoct
@major: ldx tmp1
        lda major,x
@addoct:
        cpy #0
        beq @notoct
        clc
        adc #12
@notoct:
        ; plus the season's root
        ldx _mus_season
        clc
        adc root,x
        tax
        lda pul_lo,x
        sta $4002
        lda pul_hi,x
        ora #$08
        sta $4003
        lda #10
        sta mus_vol
@rest:
        ; the bass: a long triangle note every four steps
        inc mus_step
        lda mus_step
        and #3
        bne @nobass
        lda mus_step
        lsr a
        lsr a
        and #3
        tay
        lda bassoff,y
        ldx _mus_season
        clc
        adc root,x
        tax
        lda tri_lo,x
        sta $400A
        lda tri_hi,x
        sta $400B
        lda #$FF
        sta $4008
@nobass:
        rts

tempo:   .byte 8, 7, 9, 12
root:    .byte 27, 22, 24, 17       ; offsets into the note table (table starts at A1 = MIDI 33)
major:   .byte 0, 2, 4, 7, 9
minor:   .byte 0, 3, 5, 7, 10
walk:    .byte $FE, $FF, $01, $02
bassoff: .byte 0, 7, 5, 7

pul_lo: .byte $F1,$7F,$13,$AD,$4D,$F3,$9D,$4C,$00,$B8,$74,$34,$F8,$BF,$89,$56,$26,$F9,$CE,$A6,$80,$5C,$3A,$1A,$FB,$DF,$C4,$AB,$93,$7C,$67,$52,$3F,$2D,$1C,$0C,$FD,$EF,$E1,$D5,$C9,$BD,$B3,$A9,$9F,$96,$8E,$86,$7E
pul_hi: .byte $07,$07,$07,$06,$06,$05,$05,$05,$05,$04,$04,$04,$03,$03,$03,$03,$03,$02,$02,$02,$02,$02,$02,$02,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
tri_lo: .byte $F8,$BF,$89,$56,$26,$F9,$CE,$A6,$80,$5C,$3A,$1A,$FB,$DF,$C4,$AB,$93,$7C,$67,$52,$3F,$2D,$1C,$0C,$FD,$EF,$E1,$D5,$C9,$BD,$B3,$A9,$9F,$96,$8E,$86,$7E,$77,$70,$6A,$64,$5E,$59,$54,$4F,$4B,$46,$42,$3F
tri_hi: .byte $03,$03,$03,$03,$03,$02,$02,$02,$02,$02,$02,$02,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

; sound effects
sfx_lo: .byte 0, <fx_click, <fx_build, <fx_coin, <fx_error, <fx_bell, <fx_horn, <fx_fan, <fx_chop
sfx_hi: .byte 0, >fx_click, >fx_build, >fx_coin, >fx_error, >fx_bell, >fx_horn, >fx_fan, >fx_chop
fx_click:
        .byte 2, $6A, $00
        .byte 0
fx_build:
        .byte 4, $52, $01
        .byte 6, $D5, $00
        .byte 0
fx_coin:
        .byte 3, $A9, $00
        .byte 8, $70, $00
        .byte 0
fx_error:
        .byte 5, $F8, $03
        .byte 7, $74, $04
        .byte 0
fx_bell:
        .byte 12, $8E, $00
        .byte 20, $D5, $00
        .byte 0
fx_horn:
        .byte 14, $A6, $02
        .byte 24, $3A, $02
        .byte 0
fx_fan:
        .byte 5, $AB, $01
        .byte 5, $52, $01
        .byte 5, $1C, $01
        .byte 18, $D5, $00
        .byte 0
fx_chop:
        .byte 4, $06, $FF
        .byte 0
