#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

volatile uint32_t milliseconds = 0;

ISR(TIMER1_OVF_vect)
{
    milliseconds++;
}

uint32_t millis(void)
{
    uint32_t m;
    cli();
    m = milliseconds;
    sei();
    return m;
}

void init(void)
{
    DDRB |= (1<<PB1) | (1<<PB5); // D9 PWM, D13 LED
    DDRD |= (1<<PD2);            // Trigger output

    PORTB |= (1<<PB0);           // D8 pullup
    PORTD |= (1<<PD4);           // D4 pullup

    // ADC
    ADMUX = (1<<REFS0);

    ADCSRA = (1<<ADEN) |
             (1<<ADPS2) |
             (1<<ADPS1) |
             (1<<ADPS0);

    // PWM D9
    TCCR1A = (1<<COM1A1) | (1<<WGM10);
    TCCR1B = (1<<WGM12) | (1<<CS11);

    // Timer interrupt
    TIMSK1 = (1<<TOIE1);

    sei();
}

uint16_t readADC(uint8_t ch)
{
    ADMUX = (ADMUX & 0xF0) | ch;

    ADCSRA |= (1<<ADSC);

    while(ADCSRA & (1<<ADSC));

    return ADC;
}

uint16_t getDistance(void)
{
    PORTD &= ~(1<<PD2);
    _delay_us(2);

    PORTD |= (1<<PD2);
    _delay_us(10);
    PORTD &= ~(1<<PD2);

    while(!(PIND & (1<<PD3)));

    uint16_t count = 0;

    while(PIND & (1<<PD3))
    {
        count++;
        _delay_us(10);

        if(count > 3000)
            return 999;
    }

    return count / 58;
}

int main(void)
{
    init();

    uint8_t mode = 0;

    bool light = false;

    uint8_t bright = 0;

    uint32_t lastAct = 0;
    uint32_t lastMove = 0;

    while(1)
    {
        uint32_t now = millis();

        // Mode button
        if(!(PIND & (1<<PD4)))
        {
            _delay_ms(20);

            if(!(PIND & (1<<PD4)))
            {
                mode = (mode + 1) % 3;

                while(!(PIND & (1<<PD4)));
            }
        }

        switch(mode)
        {
            // Manual Mode
            case 0:

                light = !(PINB & (1<<PB0));

                if(light)
                {
                    bright = readADC(0) >> 2;
                    lastAct = now;
                }

                break;

            // Proximity Mode
            case 1:
            {
                uint16_t dist = getDistance();

                if(dist > 0 && dist < 100)
                {
                    light = true;
                    bright = 255;

                    lastMove = now;
                    lastAct = now;
                }

                if(light && (now - lastMove > 30000))
                {
                    light = false;
                }
            }
            break;

            // Daylight Mode
            case 2:
            {
                uint16_t ldr = readADC(1);

                if(ldr > 850)
                {
                    light = false;
                }
                else if(ldr < 300)
                {
                    light = true;
                    bright = 255;
                }
                else
                {
                    light = true;
                    bright = 255 - ((ldr - 300)/2);
                }

                if(light)
                    lastAct = now;
            }
            break;
        }

        OCR1A = light ? bright : 0;

        if(light)
            PORTB |= (1<<PB5);
        else
            PORTB &= ~(1<<PB5);

        _delay_ms(50);
    }
}