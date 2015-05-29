#ifndef Life_H
#define Life_H

#include <stdint.h>
#include "matrix.h"

#define LIFE_HASH_COUNT			20
#define LIFE_MATCH_COUNT		20

namespace digitalcave {
	class Life {
	private:
		uint8_t** state;
		uint32_t* hashes;
		uint8_t matches = 0;
		uint8_t running = 0;
	
	public:
		void start();
		void stop();

	private:
		uint8_t getState(uint8_t x, uint8_t y);
		void setState(uint8_t x, uint8_t y, uint8_t value);

		/* clear the board */
		void clear();
	
		/* create a random board */
		void randomize();

		uint8_t getNeighborCount(uint8_t x, uint8_t y);
		uint32_t getStateHash();
		pixel_t translate(uint8_t state);
	
		/* write the board state to the matrix */
		void flush();
	
		/* remove hashes, clear the board, and set it to a random state */
		void reset();
	
		void run();
	};
}

#endif