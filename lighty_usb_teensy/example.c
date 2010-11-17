/* Teensy RawHID example
 * http://www.pjrc.com/teensy/rawhid.html
 * Copyright (c) 2009 PJRC.COM, LLC
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above description, website URL and copyright notice and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Modified to provide PWM Motor control using Timer 1
//	11/9/2010     -jkl
// Fixed port definitions to comply with Atmel desires. 11/16/2010 -jkl
//	ADCs only read when asked.

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdlib.h> /* malloc, free */
#include <string.h> /* memcpy */
#include <util/delay.h>
#include "usb_rawhid.h"
#include "analog.h"
#include "example.h"

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

volatile uint8_t do_output=0;
uint8_t buffer[RAWHID_RX_SIZE];

/* unpack a buffer received from kernel land; inverts pack from kernel land
 *
 * @buf: buffer from kernel land, packed by
 * ../usb_driver/teensy.c:pack(), of size RAWHID_RX_SIZE
 *
 * @return: by value teensy_msg with malloc'd .buf: caller is expected
 * to FREE.
 */
struct teensy_msg unpack(uint8_t * buf) {
        struct teensy_msg msg = {
                .destination = buf[0],
                .size        = buf[1],
        };
        msg.buf = malloc(msg.size);
        if (!msg.buf) {
                /* TODO: fail spectacularly */
        }
        memcpy(msg.buf,buf,msg.size);
        return msg;
}

/* pack a teensy_msg for transmission to kernel land; to be inverted
 * by unpack() in kernel land
 *
 * @msg: msg.size must be <= RAWHID_TX_SIZE - 2
 *
 * @return: malloc'd buf: caller is expected to FREE.
 */
uint8_t * pack(struct teensy_msg msg) {
        uint8_t * buf = malloc(RAWHID_TX_SIZE);
        if (!buf) {
                /* TODO: fail spectacularly */
        }

        if (1+1+ msg.size > RAWHID_TX_SIZE) {
                /* TODO: fail spectacularly */
        }

        buf[0] = msg.destination;
        buf[1] = msg.size;
        memcpy(buf+2,msg.buf,msg.size);
        /* leaving garbage in the tail bytes ... */
        return buf;
}

/* provide power to PORTD2 for time @time ms */
/* TODO: generalize this to a macro */
void power_portd2(double time) {
        DDRD |= (1<<PORTD2);
        PORTD |= (1<<PORTD2);
        _delay_ms(time);
        PORTD &= ~(1<<PORTD2);
}

/* send @msg to kernel land teensy */
#define SEND_TIMEOUT 50
void send(struct teensy_msg msg) {
        uint8_t * buf = pack(msg);
        // send the packet
        usb_rawhid_send(buf, SEND_TIMEOUT);
        free(buf);
}

/* handler for adc msgs
 *
 * @msg: expects adc pin to read in msg.buf[0]
 *
 */
#define ADC_READ_SIZE 24 /* assumed EVEN */
void handle_adc(struct teensy_msg msg) {
        uint8_t i, pin = msg.buf[0]; /* TODO: use this */
        uint16_t val;
        /* TODO: use onboard light instead */
        power_portd2(500); /* power light for debug */

        if (msg.size < 1) {
                /* TODO: fail spectacularly */
        }

        msg.size = ADC_READ_SIZE;
        msg.buf = malloc(msg.size); /* caller still knows orig msg.buf */
        if (!msg.buf) {
                /* TODO: fail spectacularly */
        }

        // put A/D measurements into next 24 bytes
        for (i=0; i<ADC_READ_SIZE/2; i++) {
				val = analogRead(i);
				msg.buf[i * 2]     = val >> 8;
				msg.buf[i * 2 + 1] = val & 0xff;
        }
        send(msg);
        free(msg.buf);
}

void handle_mc(struct teensy_msg msg) {
        uint8_t speed = msg.buf[0],
                direction = msg.buf[1]; /* TODO: use this */
        char reply[] = "( , ) received in handle_mc()\0";
        reply[1] = speed; reply[3] = direction;

        /* TODO: use onboard light instead */
        power_portd2(500); /* power light for debug */

        /* validate input */
        if (msg.size < 1+1) {
                /* TODO: fail spectacularly */
        }
        if (direction != 'f' && direction != 'r') {
                /* TODO: fail spectacularly */
        }

        /* TODO: control motors */
        /*
				case 'a':		// Motors Fwd, 87.5%
					PORTD &= ~((1<<PORTD6) | (1<<PORTD7));	// turns off motors
					PORTC &= ~((1<<PORTC6) | (1<<PORTC7));
					OCR1A = 175;
					OCR1B = 175; 
					_delay_ms(500);
					PORTD |= (1<<PORTD6);	
					PORTC |= (1<<PORTC6);
					break;
				case 'b':		// Motors Fwd, 50%
					PORTD &= ~((1<<PORTD6) | (1<<PORTD7));	// turns off motors
					PORTC &= ~((1<<PORTC6) | (1<<PORTC7));
					OCR1A = 100;
					OCR1B = 100; 
					_delay_ms(500);
					PORTD |= (1<<PORTD6);	
					PORTC |= (1<<PORTC6);
					break;
				case 'c':		// Motors Rev, 50%
					PORTD &= ~((1<<PORTD6) | (1<<PORTD7));	// turns off motors
					PORTC &= ~((1<<PORTC6) | (1<<PORTC7));
					OCR1A = 100;
					OCR1B = 100; 
					_delay_ms(500);
					PORTD |= (1<<PORTD7);	
					PORTC |= (1<<PORTC7);
					break;
				case 'd':
					DDRD |= (1<<PORTD1);
					PORTD |= (1<<PORTD1);
					PORTD &= ~((1<<PORTD6) | (1<<PORTD7));	// turns off motors
					PORTC &= ~((1<<PORTC6) | (1<<PORTC7));
					OCR1A = 0;
					OCR1B = 0; 
					_delay_ms(500);
					PORTD &= ~(1<<PORTD1);
					_delay_ms(500);
					break;
        */

        msg.size = sizeof(reply);
        msg.buf = malloc(msg.size); /* caller still knows orig msg.buf */
        if (!msg.buf) {
                /* TODO: fail spectacularly */
        }
        memcpy(msg.buf,reply,msg.size);

        send(msg);
        free(msg.buf);
}

int main(void)
{
    int8_t r, i;
    struct teensy_msg msg;

	// set for 16 MHz clock
	CPU_PRESCALE(0);

	// Initialize the USB, and then wait for the host to set configuration.
	// If the Teensy is powered without a PC connected to the USB port,
	// this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

	// Wait an extra second for the PC's operating system to load drivers
	// and do whatever it does to actually be ready for input
	_delay_ms(1000);

// Set up Timer 1 for PWM control.
// P&F Correct, Runs 5KHz. Divide 16Mhz clock by 8, cycle = 200.
// Make OCR1A & B outputs.
	DDRB |= ((1<<PORTB5) | (1<<PORTB6));
	//TCCR1A = (1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0);
	TCCR1A = (1<<COM1A1)  | (1<<COM1B1) ; 
	TCCR1B = (1<<WGM13) | (1<<CS11);
	ICR1 = 200;
	OCR1A = 0;	// Should stay low to start
	OCR1B = 0; 

// Set up Control Signals. PD6&7 for OCR1B; PC6&7 for OCR1A.
	DDRD |= (1<<PORTD6) | (1<<PORTD7);
	DDRC |= (1<<PORTC6) | (1<<PORTC7);
	PORTD &= ~((1<<PORTD6) | (1<<PORTD7));	// take low to start (off)
	PORTC &= ~((1<<PORTC6) | (1<<PORTC7));

// Timer 0 is not necessary - leave code in case we later want it.
        // Configure timer 0 to generate a timer overflow interrupt every
        // 256*1024 clock cycles, or approx 61 Hz when using 16 MHz clock
//        TCCR0A = 0x00;
//        TCCR0B = 0x05;
 //       TIMSK0 = (1<<TOIE0);
// give a blink at the start
// Loop for debug
//while (1){

	DDRD |= (1<<PORTD3);
	PORTD |= (1<<PORTD3);
	_delay_ms(500);
	PORTD &= ~(1<<PORTD3);
	_delay_ms(500);
//}
	while (1) {
		// if received data, do something with it
		r = usb_rawhid_recv(buffer, 0);
        // TODO: extract message struct from buf and pass to
        // appropriate handler()
		if (r > 0) {
// PD4 --> Red 1
// PD6 --> Green 1
// PD7 --> Blue 1
// give a blink on packet received - uncomment for debug
/*
	DDRD |= (1<<PORTD3);
	PORTD |= (1<<PORTD3);
	_delay_ms(500);
	PORTD &= ~(1<<PORTD3);
	_delay_ms(500);	*/
            msg = unpack(buffer);
			switch(msg.destination){
            case 'e':
                    handle_adc(msg);
					break;
            case 'm':
                    handle_mc(msg);
                    break;
            default:
                    /* TODO: fail spectacularly */
					break;
			}
            free(msg.buf);
            _delay_ms(50);
		}
	}
}

// This interrupt routine is run approx 61 times per second.
/*  Note used currently
ISR(TIMER0_OVF_vect)
{
	static uint8_t count=0;

	// set the do_output variable every 2 seconds
	if (++count > 122) {
		count = 0;
		//do_output = 1;
	}
}*/

