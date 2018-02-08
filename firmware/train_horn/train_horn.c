//******************************************************************************
//  MSP430FR2000 - Train Horn
//
//  Description: Toggle pins at multiple frequencies using only TimerB,
//  mimic train horn for silly reasons.
//  P1.5 toggles using CRR0 and software. P1.6 and P1.7 toggle using CCR1 and CCR2
//  Button on P1.1 enables frequency generation until TimerB overflows
//  eCOMP to monitor Vbat (not setup currently, test mode only)
//
//  ACLK = TBCLK = 32768Hz, MCLK = SMCLK = default DCODIV ~1MHz
//  P1.5 = CCR0 ~ 32KHz/(2*66) = ~248.24Hz -> target frequency: 246.94Hz (B3)
//  P1.6 = CCR1 ~ 32KHz/(2*53) = ~309.13Hz -> target frequency: 311.13Hz (D#4)
//  P1.7 = CCR2 ~ 32KHz/(2*37) = ~442.81Hz -> target frequency: 440.00Hz (A4)
//
//                MSP430FR2000
//             -----------------
//         /|\|             P1.0|--> LED for timer status
//          | |             P1.1|<-- Button to gnd
//          | |          P1.2/C2|<-- V+ eCOMP input
//          | |             P1.5|--> ~248.24Hz
//          --|RST    P1.6/TB0.1|--> ~309.13Hz
//            |       P1.7/TB0.2|--> ~442.81Hz
//            |        P2.0/COUT|--> LED eCOMP status
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

    /* CANNOT GET IT TO WORK
    // Configure Comparator input & output
    P1SEL0 |= BIT2;                             // Select eCOMP input function on P1.2/C2
    P1SEL1 |= BIT3;
    P1SEL0 |= BIT3;                             // Select eCOMP input function on P1.3/C3
    P1SEL1 |= BIT3;
    //P2DIR  |= BIT0;                             // Select CPOUT function on P2.0/COUT
    //P2SEL1 |= BIT0;
    */

    // Configure Interrupt Button
    P1IES  |= BIT1;                             // play button interrupts on high-to-low transition
    P1REN  |= BIT1;                             // enable resistor, must set P1OUT for pullup
    P1OUT  |= BIT1;

    // Configure Timer_B Outputs
    P1SEL1 |= BIT6 | BIT7;                      // select TB0.1 and TB0.2 pin functions for P1.6 and P1.7
    P1DIR  |= BIT0 | BIT5 | BIT6 | BIT7;        // P1.0, P1.5, P1.6, and P1.7 outputs
    P1OUT  &= ~BIT0;                            // turn off LED on P1.0

    // Configure Unused GPIO
    P1DIR |= BIT3 | BIT4;
    P2DIR |= BIT1 | BIT6 | BIT7;

    PM5CTL0 &= ~LOCKLPM5;                       // Disable the GPIO power-on default high-impedance mode to activate
                                                // previously configured port settings
    /*
    // Configure reference
    PMMCTL0_H = PMMPW_H;                        // Unlock the PMM registers
    PMMCTL2 |= INTREFEN;                        // Enable internal reference
    while(!(PMMCTL2 & REFGENRDY));              // Poll till internal reference settles

    // Setup eCOMP
    CPCTL0 |= CPPSEL1;                          // Select C2 as input for V+ terminal
    CPCTL0 |= CPNSEL0 | CPNSEL1;                // Select C3 as input for V- terminal
    //CPCTL0 |= CPNSEL1 | CPNSEL2;                // Select DAC as input for V- terminal
    CPCTL0 |= CPPEN | CPNEN;                    // Enable eCOMP input
    //CPDACCTL |= CPDACREFS | CPDACEN;            // Select on-chip VREF and enable DAC
    //CPDACDATA |= 0x003f;                        // CPDACBUF1=On-chip VREF *32/64 = 0.75V
    CPCTL1 |= CPINV | CPIIE | CPIE;             // eCOMP inverted output, dual edge interrupt enabled
    CPCTL1 |= CPHSEL_1;                         // 10mV hysteresis mode
    CPCTL1 |= CPEN | CPMSEL;                    // enable eCOMP in low power mode
     */

    P1IFG = 0;                                  // clear any pending interrupts
    P1IE = BIT1;                                // enable interrupts on P1.1

    for(;;) {
        __bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, enable interrupts
        __no_operation();                       // For debug
    }
}

/*// eCOMP Interrupt Vector handler
#pragma vector=ECOMP0_VECTOR
__interrupt void ECOMP_ISR(void)
{
    switch(__even_in_range(CPIV, CPIV__CPIIFG))
    {
        case CPIV__NONE:
            break;
        case CPIV__CPIFG:
            P2OUT &= !BIT6;
            break;
        case CPIV__CPIIFG:
            P2OUT |= BIT6;
            break;
        default:
            break;
    }
}*/

// Port1 Interrupt Vector handler
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    P1IE = 0;                                   // disable any further interrupts
    P1OUT ^= BIT0;                              // toggle P1.0 LED for timer_b status

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
            P1OUT    ^= BIT0;                   // toggle P1.0 LED for timer_b status
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
