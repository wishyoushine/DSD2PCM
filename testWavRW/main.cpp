﻿// testWavRW.cpp: 定义控制台应用程序的入口点。
//
#define DR_WAV_IMPLEMENTATION

#define JAVA_VERSION

#include <stdio.h>
#include <stdint.h>
#include "wavfile.h"

#include "dsdfile.h"
#include "state2.h"
#include "fir_halfband.h"
#include <time.h>
// ref
//	https://code.google.com/archive/p/dsd2pcm    dsd2pcm C version
// dsd2pcm java version

//	stm32f7player (ffmpeg)
//	https://github.com/Kharabadze/DSD4Winamp	
//	https://github.com/amikey/SFBAudioEngine/tree/8e69d2590ea109879cc31b31429a4f57b4f352ef/Decoders
//	https://github.com/clivem/dsdplay/blob/master/src/dsdplay.c

// Samples are downloaded from:
// https://samplerateconverter.com/content/free-samples-dsf-audio-files  dsf demo files.
// https://www.oppodigital.com/hra/dsd-by-davidelias.aspx
// http://www.2l.no/hires/index.html
int main()
{
	DSD dsdfile;
	float *float_out_352[2] = {};											// stereo output
	float *float_out_884[2]{};
	size_t nSamples_per_channel = 0;										
	// -1 Read dsd file
	dsd_read(&dsdfile, "2L-125_stereo-2822k-1b_04.dsf");					// music signal

	// -2.1 State 1. ( f64 -> f8 ) 8:1	,general 352.8khz signal
	dsd_decode(&dsdfile, float_out_352, nSamples_per_channel);
	//wavwrite_float("v001 - sweep - to processed.wav", float_out_352, nSamples_per_channel , 1, 44100 * 8 ); // not output
	
	// -2.2 State 2. (f8 -> f2) 4:1		,resample 352.8khz to 88.4khz using a FIR filter
	FIR *lpf_882 = FIR_Create( 48, fir_coeffs_state2);
	size_t nStep = 4;
	float_out_884[0] = (float*)malloc(sizeof(float) * nSamples_per_channel / nStep);
	float_out_884[1] = (float*)malloc(sizeof(float) * nSamples_per_channel / nStep);
	FIR_Process(lpf_882, float_out_352[0], 0, float_out_884[0], 0, nSamples_per_channel, nStep);

	// -3 output to wav
	wavwrite_float("music v1.wav", &float_out_884[0], nSamples_per_channel / nStep, 1, 44100 * 2);

	// -4 Destory and Free resources
	FIR_Destory(lpf_882);
	free(float_out_884[0]);
	free(float_out_884[1]);
	free(float_out_352[0]);

	return 0;
}




/*
Usage:
About the DSD read method:
	dsd2pcm.c
	dsd2pcm.h
	dsdfile.h

1.	read dsd file:

		DSD dsdfile;				// initialize the dsd structure
		dsd_read(&dsdfile, "dsd filename.dsd");	//use the | int dsd_read(DSD *dsdfile, const char* file_name) |  function

2.	decode dsd data,note that the library decode the dsd data to 352khz float data.

		float *float_out_352[2] = {};	// initialize the output pointer
		dsd_decode(&dsdfile, float_out_352, nSamples_per_channel);	// decode the dsd data

3. optional:
	Resample to other samplerate as showed above.

About the WAV read /write method:
	dr_wav.h
	wavfile.h

1.	Wav file read:
		WAV wavfile;				// initialize the wav structure
		wavread("your wav file.wav",&wavfile); // use the wavread function | wavread(const char* filename, wav* wavfile) |
		// the decoded PCM data(int16_t / float) can be assessed through the wav structure
		// wavfile.pDataS16[channel_num][sample_num]
		// wavfile.pDataFloat[channel_num][sample_num]
2. Wav file write:
	we provide two functions convert int16_t / float pcm data into 16-bits wav file.

	int wavwrite_s16(const char* filename, int16_t * const *pDataS16, size_t nSamples, unsigned int nChannels, unsigned int sampleRate) 
	int wavwrite_float(const char* filename, float * const *pDataFloat, size_t nSamples, unsigned int nChannels, unsigned int sampleRate) 

*/