//******************************************************************************
//  MSP430FR2000 - Train Horn Hold To Play
//
//  Description: Toggle pins at multiple frequencies using only TimerB,
//  mimic train horn for silly reasons.
//  P1.5 toggles using CRR0 and software. P2.0 and P2.1 toggle using CCR1 and CCR2
//  Button on P1.1 enables frequency generation while held low
//  1.2 VREF+ output on pin 1.7 to connect to external comparator for LiPo voltage monitor,
//      could not get MSP430 internal comparator to work
//
//  ACLK = TBCLK = 32768Hz, MCLK = SMCLK = default DCODIV ~1MHz
//  P1.5 = CCR0 ~ 32KHz/(2*64) = ~256.00Hz -> target frequency: 255Hz (~C4)
//  P2.0 = CCR1 ~ 32KHz/(2*53) = ~309.13Hz -> target frequency: 311Hz (~D#4)
//  P2.1 = CCR2 ~ 32KHz/(2*37) = ~442.81Hz -> target frequency: 440Hz (A4)
//
//                MSP430FR2000
//             -----------------
//            |             P1.0|--> LED for timer status
//            |             P1.2|--> Audio Amp Shutdown
//            |             P1.3|<-- Play Button to gnd
//            |                 |
//            |       P1.7/VREF+|--> 1.2VREF Output
//            |                 |
//            |             P1.5|--> ~248.24Hz
//            |       P2.0/TB0.1|--> ~309.13Hz
//            |       P2.1/TB0.2|--> ~442.81Hz
//            |                 |
//
//
//   TimerB multi frequency based on TI app note SLAA513A and accompanying code
//******************************************************************************
#include <msp430.h>

// Port1 Pin Defines
#define PLAY_BTN    BIT3                        // Play button port 1 input
#define PLAY_LED    BIT0                        // Play LED port 1 output
#define AMP_EN      BIT2                        // Amplifier enable port 1 output

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;                   // Stop WDT

    // Configure reset
    SFRRPCR |= SYSRSTRE | SYSRSTUP;             // Enable internal pullup resistor on reset pin

    // Configure Interrupt Button
    P1IES  |= PLAY_BTN;                         // play button interrupts on high-to-low transition
    P1REN  |= PLAY_BTN;                         // enable internal resistor
    P1OUT  |= PLAY_BTN;                         // set resistor to pullup

    // Configure Audio Amp Shutdown
    P1DIR |= AMP_EN;                            // set as output
    P1OUT &= ~AMP_EN;                           // Pull low to turn off audio amp

    // Configure Play LED indicator
    P1DIR  |= PLAY_LED;                         // set as output
    P1OUT  &= ~PLAY_LED;                        // turn off LED

    // Configure Timer_B Outputs
    P2SEL0 |= BIT0 | BIT1;                      // Select TB0.1 and TB0.2 pin functions for P2.0 and P2.1
    P2DIR  |= BIT0 | BIT1;
    P1DIR  |= BIT5;                             // P1.5 is an output

    // Configure Voltage Reference Output
    P1SEL0 |= BIT7;                             // Select VREF+ pin function for P1.7
    P1SEL1 |= BIT7;

    // Configure Unused GPIO as outputs per ULP advice
    P1DIR |= BIT1 | BIT4 | BIT6;
    P2DIR |= BIT6 | BIT7;

    PM5CTL0 &= ~LOCKLPM5;                       // Disable the GPIO power-on default high-impedance mode to activate
                                                // previously configured port settings

    // Configure reference
    PMMCTL0_H = PMMPW_H;                        // Unlock the PMM registers
    PMMCTL2 |= EXTREFEN;                        // Enable external reference
    while(!(PMMCTL2 & REFGENRDY));              // Poll until internal reference settles

    P1IFG &= ~PLAY_BTN;                         // clear any pending interrupts
    P1IE |= PLAY_BTN;                           // enable interrupts on P1.1

    // setup timerB
    TB0CCTL0 = OUTMOD_4 + CCIE;                 // TBCCR0 toggle, interrupt enabled
    TB0CCTL1 = OUTMOD_4 + CCIE;                 // TBCCR1 toggle, interrupt enabled
    TB0CCTL2 = OUTMOD_4 + CCIE;                 // TBCCR2 toggle, interrupt enabled
    TB0CTL = TBSSEL_1 | TBCLR | TBIE;           // ACLK, continuous mode, clear TBR, enable interrupts

    for(;;) {
        __bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, enable interrupts

        TB0CTL  |= MC_2 | TBIE;                 // enable timer

        while (!(P1IN & PLAY_BTN));             // Poll until play button is released

        P1OUT ^= PLAY_LED | AMP_EN;             // toggle play LED for status indicator
                                                // turn off audio amp
        TB0CTL   &= ~MC_2;                      // disable Timer_B
        P1OUT    &= ~(BIT5 | BIT6 | BIT7);      // set pins low since they could have been high during interrupt

        // enable play button again
        P1IFG &= ~PLAY_BTN;                     // acknowledge all interrupts
        P1IE  |= PLAY_BTN;                      // enable interrupt on play button
    }
}

// Port1 Interrupt Vector handler
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    P1IE &= ~PLAY_BTN;                          // disable any further interrupts
    P1OUT ^= PLAY_LED | AMP_EN;                 // toggle play LED for status indicator
                                                // toggle audio amp on

    __bic_SR_register_on_exit(LPM3_bits);       // Exit LPM3
}

// Timer0_B3 Interrupt Vector (TBIV) handler
#pragma vector=TIMER0_B0_VECTOR
__interrupt void TIMER0_B0_ISR(void)
{

    TBCCR0 += 64;
    P1OUT ^= BIT5;                              // ~256Hz frequency generation on P1.5
}

// Timer0_B3 Interrupt Vector (TBIV) handler
#pragma vector=TIMER0_B1_VECTOR
__interrupt void TIMER0_B1_ISR(void)
{
    switch(__even_in_range(TB0IV,TB0IV_TBIFG))
    {
        case TB0IV_NONE:
            break;                              // No interrupt
        case TB0IV_TBCCR1:                      // CCR1
            TBCCR1 += 53;                       // ~309.13Hz frequency generation on P1.6
            break;
        case TB0IV_TBCCR2:                      // CCR2
            TBCCR2 += 37;                       // ~442.81Hz frequency generation on P1.7
            break;
        case TB0IV_TBIFG:                       // overflow
            break;
        default: 
            break;
    }
}
