#include "Sample.h"

using namespace digitalcave;

//All the audio junk.  Static members of the class.

//Audio board control object
AudioControlSGTL5000 Sample::control;
uint8_t Sample::controlEnabled = 0;
uint8_t Sample::masterVolume = 0;

//I2S input
AudioInputI2S Sample::input;

//Mixer
AudioMixer16 Sample::mixer;

//Output
AudioOutputI2S Sample::output;

//Input passthrough to mixer
AudioConnection Sample::inputToMixer0(input, 0, mixer, SAMPLE_COUNT);
AudioConnection Sample::inputToMixer1(input, 1, mixer, SAMPLE_COUNT + 1);

//Mixer to output
AudioConnection Sample::mixerToOutput0(mixer, 0, output, 0);
AudioConnection Sample::mixerToOutput1(mixer, 0, output, 1);

//Initialize samples array
uint8_t Sample::currentIndex = 0;
Sample Sample::samples[SAMPLE_COUNT];

/***** Static methods *****/

uint8_t Sample::getMasterVolume(){
	return masterVolume;
}

void Sample::setMasterVolume(uint8_t volume){
	if (!controlEnabled){
		control.enable();
		controlEnabled = 1;
	}

	masterVolume = volume;
	control.volume(volume / 256.0);
}

Sample* Sample::findAvailableSample(uint8_t pad, uint8_t volume){
	//Oldest overall sample
	uint8_t oldestSample = 0;
	uint16_t oldestSamplePosition = 0;

	//Oldest sample played from the same pad, which was quieter than the new sound
	uint8_t oldestSampleByPad = 0;
	uint16_t oldestSampleByPadPosition = 0;
	uint8_t sampleByPadCount = 0;
	
	for (uint8_t i = 0; i < SAMPLE_COUNT; i++){
		if (samples[i].isPlaying() == 0){
			return &(samples[i]);	//If we have a sample object that is not playing currently, just use it.
		}
		else {
			if (samples[i].getPositionMillis() > oldestSamplePosition){
				oldestSamplePosition = samples[i].getPositionMillis();
				oldestSample = i;
			}
			if (samples[i].getLastPad() == pad
					&& samples[i].getVolume() < volume
					&& samples[i].getPositionMillis() > oldestSampleByPadPosition){
				oldestSampleByPadPosition = samples[i].getPositionMillis();
				oldestSampleByPad = i;
				sampleByPadCount++;
			}
		}
	}
	
	//If we get to here, there are no available channels, so we will have to stop a playing one.
	// We try to pick the one which will cause the lease disruption.
	if (sampleByPadCount >= 2){
		//If there are at least 2 samples already playing for this pad, we can kill the oldest.
		samples[oldestSampleByPad].stop();
		return &(samples[oldestSampleByPad]);
	}
	else {
		//If there are no free samples and not enough playing samples in the current channel, we 
		// just pick the oldest sound and stop it.
		samples[oldestSample].stop();
		return &(samples[oldestSample]);
	}
}

/***** Instance methods *****/

Sample::Sample(): 
		mixerChannel(currentIndex), 
		lastPad(0xFF), 
		playSerialRaw(),
		sampleToMixer(playSerialRaw, 0, mixer, currentIndex) {
	currentIndex++;	//Increment current index
}

void Sample::play(char* filename, uint8_t pad, uint8_t volume){
	Serial.println(filename);
	lastPad = pad;
	setVolume(volume);
	playSerialRaw.play(filename);
}

uint8_t Sample::isPlaying(){
	return playSerialRaw.isPlaying();
}

uint32_t Sample::getPositionMillis(){
	return playSerialRaw.isPlaying() ? playSerialRaw.positionMillis() : 0;
}

void Sample::stop(){
	playSerialRaw.stop();
}

uint8_t Sample::getVolume(){
	return volume;
}

void Sample::setVolume(uint8_t volume){
	this->volume = volume;
	mixer.gain(mixerChannel, max(volume / LINEAR_DIVISOR, pow(EXPONENTIAL_BASE, volume)) / VOLUME_DIVISOR);
}

uint8_t Sample::getLastPad(){
	return lastPad;
}