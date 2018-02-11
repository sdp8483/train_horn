//******************************************************************************
//  MSP430FR2000 - Train Horn
//
//  Description: Toggle pins at multiple frequencies using only TimerB,
//  mimic train horn for silly reasons.
//  P1.5 toggles using CRR0 and software. P2.0 and P2.1 toggle using CCR1 and CCR2
//  Button on P1.1 enables frequency generation until TimerB overflows
//  1.2 VREF+ output on pin 1.7 to connect to external comparator for LiPo voltage monitor,
//      could not get MSP430 internal comparator to work
//
//  ACLK = TBCLK = 32768Hz, MCLK = SMCLK = default DCODIV ~1MHz
//  P1.5 = CCR0 ~ 32KHz/(2*66) = ~248.24Hz -> target frequency: 246.94Hz (B3)
//  P2.0 = CCR1 ~ 32KHz/(2*53) = ~309.13Hz -> target frequency: 311.13Hz (D#4)
//  P2.1 = CCR2 ~ 32KHz/(2*37) = ~442.81Hz -> target frequency: 440.00Hz (A4)
//
//                MSP430FR2000
//             -----------------
//         /|\|             P1.0|--> LED for timer status
//          | |             P1.1|<-- Button to gnd
//          | |             P1.2|--> Audio Amp Shutdown
//          | |       P1.7/VREF+|--> 1.2VREF Output
//          | |             P1.5|--> ~248.24Hz
//          --|RST    P2.0/TB0.1|--> ~309.13Hz
//            |       P2.1/TB0.2|--> ~442.81Hz
//            |                 |
//
//
//   TimerB multifrequency based on TI app note SLAA513A and accompanying code
//******************************************************************************
#include <msp430.h>

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;                   // Stop WDT

    // Configure reset
    SFRRPCR |= SYSRSTRE | SYSRSTUP;             // Enable internal pullup resistor on reset pin

    // Configure Interrupt Button
    P1IES  |= BIT1;                             // play button interrupts on high-to-low transition
    P1REN  |= BIT1;                             // enable resistor, must set P1OUT for pullup
    P1OUT  |= BIT1;

    // Configure Audio Amp Shutdown
    P1DIR |= BIT2;                              // P1.2 control shutdown of audio amp
    P1OUT &= ~BIT2;                             // Pull low to turn off audio amp

    // Configure Timer_B Outputs
    P2SEL0 |= BIT0 | BIT1;                      // Select TB0.1 and TB0.2 pin functions for P2.0 and P2.1
    P2DIR |= BIT0 | BIT1;
    P1DIR |= BIT0 | BIT5;                       // P1.0 and P1.5 are outputs
    P1OUT  &= ~BIT0;                            // turn off LED on P1.0

    // Configure Voltage Reference Output
    P1SEL0 |= BIT7;                             // Select VREF+ pin function for P1.7
    P1SEL1 |= BIT7;

    // Configure Unused GPIO
    P1DIR |= BIT3 | BIT4 | BIT6;
    P2DIR |= BIT6 | BIT7;

    PM5CTL0 &= ~LOCKLPM5;                       // Disable the GPIO power-on default high-impedance mode to activate
                                                // previously configured port settings

    // Configure reference
    PMMCTL0_H = PMMPW_H;                        // Unlock the PMM registers
    PMMCTL2 |= EXTREFEN;                        // Enable external reference
    while(!(PMMCTL2 & REFGENRDY));              // Poll until internal reference settles

    P1IFG = 0;                                  // clear any pending interrupts
    P1IE = BIT1;                                // enable interrupts on P1.1

    for(;;) {
        __bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, enable interrupts
        __no_operation();                       // For debug
    }
}

// Port1 Interrupt Vector handler
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    P1IE = 0;                                   // disable any further interrupts
    P1OUT ^= BIT0 | BIT2;                       // toggle P1.0 LED for timer_b status
                                                // toggle P1.2 to turn audio amp on

    // setup timerB
    TB0CCTL0 = OUTMOD_4 + CCIE;                 // TBCCR0 toggle, interrupt enabled
    TB0CCTL1 = OUTMOD_4 + CCIE;                 // TBCCR1 toggle, interrupt enabled
    TB0CCTL2 = OUTMOD_4 + CCIE;                 // TBCCR2 toggle, interrupt enabled
    TB0CTL = TBSSEL_1 | MC_2 | TBCLR | TBIE;    // ACLK, continuous mode, clear TBR, enable interrupts
}

// Timer0_B3 Interrupt Vector (TBIV) handler
#pragma vector=TIMER0_B0_VECTOR
__interrupt void TIMER0_B0_ISR(void)
{

    TBCCR0 += 66;
    P1OUT ^= BIT5;                              // ~248.24Hz frequency generation on P1.5
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
            TBCCR2 += 37;                       // ~442.81Hz freguency generation on P1.7
            break;
        case TB0IV_TBIFG:                       // overflow
            P1OUT    ^= BIT0 | BIT2;            // toggle P1.0 LED for timer_b status
                                                // turn off audio amp on P1.2
            TB0CTL   &= ~(MC_2);                // disable Timer_B
            TB0CCTL0 = 0;                       // disable CCR0, CCR1, CCR2
            TB0CCTL1 = 0;
            TB0CCTL2 = 0;
            P1OUT    &= ~(BIT5 | BIT6 | BIT7);  // set pins low since they could have been high during interrupt

            // enable play button again
            P1IFG = 0;                          // acknowledge all interrupts
            P1IE  = BIT1;                        // enable interrupt on P1.1
            break;
        default: 
            break;
    }
}
