#include "pwm.h"

typedef struct pwm_pin_t {
	volatile uint8_t *port;
	uint8_t pin;
	uint16_t compare_value;
} pwm_pin_t;

typedef struct pwm_event_t {
	//Port masks
	uint8_t porta_mask;
	uint8_t portb_mask;
	uint8_t portc_mask;
	uint8_t portd_mask;

	//Only used in OCRB; ignored for high event in OCRA.
	uint16_t compare_value;			//This value of COMPB (the value that TCNT is now, when firing in OCRB interrupt).
} pwm_event_t;

static volatile uint8_t _set_phase = 0;							//Set to 1 when phase is updated
static volatile uint8_t _set_phase_lock = 0; 					//Set to 1 when we are in set_phase function.  Prevents OCRA from copying the double buffered pwm_events when this is 0.

static uint16_t _set_period = 0; 								//New period defined; set in set_period, and updated to OCRA in changed in COMPA interrupt

//Variables used to store config data
static pwm_pin_t _pwm_pins[PWM_MAX_PINS];				//Array of pins.  Index is important here, as that is how we refer to the pins from the outside world.
static uint8_t _count;											//How many pins should be used

static pwm_event_t _pwm_event_high;						//PWM event to set all pins high at start of period.  Calculated on a non-zero set_phase.
static pwm_event_t _pwm_event_high_new;					//Double buffer of pwm high event; copied to pwm_events in OCRA when _set_phase is non-zero.

static pwm_event_t _pwm_events_low[PWM_MAX_PINS + 1];	//Array of pwm events.  Each event will set one or more pins low.
static pwm_event_t _pwm_events_low_new[PWM_MAX_PINS + 1];//Double buffer of pwm events.  Calculated in each set_phase call; copied to pwm_events in OCRA when _set_phase is non-zero.
static volatile uint8_t _pwm_events_low_index;					//Current index of _pwm_events_low.  Reset in OCRA, incremented in OCRB.

static uint16_t _prescaler = 0x0;								//Numeric prescaler (1, 8, etc).  Required for _pwm_micros_to_clicks calls.
static uint8_t _prescaler_mask = 0x0;							//Prescaler mask corresponding to _prescaler

//Figure out which registers to use, depending on the chip in use
#if defined(__AVR_ATtiny25__)   || \
	defined(__AVR_ATtiny45__)   || \
	defined(__AVR_ATtiny85__)

#define PWM_8_BIT
#define TCCRA			TCCR0A
#define TCCRB			TCCR0B
#define OCRA			OCR0A
#define OCRB			OCR0B
#define TIMSKR			TIMSK
#define OCIEA			OCIE0A
#define OCIEB			OCIE0B
#define FIR				TIFR	//Force Interrupt Register
#define FOCA			OCF0A
#define TCNT			TCNT0

#elif defined(__AVR_ATmega168__)   || \
	defined(__AVR_ATmega328__)     || \
	defined(__AVR_ATmega328P__)    || \
	defined(__AVR_ATmega324P__)    || \
	defined(__AVR_ATmega644__)     || \
	defined(__AVR_ATmega644P__)    || \
	defined(__AVR_ATmega644PA__)   || \
	defined(__AVR_ATmega1284P__)

#define TCCRA			TCCR1A
#define TCCRB			TCCR1B
#define OCRA			OCR1A
#define OCRB			OCR1B
#define TIMSKR 			TIMSK1
#define OCIEA			OCIE1A
#define OCIEB			OCIE1B
#define FIR				TCCR1C
#define FOCA			FOC1A
#define TCNT			TCNT1

#else
	#error You must confirm PWM setup for your chip!  Please verify that MMCU is set correctly, and that there is a matching check definition in pwm.h
#endif

static uint16_t _pwm_micros_to_clicks(uint32_t micros){
	//There is a potential for an overflow here if micros * (F_CPU in MHz) does not fit into
	// a 32 bit variable.  If this happens, we can divide micros by prescaler before multiplying
	// with F_CPU, although this will give a loss of precision.  Leave as-is for now.
	return (((F_CPU / 1000000) * micros) / _prescaler) & 0xFFF0;
	
	//TODO Rounding, based on 10MHz clock and various prescalers:
	//Prescaler: 1, AND with 0xFF80
	//Prescaler: 8, AND with 0xFFF0
	//Prescaler: 64, AND with 0xFFFE
	//Other prescalers should be fine as-is...
	//More testing at different F_CPU values and prescalers are still required.
}

/*
 * Note: We extrapolate the DDR registers based off of the associated PORT 
 * register.  This assumes that the DDR registers come directly after the PORT
 * equivalents, with DDRX at the next address after PORTX.  This is valid for 
 * all the chips I have looked at; however, it is highly recommended that you 
 * check any new chips which you want to use this library with.
 */
void pwm_init(volatile uint8_t *ports[],
				uint8_t pins[],
				uint8_t count,
				uint32_t period) {

	_count = (count <= PWM_MAX_PINS ? count : PWM_MAX_PINS);
	
	//Store values
	for (uint8_t i = 0; i < _count; i++){
		_pwm_pins[i].port = ports[i];
		_pwm_pins[i].pin = pins[i];
		*(_pwm_pins[i].port - 0x1) |= _BV(_pwm_pins[i].pin);
	}
	
	//This is calculated by the focumula:
	// CUTOFF_VALUE = PRESCALER * MAX_VALUE / (F_CPU / 1000000)
	// where CUTOFF_VALUE is the period comparison for each if block,
	// PRESCALER is the prescaler in the datasheet for a given CSx selection,
	// and MAX_VALUE is the largest integer of the selected bit depth (256 / 65536).

#ifdef PWM_8_BIT
	uint32_t max_value = 255;
#else
	uint32_t max_value = 65535;
#endif
	
	if (period < (1 * max_value / (F_CPU / 1000000))){
		_prescaler = 1;
		_prescaler_mask = _BV(CS10);
	}
	else if (period < (8 * max_value / (F_CPU / 1000000))){
		_prescaler = 8;
		_prescaler_mask = _BV(CS11);	
	}
	else if (period < (64 * max_value / (F_CPU / 1000000))){
		_prescaler = 64;
		_prescaler_mask = _BV(CS11) | _BV(CS10);
	}
	else if (period < (256 * max_value / (F_CPU / 1000000))){
		_prescaler = 256;
		_prescaler_mask = _BV(CS12);
	}
	else if (period < (1024 * max_value / (F_CPU / 1000000))){
		_prescaler = 1024;
		_prescaler_mask = _BV(CS12) | _BV(CS10);
	}
	else {
		period = (1024 * max_value / (F_CPU / 1000000)); //The largest possible period
		_prescaler = 1024;
		_prescaler_mask = _BV(CS12) | _BV(CS10);
	}
				
	TCCRA = 0x00;
	TCCRB |= _prescaler_mask;
	
	//OCRA controls the PWM period
	OCRA = _pwm_micros_to_clicks(period);
	//OCRB controls the PWM phase.  It is initialized later.
	
	//Enable compare interrupt on both channels
	TIMSKR = _BV(OCIEA) | _BV(OCIEB);
	
	//Enable interrupts if the NO_INTERRUPT_ENABLE define is not set.  If it is, you need to call sei() elsewhere.
#ifndef NO_INTERRUPT_ENABLE
	sei();
#endif
	
	//Force interrupt on compare A initially
	FIR |= _BV(FOCA);
}

void pwm_start(){
	TCNT = 0x00;	//Restart timer counter
	TIMSKR = _BV(OCIEA) | _BV(OCIEB);	//Enable output compare match interrupt enable
	TCCRB |= _prescaler_mask;	//Enable 
}

void pwm_stop(){
	TCCRB = 0x00;
	TIMSKR |= _BV(OCIEA) | _BV(OCIEB);

	//Set pins low
	for (uint8_t i = 0; i < _count; i++){
		*(_pwm_pins[i].port) &= ~_BV(_pwm_pins[i].pin);
	}
}

//The comparison method used to sort pwm_pin variables
static int16_t _compare_values(const void *pin1, const void *pin2){
	 const pwm_pin_t *pwm1 = (const pwm_pin_t*) pin1;
	 const pwm_pin_t *pwm2 = (const pwm_pin_t*) pin2;
	 return pwm1->compare_value - pwm2->compare_value;
}

void pwm_set_phase(uint8_t index, uint32_t phase){
	_set_phase_lock = 1;

	//Bounds checking
	if (index >= _count) {
		_set_phase_lock = 0;
		return;
	}
	
	//The new compare value, in clock ticks
	uint16_t new_clicks = _pwm_micros_to_clicks(phase);
	pwm_pin_t *p = &(_pwm_pins[index]);
	
	//If there is no change from the old compare value to the new value, just exit and return, unlocking the mutex first
	if (p->compare_value == new_clicks){
		_set_phase_lock = 0;
		return;
	}
	
	//Set the new compare value
	p->compare_value = new_clicks;
	
	//Copy _pwm_pins to _pwm_pins_sorted, and then sort it by value
	pwm_pin_t _pwm_pins_sorted[PWM_MAX_PINS];
	for (uint8_t i = 0; i < _count; i++){
		_pwm_pins_sorted[i] = _pwm_pins[i];
	}
	qsort(_pwm_pins_sorted, _count, sizeof _pwm_pins_sorted[0], _compare_values);

	//Populate the _pwm_events_high variable and _pwm_events_low_new array, used in 
	// OCRA and OCRB respectively to turn pins on / off.
	//First we reset everything in this array...
	_pwm_event_high_new.porta_mask = 0x00;
	_pwm_event_high_new.portb_mask = 0x00;
	_pwm_event_high_new.portc_mask = 0x00;
	_pwm_event_high_new.portd_mask = 0x00;
	for (uint8_t i = 0; i < _count + 1; i++){
		_pwm_events_low_new[i].compare_value = 0xFFFF;
		_pwm_events_low_new[i].porta_mask = 0xFF;
		_pwm_events_low_new[i].portb_mask = 0xFF;
		_pwm_events_low_new[i].portc_mask = 0xFF;
		_pwm_events_low_new[i].portd_mask = 0xFF;
	}
	//... then we look through the sorted _pwm_pins list, and collect all of the ports / pins which are
	// set at the same compare values into a single event.
	uint16_t last_compare_value = _pwm_pins_sorted[0].compare_value;
	uint8_t last_index = 0;
	
	for (uint8_t i = 0; i < _count; i++){
		pwm_pin_t *p = &(_pwm_pins_sorted[i]);
		if (p->compare_value > last_compare_value){
			//We don't want an empty pwm_event at the beginning of the array, so verify whether last_compare_value was zero before incrementing
			if (last_compare_value > 0) {
				last_index++;
			}
			last_compare_value = p->compare_value;
		}

		if (p->compare_value > 0){
			//Set pins to high
			if (p->port == &PORTA) _pwm_event_high_new.porta_mask |= _BV(p->pin);
			else if (p->port == &PORTB) _pwm_event_high_new.portb_mask |= _BV(p->pin);
			else if (p->port == &PORTC) _pwm_event_high_new.portc_mask |= _BV(p->pin);
			else if (p->port == &PORTD) _pwm_event_high_new.portd_mask |= _BV(p->pin);

			//Set pins to low
			pwm_event_t *e = &(_pwm_events_low_new[last_index]);
			e->compare_value = last_compare_value;
			if (p->port == &PORTA) e->porta_mask &= ~_BV(p->pin);
			else if (p->port == &PORTB) e->portb_mask &= ~_BV(p->pin);
			else if (p->port == &PORTC) e->portc_mask &= ~_BV(p->pin);
			else if (p->port == &PORTD) e->portd_mask &= ~_BV(p->pin);
		}
	}

	//Signal OCRA that we are ready to load new values
	_set_phase = 1;
	_set_phase_lock = 0;
}

void pwm_set_period(uint32_t period){
	_set_period = _pwm_micros_to_clicks(period);
}



/* 
 * The frequency comparison.  When it overflows, we reset the timer to 0.
 */
#ifdef PWM_8_BIT
EMPTY_INTERRUPT(TIM0_OVF_vect)
ISR(TIM0_COMPA_vect){
#else
EMPTY_INTERRUPT(TIMER1_OVF_vect)
ISR(TIMER1_COMPA_vect){
#endif
	//Reset counter
	TCNT = 0;	

	//Update values if needed
	if (_set_phase && !_set_phase_lock){
		for (uint8_t i = 0; i < _count; i++){
			_pwm_events_low[i] = _pwm_events_low_new[i];
		}
		_pwm_event_high = _pwm_event_high_new;
		_set_phase = 0;
	}
	if (_set_period){
		OCRA = _set_period;
		_set_period = 0;
	}
	
	//Set to the first (sorted) compare value in the pwm_events_low array.  This is populated in set_phase.
	_pwm_events_low_index = 0;
	OCRB = _pwm_events_low[_pwm_events_low_index].compare_value;
	
	//Set pins high.  We do this after re-enabling the clock so that we do not artificially increase 
	// the phase.  We turn off the ports (in COMPB) in the same order that we turn them on here,
	// so that the delta of any delay between PORTA and PORTD should be reduced or eliminated.
#ifndef PWM_PORTA_UNUSED
	PORTA |= _pwm_event_high.porta_mask;
#endif
#ifndef PWM_PORTB_UNUSED
	PORTB |= _pwm_event_high.portb_mask;
#endif
#ifndef PWM_PORTC_UNUSED
	PORTC |= _pwm_event_high.portc_mask;
#endif
#ifndef PWM_PORTD_UNUSED
	PORTD |= _pwm_event_high.portd_mask;
#endif
}


/* 
 * The phase comparison.  When it overflows, we find the next highest value.
 */
#ifdef PWM_8_BIT
ISR(TIM0_COMPB_vect){
#else
ISR(TIMER1_COMPB_vect){
#endif
	pwm_event_t e = _pwm_events_low[_pwm_events_low_index];
	_pwm_events_low_index++;
	
#ifndef PWM_PORTA_UNUSED
	PORTA &= e.porta_mask;
#endif
#ifndef PWM_PORTB_UNUSED
	PORTB &= e.portb_mask;
#endif
#ifndef PWM_PORTC_UNUSED
	PORTC &= e.portc_mask;
#endif
#ifndef PWM_PORTD_UNUSED
	PORTD &= e.portd_mask;
#endif
	
	//Set the timer for the next lowest value.
	OCRB = _pwm_events_low[_pwm_events_low_index].compare_value;
}