/*
 * File:   main.c
 * Author: ujagaga@gmail.com
 *
 * Created on 12/23/2020 7:07:54 PM UTC
 * "Created in MPLAB Xpress"
 * The CPU should sleep and wake if there is a UART request. At request, 
 * it will save 4 bytes received or send them back. The UART command format is 
 * 4 bytes to mark message start, then command, 1 for read and 2 for write, then 4 bytes of data if write.
 */
 
#define F_CPU   1000000ul
#define BAUD 	        (300)
#define BAUD_PRESCALE 	((F_CPU / (BAUD * 16L)) - 1)


#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <util/delay.h>

#define BODS 7 //BOD Sleep bit in MCUCR
#define BODSE 2 //BOD Sleep enable bit in MCUCR

uint8_t rxBuf[16];
uint8_t wrIdx;
uint8_t rdIdx;
const uint8_t msgStart[] = {0xDE, 0xAD, 0xBE, 0xAF};
uint8_t save[4];

#define DATA_LEN()          (wrIdx - rdIdx)
#define MASK                (16)
#define CMD_RD              (1u)
#define CMD_WR              (2u)
#define CMD_SIZE            (1u)
#define SAVE_SIZE           (sizeof(save))
#define START_SIZE          (sizeof(msgStart))
#define RX_MAX_SIZE         (sizeof(rxBuf) - 1)

ISR(USART_RX_vect) {
    uint8_t rx = UDR;
    
    if(DATA_LEN() < RX_MAX_SIZE){ 
    	rxBuf[wrIdx & MASK] = rx;
    	wrIdx++;
    }
}

ISR(PCINT0_vect) {
    // This is called when the interrupt occurs, but I don't need to do anything in it
}

bool msgStartFound(){
    bool retVal = false;
    
    if(DATA_LEN() > START_SIZE){
        retVal = true;
        int i;
        for(i = 0; i < START_SIZE; i++){
            if(rxBuf[(rdIdx + i)  & MASK] != msgStart[i]){
                retVal = false;
                break;
            }
        }        
    }
    
    return retVal;
}


void sendSaved(){
    PORTD |= (1 << PD1);   // UART TX output
    
    int i;
    for(i = 0; i < SAVE_SIZE; i++){
        // wait for empty data register
        while (!(UCSRA & (1<<UDRE)));
        // set data into data register
        UDR = save[i];
    }
    
    PORTD &= ~(1 << PD1);  
}

void saveReceived(){
    // wait for message to save
    while(DATA_LEN() < SAVE_SIZE);
    
    int i;
    for(i = 0; i < SAVE_SIZE; i++){
        save[i] = rxBuf[rdIdx & MASK];
        rdIdx++;        
    }    
}

void setup() {			
	// Setup pin direction as input for all but UART TX
	DDRA = 0;
	DDRB = 0;
	DDRD = (1 << PD1);
	// Enable all pull up resistors
	PORTD = 0xff;   
	PORTB = 0xff;
	
	/* Uart init */
	UBRRL = (unsigned char)BAUD_PRESCALE;
	UBRRH = (BAUD_PRESCALE >> 8);
	UCSRB = (1<<RXEN) | (1 << TXEN) | (1 << RXCIE);
	
	wrIdx = 0;
	rdIdx = 0;	
   
    
    ACSR |= (1 << ACD);                 /* Disable Analog Comparator */
    GIMSK |= (1 << 5);                    // Enable Pin Change Interrupts
    PCMSK |= (1 << PCINT0);               // Use PB3 as interrupt pin
    
 
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
	
	sei();
}

int main(){ 
    
    setup();   
   
    
    while(1){         
        
        if(DATA_LEN() > START_SIZE){
            // Check if valid message start is found
            if(msgStartFound()){
                
                rdIdx += START_SIZE; // Fast forward pased message start to command byte
                
                uint8_t cmd = rxBuf[rdIdx & MASK];
                rdIdx++;
                
                if(cmd == CMD_RD){
                    sendSaved();
                }else if(cmd == CMD_WR){
                    saveReceived(); 
                }
                
                // Request compleated. Go to sleep.
                sleep_mode();
                
            }else{
                // Discard byte
                rdIdx++;   
            }            
        }                
    }
    return 0;   
}
