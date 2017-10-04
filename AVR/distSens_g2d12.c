/************************************************************************/
// File:			distSens_g2d12.c
// Contributors:    Johannes Schrimpf, NTNU Fall 2007
//                  Jannicke Selnes Tusvik, NTNU Fall 2009
//                  Erlend Ese, NTNU Spring 2016
//
// Purpose:         Driver for infrared distance sensors: Sharp G2D12
//
//
// Port and pins defined in defines.h file
/************************************************************************/

/*  AVR includes    */
#include <avr/io.h>

/*  Custom includes    */
#include "distSens_g2d12.h"
#include "defines.h"

/************************************************************************/
// Array to map analog value to distance. Recalibration may be needed. 
// Sensors may be due for replacement by a LIDAR
/************************************************************************/
uint8_t ui8_analogToCM[4][256]={
    {   // Left sensor connected to PINA0 = 0
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,80,78,77,75,
        73,72,70,68,66,65,63,61,60,59,57,56,55,54,53,52,52,51,50,49,48,
        47,47,46,45,44,43,43,42,41,41,40,40,39,39,38,38,37,37,36,36,35,
        35,35,34,34,34,33,33,33,32,32,32,32,31,31,31,31,30,30,30,29,29,
        29,29,28,28,28,28,28,27,27,27,27,26,26,26,26,25,25,25,25,25,24,
        24,24,24,24,23,23,23,23,23,23,22,22,22,22,22,22,21,21,21,21,21,
        21,21,20,20,20,20,20,20,20,19,19,19,19,19,19,19,19,19,18,18,18,
        18,18,18,18,18,18,18,17,17,17,17,17,17,17,17,17,17,17,16,16,16,
        16,16,16,16,16,16,16,16,15,15,15,15,15,15,15,15,15,15,15,14,14,
        14,14,14,14,14,14,14,14,14,14,13,13,13,13,13,13,13,13,13,13,13,
        13,13,13,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
        12,12,12,12,12,12,11,11,11,11,11,11,11,10,10,10},
    { // Rear sensor connected to PINA1 = 1
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        80,77,75,72,70,68,66,64,63,61,60,59,58,56,55,54,53,53,52,51,50,
        49,48,48,47,46,45,45,44,43,43,42,41,41,40,39,39,38,38,37,37,36,
        36,35,35,35,34,34,33,33,33,32,32,31,31,31,30,30,30,29,29,29,28,
        28,28,28,27,27,27,26,26,26,26,25,25,25,25,25,24,24,24,24,24,23,
        23,23,23,23,22,22,22,22,22,22,21,21,21,21,21,21,20,20,20,20,20,
        20,20,19,19,19,19,19,19,19,19,18,18,18,18,18,18,18,18,18,17,17,
        17,17,17,17,17,17,17,16,16,16,16,16,16,16,16,16,16,15,15,15,15,
        15,15,15,15,15,14,14,14,14,14,14,14,14,14,14,13,13,13,13,13,13,
        13,13,13,13,13,12,12,12,12,12,12,12,12,12,12,12,11,11,11,11,11,
        11,11,11,11,11,11,11,11,11,11,11,10,10,10,10,10,10,10,10,10,10,
        10,10,10,10,10,10,10,10,10,10,10,10,10,10},
    { // Right sensor connected to PINA2 = 2
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,80,77,75,72,70,68,66,64,63,61,60,59,58,56,55,54,53,52,
        52,51,50,49,48,48,47,46,46,45,44,44,43,43,42,42,41,41,40,40,39,
        39,38,38,37,37,36,36,35,35,35,34,34,33,33,33,32,32,31,31,31,30,
        30,30,29,29,29,28,28,28,28,27,27,27,27,26,26,26,26,25,25,25,25,
        25,24,24,24,24,24,24,23,23,23,23,23,23,23,22,22,22,22,22,22,21,
        21,21,21,21,21,21,20,20,20,20,20,20,20,19,19,19,19,19,19,19,19,
        18,18,18,18,18,18,18,18,18,17,17,17,17,17,17,17,17,17,16,16,16,
        16,16,16,16,16,16,16,16,16,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,13,13,
        13,13,13,13,13,13,13,13,13,13,13,13,12,12,12,12,12,12,12,12,12,
        12,11,11,11,11,11,11,11,11,10,10,10,10},
    {// Forward sensor connected to PINA3 = 3
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,80,78,77,75,
        73,72,70,68,66,65,63,61,60,59,57,56,55,54,53,52,52,51,50,49,48,
        47,47,46,45,44,43,43,42,41,41,40,40,39,39,38,38,37,37,36,36,35,
        35,35,34,34,34,33,33,33,32,32,32,32,31,31,31,31,30,30,30,29,29,
        29,29,28,28,28,28,28,27,27,27,27,26,26,26,26,25,25,25,25,25,24,
        24,24,24,24,23,23,23,23,23,23,22,22,22,22,22,22,21,21,21,21,21,
        21,21,20,20,20,20,20,20,20,19,19,19,19,19,19,19,19,19,18,18,18,
        18,18,18,18,18,18,18,17,17,17,17,17,17,17,17,17,17,17,16,16,16,
        16,16,16,16,16,16,16,16,15,15,15,15,15,15,15,15,15,15,15,14,14,
        14,14,14,14,14,14,14,14,14,14,13,13,13,13,13,13,13,13,13,13,13,
        13,13,13,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
        12,12,12,12,12,12,11,11,11,11,11,11,11,10,10,10},
};

/* Initialize distance sensors and ADC */
void vDistSens_init(){
    /* Initialize sensor pins as input */
    distSensReg &= ~((1<<distSensFwd) & (1<<distSensLeft) & (1<<distSensRight) & (1<<distSensRear));
    
    /* Initialize common sensor power pin as output */
    distSensReg |= (1<<distSensPower);

    /* Turn off all sensor power */
    distSensPort &= ~(1<<distSensPower);
    
    /* Internal 2.56V VREG with external capacitor at AREF pin */
    /* Datasheet p255 table 21-3 */
    ADMUX |= (1<<REFS1) | (1<<REFS0);
    
    /* ADC enable */
    ADCSRA |= (1<<ADEN);
    
    /* ADC prescaler setting (div. factor = 16) */
    /* Datasheet p257 table 21-5 */
    ADCSRA |= (1<<ADPS2) | (0<<ADPS1) | (0<<ADPS0);
}

/* Turn on all distance sensors */
void vDistSens_On(){
    distSensPort |= (1<<distSensPower);
}

/* Turn off all distance sensors */
void vDistSens_Off(){
    distSensPort &= ~(1<<distSensPower);
}

/* Reads a value from the IR sensor ui8_num and returns a value in cm */
uint8_t ui8DistSens_readCM(uint8_t sensorDirection){
    
    uint8_t ui8_analogValue;
    
    /* Choose channel */
    ADMUX = sensorDirection;
    
    /* Enable internal 2,54V AREF */
    ADMUX |= (1<<REFS1) | (1<<REFS0);
    
    /* Start conversion */
    ADCSRA |= (1<<ADSC);
    loop_until_bit_is_clear(ADCSRA, ADSC); // Macro from <avr/io.h>, wait until bit bit in IO register is set.

    /* Return the 8 most significant bits from the 10 bit register */
    ui8_analogValue = (ADCL >> 2) | (ADCH << 6);
    
    // Returns corresponding distance in CM
    return ui8_analogToCM[sensorDirection][ui8_analogValue];
}

/* Reads a value from the IR sensor ui8_num and returns analog value */
uint8_t ui8DistSens_readAnalog(uint8_t distSensDir){
    
    uint16_t ui16_analogValue=0;
    
    /* Choose channel */
    ADMUX = distSensDir;
    
    /* Enable internal 2,54V AREF */
    ADMUX |= (1<<REFS1) | (1<<REFS0);

    /* Start conversion */
    ADCSRA |= (1<<ADSC);
    loop_until_bit_is_clear(ADCSRA, ADSC); // Macro from avr/io.h: Wait until bit bit in IO register sfr is clear.
    
    /* Return the 8 most significant bits from the 10 bit register */
    ui16_analogValue += (ADCL >> 2) | (ADCH << 6);
    return ((uint8_t)(ui16_analogValue));
}
