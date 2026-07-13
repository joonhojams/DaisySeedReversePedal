#include "daisy_seed.h"
#include "daisysp.h"

// Interleaved audio definitions -> allows us to handle stereo audio in a single callback
// interleaved audio buffers are better optimized for hardware
#define LEFT (i)
#define RIGHT (i + 1)

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

// set to 48000 as default -> MUST MATCH sample rate set in main()
#define SAMPLE_RATE (48000)
#define DELAY_TIME (5.0f) // seconds

// REVERSE
// we will use a CIRCULAR buffer:
    // - think circular array, but ALSO sliding window
    // - there is a READ (delayed output) pointer/index and a WRITE (input) pointer/index
    // - both indices loop to start of buffer once end is reached
// Variables for REVERSE


size_t const revMaxLen = SAMPLE_RATE * DELAY_TIME; // max recording length (by setting to SAMPLE_RATE*5, we get 5 seconds)
float DSY_SDRAM_BSS buffer[revMaxLen]; // the buffer -> an array of floats stored in SDRAM (64 MB) instead of SRAM (512 KB)
size_t inputIndex = 0; // index of input signal in buffer, where audio is being WRITTEN
size_t reverseIndex = 0; // keeps track of how long user has held down the reverse switch for
char reverseOn = 0; // needs to be mapped to physical hardware input

// input - signal, preferrably the effected one, not the dry one
// toggle - 1 for WRITING FORWARDS, -1 for READING in REVERSE, 0 for OFF
static float reverse(float input, int toggle){
    float output;    
    if (toggle == 1) {
        // WRITE FORWARDS
        buffer[inputIndex] = input; // WRITE input signal into reverse buffer
		output = input; // leave input signal unaffected

		// increment inputIndex, and wrap around if it exceeds revMaxLen
        inputIndex++;
        inputIndex %= (int)revMaxLen; // ensures circular behavior
    } else if (toggle == -1){
        // READ BACKWARDS
        output = buffer[inputIndex]; // READ from saved buffer into output

		// decrement inputIndex, and wrap around if it goes below 0
        inputIndex--;
        inputIndex += revMaxLen; // since moving in negative direction
        inputIndex %= (int)revMaxLen; // ensures circular behavior
    }
    return output;
}

void AudioCallback(AudioHandle::InterleavingInputBuffer in, 
	AudioHandle::InterleavingOutputBuffer out, 
	size_t size)
{
	for (size_t i = 0; i < size; i+=2) {
		// stereo input/output, so we need to handle both channels
		if (reverseOn) {
			// if reverse is ON, we will WRITE to the buffer
			reverseIndex++;
			out[LEFT] = reverse(in[LEFT], 1);
			out[RIGHT] = reverse(in[RIGHT], 1);
		} else if (reverseIndex > 0) {
			// otherwise, if the reverseIndex is positive, we will READ from the buffer
			// reverseIndex is used to keep track of how long the user has held down the reverse switch for, so we can read back the correct amount of audio
			reverseIndex--;
			out[LEFT] = reverse(in[LEFT], -1);
			out[RIGHT] = reverse(in[RIGHT], -1);
		}
		else{
			out[LEFT] = in[LEFT];
			out[RIGHT] = in[RIGHT];
		}
	}
}	

int main(void) {
	hw.Init();
	memset(buffer, 0, sizeof(buffer)); // clear uninitialized SDRAM buffer
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ); // hardcoded sample rate of 48kHz
	hw.StartAudio(AudioCallback);
	while(1) {}
}
