/* Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md. */
/*
 * Application side of the CHGame boot-request handshake.
 *
 * The retained block lives in .boot_magic, which the application linker script
 * pins to CHGAME_MAGIC_ADDR and marks NOLOAD -- exactly as the bootloader's
 * script does -- so startup neither loads nor zeroes it and the value survives
 * NVIC_SystemReset(). Verified on hardware: SRAM does survive a warm reset on
 * this part, but NOT a power cycle, which is what we want. A marker that
 * outlived power loss could strand a device in the bootloader.
 */
#include "chgame_map.h"
#include "chgame_boot.h"
#include "ch32yyxx.h"

static volatile chgame_bootreq_t chgame_bootreq
    __attribute__((section(".boot_magic"), used));

void chgame_enter_bootloader(void)
{
    chgame_bootreq.magic   = CHGAME_BOOT_MAGIC;
    chgame_bootreq.inverse = (uint32_t)~CHGAME_BOOT_MAGIC;

    /* Leave the USB peripheral detached so the host sees a clean disconnect
       rather than a device that stops answering. */
    __disable_irq();
    NVIC_SystemReset();

    for (;;) { }
}

/* CHGAME: a crash: a fault, or an interrupt with no handler (HardFault_Handler
 * in ch32x035_it.c, while1_handler in ch32x035_misc.c). Interrupts stay off
 * from here, so the 1 kHz tick that steps a game's sound stops, and the timer
 * driving the piezo would hold the note that was sounding for ever. Instead
 * the piezo pin (PB10) becomes a plain output, low, which takes it from the
 * timer, and the status LED (PB9) stays on. Power off and on to go on.
 *
 * A debug build (-DCHGAME_DEBUG=1, the CHGame library's switch, which
 * build.extra_flags gives the core too) also keeps the crash at the bottom of
 * the stack (chgame_map.h), and a press of A (PB1) restarts the program,
 * straight past the menu, for the CHGame library's '!' command to report it.
 * ra and sp are as they were at the crash (HardFault_Handler passes them). */
extern volatile uint32_t CFGHR_tmpB;    /* GPIOB CFGHR is write-only: its shadow */
#if CHGAME_DEBUG
extern uint32_t _susrstack[], _eusrstack[];
extern void chgame_fault_park(void) __attribute__((weak, noreturn));
#endif

__attribute__((used, externally_visible, noreturn))
void chgame_fault(uint32_t ra, uint32_t sp)
{
#if CHGAME_DEBUG
    volatile uint32_t *rec = _susrstack;
    uint32_t v;
    __asm volatile ("csrr %0, 0x342" : "=r"(v)); rec[1] = v;   /* mcause */
    __asm volatile ("csrr %0, 0x341" : "=r"(v)); rec[2] = v;   /* mepc   */
    __asm volatile ("csrr %0, 0x343" : "=r"(v)); rec[3] = v;   /* mtval  */
    rec[4] = ra;
    rec[5] = sp;
    rec[0] = CHGAME_FAULT_MAGIC;
#else
    (void)ra; (void)sp;
#endif

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOB;
#if !defined(ARDUINO_CHGAME_WEACT_F8U6)
    CFGHR_tmpB = (CFGHR_tmpB & ~0xFF0u) | 0x330u;    /* PB9, PB10: push-pull outputs */
    GPIOB->CFGHR = CFGHR_tmpB;
    GPIOB->BCR  = 1u << 10;
    GPIOB->BSHR = 1u << 9;
#endif

#if CHGAME_DEBUG
    /* A sketch can go on answering its PC instead (the CHGame library's debug
       protocol: '!' reports this crash, 'B' goes to the bootloader): leave the
       trap for its chgame_fault_park(), interrupts on, on a fresh stack. A
       crash inside an interrupt handler may leave that interrupt's level
       blocked, and USB with it; A below is then the only way on. */
    if (chgame_fault_park) {
        __asm volatile (
            "csrw mepc, %0\n\t"
            "li t0, 0x1880\n\t"         /* MPP = Machine, MPIE: interrupts on after mret */
            "csrs mstatus, t0\n\t"
            "mv sp, %1\n\t"
            "mret"
            :: "r"(chgame_fault_park), "r"(_eusrstack) : "t0", "memory");
    }

    /* A, to GND with the pull-up: released, then pressed, each read steadily
       for a few ms so neither a hold from before the crash nor bounce counts. */
    GPIOB->CFGLR = (GPIOB->CFGLR & ~0xF0u) | 0x80u;
    GPIOB->BSHR = 1u << 1;
    for (uint32_t pressed = 0; pressed < 2; pressed++)
        for (uint32_t n = 0; n < 20000u; )
            n = ((GPIOB->INDR & 2u) == (pressed ? 0u : 2u)) ? n + 1u : 0u;

    chgame_bootreq.magic   = CHGAME_RUN_MAGIC;
    chgame_bootreq.inverse = (uint32_t)~CHGAME_RUN_MAGIC;
    NVIC_SystemReset();
#endif

    for (;;) { }
}
