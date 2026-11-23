; Moat NES: startup, interrupts, pad, PPU queue, sound. Mapper 1 (MMC1) with 8K battery WRAM.
        .import _main
        .importzp sp
        .export __STARTUP__ : absolute = 1
        .export _nmi_wait, _pad, _padnew, _frames, _ppu_mask, _vq_len, _scrx, _scry
        .export _snd_play, _mus_on, _mus_season, _snd_init, _set_bank, _cur_bank, _set_chr
        .export _oam, _vq
        .exportzp _zpt
        .import __STACKTOP__

PPUCTRL = $2000
PPUMASK = $2001
PPUSTAT = $2002
OAMADDR = $2003
PPUSCRL = $2005
PPUADDR = $2006
PPUDATA = $2007
OAMDMA  = $4014
JOY1    = $4016

        .segment "HEADER"
        .byte "NES", $1A, 8, 2, $12, $00, 0, 0, 0, 0, 0, 0, 0, 0   ; MMC1, battery, 128K PRG, 8K CHR

        .segment "ZEROPAGE"
_pad:      .res 1
_padnew:   .res 1
_frames:   .res 1
_ppu_mask: .res 1
_vq_len:   .res 1
_scrx:     .res 1
_scry:     .res 1
nmi_flag:  .res 1
_zpt:      .res 4
_cur_bank: .res 1
; sound
sfx_t:     .res 1       ; frames left of current effect
sfx_id:    .res 1
sfx_p:     .res 1
_mus_on:   .res 1
_mus_season: .res 1
mus_t:     .res 1
mus_n:     .res 1       ; step in bar
mus_seed:  .res 1
mus_lead:  .res 1
mus_vib:   .res 1
mus_bt:    .res 1

        .segment "OAM"
_oam:   .res 256
        .segment "VBUF"
_vq:    .res 256

        .segment "STARTUP"
reset:
        sei
        cld
        ldx #$40
        stx $4017
        ldx #$FF
        txs
        inx
        stx PPUCTRL
        stx PPUMASK
        stx $4010
        bit PPUSTAT
@w1:    bit PPUSTAT
        bpl @w1
        ; MMC1: reset shift register, then control = $0E (vertical, fix last PRG bank, 8K CHR)
        lda #$80
        sta $8000
        lda #$0E
        jsr mmc1_ctrl
        lda #0
        jsr mmc1_chr0
        lda #0
        jsr mmc1_prg          ; PRG bank 0, WRAM enabled
        ; clear internal RAM (not WRAM: it holds the saved realm)
        lda #0
        tax
@clr:   sta $0000,x
        sta $0100,x
        sta $0300,x
        sta $0400,x
        sta $0500,x
        sta $0600,x
        sta $0700,x
        inx
        bne @clr
        lda #$FF
        ldx #0
@oam:   sta $0200,x
        inx
        bne @oam
@w2:    bit PPUSTAT
        bpl @w2
        lda #<__STACKTOP__
        sta sp
        lda #>__STACKTOP__
        sta sp+1
        jsr _snd_init
        lda #$88
        sta PPUCTRL            ; NMI on, BG $0000, sprites $1000
        jmp _main

mmc1_ctrl:
        sta $8000
        lsr a
        sta $8000
        lsr a
        sta $8000
        lsr a
        sta $8000
        lsr a
        sta $8000
        rts
mmc1_chr0:
        sta $A000
        lsr a
        sta $A000
        lsr a
        sta $A000
        lsr a
        sta $A000
        lsr a
        sta $A000
        rts
_set_chr:
        sta $A000
        lsr a
        sta $A000
        lsr a
        sta $A000
        lsr a
        sta $A000
        lsr a
        sta $A000
        rts
_set_bank:
        sta _cur_bank
mmc1_prg:
        sta $E000
        lsr a
        sta $E000
        lsr a
        sta $E000
        lsr a
        sta $E000
        lsr a
        sta $E000
        rts

; wait for the next NMI (the frame's PPU work is done in it)
_nmi_wait:
        lda _frames
@w:     cmp _frames
        beq @w
        rts

nmi:
        pha
        txa
        pha
        tya
        pha
        lda _ppu_mask
        and #$18
        beq @norender          ; rendering off: the main code owns the PPU
        lda #0
        sta OAMADDR
        lda #>_oam
        sta OAMDMA
        ; flush the vram queue: [hi, lo, len, data...] ... ; hi=0 ends
        ldx #0
        cpx _vq_len
        beq @noq
@q:     lda _vq,x
        beq @qend
        sta PPUADDR
        inx
        lda _vq,x
        sta PPUADDR
        inx
        ldy _vq,x
        inx
@qb:    lda _vq,x
        sta PPUDATA
        inx
        dey
        bne @qb
        cpx _vq_len
        bcc @q
@qend:  lda #0
        sta _vq_len
        sta _vq
@noq:
        lda #0
        sta PPUADDR
        sta PPUADDR
        lda _scrx
        sta PPUSCRL
        lda _scry
        sta PPUSCRL
        lda #$88
        sta PPUCTRL
        lda _ppu_mask
        sta PPUMASK
@norender:
        ; pad
        lda _pad
        pha
        lda #1
        sta JOY1
        sta _pad
        lsr a
        sta JOY1
@rd:    lda JOY1
        lsr a
        rol _pad
        bcc @rd
        pla
        eor #$FF
        and _pad
        sta _padnew
        inc _frames
        jsr snd_tick
        pla
        tay
        pla
        tax
        pla
        rti
irq:
        rti

; ------------------------------------------------------------------ sound (stubs, replaced by snd.s)
_snd_init:
        rts
_snd_play:
        rts
snd_tick:
        rts

        .segment "VECTORS"
        .word nmi, reset, irq

        .segment "CHARS"
        .incbin "build/chr.bin"
