#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>

#include "lib/serial/serial.h"
#include "lib/i2c/i2c_master.h"

#define SHIFT_PORT			PORTD
#define SHIFT_DATA_PIN		PIND5
#define SHIFT_CLOCK_PIN		PIND6
#define SHIFT_LATCH_PIN		PIND7

#define ADDRESS 			0x51

void shift_out(uint8_t data){
	for (int i = 0; i < 8; i++){
		//Clear the data pin first...
		SHIFT_PORT &= ~_BV(SHIFT_DATA_PIN);
		//... then set the bit (if appropriate).  We could probably
		// do this in one step, but this is more clear, plus speed is 
		// (probably) not critical here.
		SHIFT_PORT |= (((data >> (7 - i)) & 0x1) << SHIFT_DATA_PIN);
		
		//Pulse clock to shift in
		SHIFT_PORT &= ~_BV(SHIFT_CLOCK_PIN);
		SHIFT_PORT |= _BV(SHIFT_CLOCK_PIN);
	}
}

void shift_latch_data(){
	//Pulse latch to transfer contents to output
	SHIFT_PORT &= ~_BV(SHIFT_LATCH_PIN);	
	SHIFT_PORT |= _BV(SHIFT_LATCH_PIN);
}

/*
 * Shifts the given hours and minutes to the shift register, and latches the data.  The
 * hours and minutes given here are in BCD format (as given directly from the RTC module).
 * The cathode is either 1 or not 1 (pin 1 and 2), and maps to bit 16 and 15 respectively.
 */
uint16_t time_to_data(uint8_t hours, uint8_t minutes, uint8_t twenty_four_hour, uint8_t cathode){
	if (cathode != 1) cathode = 0;
	
	//We have four digits: H2, H1, M2, M1.  (H2 is MSB of Hour).  Each of these digits has
	// 7 segments: A .. F.  Each segment is lit by a (pin,pin) combination, with the pin
	// numbers mapping to the LED panel pins, with the first
	// pin being the cathode (pin 1 or 2), and the second pin being the anode (pin .  We 
	// map these combinations out below, above the defines for each digit.

	//Here we map between LED pins to shift register outputs, counting down from bit 14: 
	// LED		Shift Data		Shift Pin
	// 6		13				5
	// 7		12				4
	// 8		11				3
	// 9		10				2
	// 10		9				1
	// 12		8				15
	// 13		7				7
	// 15		6				6
	// 16		5				5
	// 17		4				4
	// 18		3				3
	// 19		2				2
	// 20		1				1
	// 21		0				15
	
	//    HourMinNumber_Segment_Cathode
	// H2: A=(1,7), B=(2,6), C=(2,9), D=(1,8), E=(2,8), G=(2,7) (24-hour not supported, since F is not implemented)	
	#define H2_A_1 	_BV(12)
	#define H2_A_2 	0
	#define H2_B_1 	0
	#define H2_B_2	_BV(13)
	#define H2_C_1	0
	#define H2_C_2	_BV(10)
	#define H2_D_1	_BV(11)
	#define H2_D_2	0
	#define H2_E_1	0
	#define H2_E_2	_BV(11)
	#define H2_F_1	0
	#define H2_F_2	0
	#define H2_G_1	0
	#define H2_G_2	_BV(12)
	
	// H1: A=(2,13), B=(2,10), C=(2,12), D=(1,12), E=(1,9), F=(1,13), G=(1,10)
	#define H1_A_1	0
	#define H1_A_2	_BV(7)
	#define H1_B_1	0
	#define H1_B_2	_BV(9)
	#define H1_C_1	0
	#define H1_C_2	_BV(8)
	#define H1_D_1	_BV(8)
	#define H1_D_2	0
	#define H1_E_1	_BV(10)
	#define H1_E_2	0
	#define H1_F_1	_BV(7)
	#define H1_F_2	0
	#define H1_G_1	_BV(9)
	#define H1_G_2	0
	
	// M2: A=(1,15), B=(1,16), C=(1,17), F=(2,15), G=(2,16), D=(2,17), E=(2,18)
	#define M2_A_1	_BV(6)
	#define M2_A_2	0
	#define M2_B_1	_BV(5)
	#define M2_B_2	0
	#define M2_C_1	_BV(4)
	#define M2_C_2	0
	#define M2_D_1	0
	#define M2_D_2	_BV(4)
	#define M2_E_1	0
	#define M2_E_2	_BV(3)
	#define M2_F_1	0
	#define M2_F_2	_BV(6)
	#define M2_G_1	0
	#define M2_G_2	_BV(5)

	// M1: E=(1,18), G=(1,19), D=(1,20), F=(1,21), B=(2,19), C=(2,20), A=(2,21)
	#define M1_A_1	0
	#define M1_A_2	_BV(0)
	#define M1_B_1	0
	#define M1_B_2	_BV(2)
	#define M1_C_1	0
	#define M1_C_2	_BV(1)
	#define M1_D_1	_BV(1)
	#define M1_D_2	0
	#define M1_E_1	_BV(3)
	#define M1_E_2	0
	#define M1_F_1	_BV(0)
	#define M1_F_2	0
	#define M1_G_1	_BV(2)
	#define M1_G_2	0

	
	uint16_t data = 0x0000;

	//Set the cathode to use
	if (cathode) data |= _BV(15);
	else data |= _BV(14);

	//Adjust times
	if (twenty_four_hour){
		if (hours >= 0x24) hours = 0;
	}
	else {
		if (hours == 0x0) hours = 0x12;	//0 == midnight
		if (hours > 0x12) hours = hours - 0x12; //Convert to 12 hour format
	}
	
	if (minutes > 0x59) minutes = 0;
	
	//Set MSB Hour
	switch (hours >> 4) {
		case 1:
			if (cathode) data |= H2_B_1 | H2_C_1;
			else data |= H2_B_2 | H2_C_2;
			break;
			
		case 2:
			if (cathode) data |= H2_A_1 | H2_B_1 | H2_D_1 | H2_E_1 | H2_G_1;
			else data |= H2_A_2 | H2_B_2 | H2_D_2 | H2_E_2 | H2_G_2;
			break;
	}
	
	//Set LSB hour
	switch (hours & 0xF) {
		case 0:
			if (cathode) data |= H1_A_1 | H1_B_1 | H1_C_1 | H1_D_1 | H1_E_1 | H1_F_1;
			else data |= H1_A_2 | H1_B_2 | H1_C_2 | H1_D_2 | H1_E_2 | H1_F_2;
			break;

		case 1:
			if (cathode) data |= H1_B_1 | H1_C_1;
			else data |= H1_B_2 | H1_C_2;
			break;
			
		case 2:
			if (cathode) data |= H1_A_1 | H1_B_1 | H1_D_1 | H1_E_1 | H1_G_1;
			else data |= H1_A_2 | H1_B_2 | H1_D_2 | H1_E_2 | H1_G_2;
			break;

		case 3:
			if (cathode) data |= H1_A_1 | H1_B_1 | H1_C_1 | H1_D_1 | H1_G_1;
			else data |= H1_A_2 | H1_B_2 | H1_C_2 | H1_D_2 | H1_G_2;
			break;

		case 4:
			if (cathode) data |= H1_B_1 | H1_C_1 | H1_F_1 | H1_G_1;
			else data |= H1_B_2 | H1_C_2 | H1_F_2 | H1_G_2;
			break;

		case 5:
			if (cathode) data |= H1_A_1 | H1_C_1 | H1_D_1 | H1_F_1 | H1_G_1;
			else data |= H1_A_2 | H1_C_2 | H1_D_2 | H1_F_2 | H1_G_2;
			break;

		case 6:
			if (cathode) data |= H1_A_1 | H1_C_1 | H1_D_1 | H1_E_1 | H1_F_1 | H1_G_1;
			else data |= H1_A_2 | H1_C_2 | H1_D_2 | H1_E_2 | H1_F_2 | H1_G_2;
			break;

		case 7:
			if (cathode) data |= H1_A_1 | H1_B_1 | H1_C_1;
			else data |= H1_A_2 | H1_B_2 | H1_C_2;
			break;

		case 8:
			if (cathode) data |= H1_A_1 | H1_B_1 | H1_C_1 | H1_D_1 | H1_E_1 | H1_F_1 | H1_G_1;
			else data |= H1_A_2 | H1_B_2 | H1_C_2 | H1_D_2 | H1_E_2 | H1_F_2 | H1_G_2;
			break;

		case 9:
			if (cathode) data |= H1_A_1 | H1_B_1 | H1_C_1 | H1_D_1 | H1_F_1 | H1_G_1;
			else data |= H1_A_2 | H1_B_2 | H1_C_2 | H1_D_2 | H1_F_2 | H1_G_2;
			break;
	}
			
	//Set MSB minute
	switch (minutes >> 4) {
		case 0:
			if (cathode) data |= M2_A_1 | M2_B_1 | M2_C_1 | M2_D_1 | M2_E_1 | M2_F_1;
			else data |= M2_A_2 | M2_B_2 | M2_C_2 | M2_D_2 | M2_E_2 | M2_F_2;
			break;

		case 1:
			if (cathode) data |= M2_B_1 | M2_C_1;
			else data |= M2_B_2 | M2_C_2;
			break;
			
		case 2:
			if (cathode) data |= M2_A_1 | M2_B_1 | M2_D_1 | M2_E_1 | M2_G_1;
			else data |= M2_A_2 | M2_B_2 | M2_D_2 | M2_E_2 | M2_G_2;
			break;

		case 3:
			if (cathode) data |= M2_A_1 | M2_B_1 | M2_C_1 | M2_D_1 | M2_G_1;
			else data |= M2_A_2 | M2_B_2 | M2_C_2 | M2_D_2 | M2_G_2;
			break;

		case 4:
			if (cathode) data |= M2_B_1 | M2_C_1 | M2_F_1 | M2_G_1;
			else data |= M2_B_2 | M2_C_2 | M2_F_2 | M2_G_2;
			break;

		case 5:
			if (cathode) data |= M2_A_1 | M2_C_1 | M2_D_1 | M2_F_1 | M2_G_1;
			else data |= M2_A_2 | M2_C_2 | M2_D_2 | M2_F_2 | M2_G_2;
			break;

		case 6:
			if (cathode) data |= M2_A_1 | M2_C_1 | M2_D_1 | M2_E_1 | M2_F_1 | M2_G_1;
			else data |= M2_A_2 | M2_C_2 | M2_D_2 | M2_E_2 | M2_F_2 | M2_G_2;
			break;

		case 7:
			if (cathode) data |= M2_A_1 | M2_B_1 | M2_C_1;
			else data |= M2_A_2 | M2_B_2 | M2_C_2;
			break;

		case 8:
			if (cathode) data |= M2_A_1 | M2_B_1 | M2_C_1 | M2_D_1 | M2_E_1 | M2_F_1 | M2_G_1;
			else data |= M2_A_2 | M2_B_2 | M2_C_2 | M2_D_2 | M2_E_2 | M2_F_2 | M2_G_2;
			break;

		case 9:
			if (cathode) data |= M2_A_1 | M2_B_1 | M2_C_1 | M2_D_1 | M2_F_1 | M2_G_1;
			else data |= M2_A_2 | M2_B_2 | M2_C_2 | M2_D_2 | M2_F_2 | M2_G_2;
			break;
	}
			
	//Set LSB minute
	switch (minutes & 0x0F) {
		case 0:
			if (cathode) data |= M1_A_1 | M1_B_1 | M1_C_1 | M1_D_1 | M1_E_1 | M1_F_1;
			else data |= M1_A_2 | M1_B_2 | M1_C_2 | M1_D_2 | M1_E_2 | M1_F_2;
			break;

		case 1:
			if (cathode) data |= M1_B_1 | M1_C_1;
			else data |= M1_B_2 | M1_C_2;
			break;
			
		case 2:
			if (cathode) data |= M1_A_1 | M1_B_1 | M1_D_1 | M1_E_1 | M1_G_1;
			else data |= M1_A_2 | M1_B_2 | M1_D_2 | M1_E_2 | M1_G_2;
			break;

		case 3:
			if (cathode) data |= M1_A_1 | M1_B_1 | M1_C_1 | M1_D_1 | M1_G_1;
			else data |= M1_A_2 | M1_B_2 | M1_C_2 | M1_D_2 | M1_G_2;
			break;

		case 4:
			if (cathode) data |= M1_B_1 | M1_C_1 | M1_F_1 | M1_G_1;
			else data |= M1_B_2 | M1_C_2 | M1_F_2 | M1_G_2;
			break;

		case 5:
			if (cathode) data |= M1_A_1 | M1_C_1 | M1_D_1 | M1_F_1 | M1_G_1;
			else data |= M1_A_2 | M1_C_2 | M1_D_2 | M1_F_2 | M1_G_2;
			break;

		case 6:
			if (cathode) data |= M1_A_1 | M1_C_1 | M1_D_1 | M1_E_1 | M1_F_1 | M1_G_1;
			else data |= M1_A_2 | M1_C_2 | M1_D_2 | M1_E_2 | M1_F_2 | M1_G_2;
			break;

		case 7:
			if (cathode) data |= M1_A_1 | M1_B_1 | M1_C_1;
			else data |= M1_A_2 | M1_B_2 | M1_C_2;
			break;

		case 8:
			if (cathode) data |= M1_A_1 | M1_B_1 | M1_C_1 | M1_D_1 | M1_E_1 | M1_F_1 | M1_G_1;
			else data |= M1_A_2 | M1_B_2 | M1_C_2 | M1_D_2 | M1_E_2 | M1_F_2 | M1_G_2;
			break;

		case 9:
			if (cathode) data |= M1_A_1 | M1_B_1 | M1_C_1 | M1_D_1 | M1_F_1 | M1_G_1;
			else data |= M1_A_2 | M1_B_2 | M1_C_2 | M1_D_2 | M1_F_2 | M1_G_2;
			break;
		
	}
	
	
	return data;
}

void get_current_time(uint8_t *hours, uint8_t *minutes){
	uint8_t message[4];
	
	//Reset the register to reading time
	message[0] = ADDRESS << 1 | I2C_WRITE;
	message[1] = 0x02; //Reset register pointer to 0x02
	i2c_start_transceiver_with_data(message, 2);

	message[0] = ADDRESS << 1 | I2C_READ;
	i2c_start_transceiver_with_data(message, 4);
	i2c_get_data_from_transceiver(message, 4);
	
	*hours = message[3];
	*minutes = message[2];
}

int main (void){
	serial_init_b(9600);
	i2c_master_init(100);
	
	//Enable outputs for shift register (clock LED driver)
	DDRD |= _BV(SHIFT_DATA_PIN) | _BV(SHIFT_CLOCK_PIN) | _BV(SHIFT_LATCH_PIN);

	uint8_t cathode = 0;
	char temp[32];
	uint16_t data1 = 0;
	uint16_t data2 = 0;
	uint8_t hours = 0;
	uint8_t minutes = 0;
	
	while (1){
		get_current_time(&hours, &minutes);
		
		data1 = time_to_data(hours, minutes, 0, 1);
		data2 = time_to_data(hours, minutes, 0, 0);
		
		//Display refresh loop
		for(int i = 0; i < 500; i++){
			if (cathode){
				shift_out(data1 >> 8);
				shift_out(data1 & 0xFF);		
			}
			else {
				shift_out(data2 >> 8);
				shift_out(data2 & 0xFF);
			}
			
			shift_latch_data();
			_delay_ms(1);
			cathode = ~cathode;		
		}
		/*
		minutes++;
		if ((minutes & 0xF) >= 0xA) minutes += 6;
		if (minutes > 0x59) {
			minutes = 0;
			hours++;
		}
		if ((hours & 0xF) >= 0xA) hours += 6;
		if (hours >= 0x24){
			hours = 0;
			minutes = 0;
		}
		*/
	}
}

