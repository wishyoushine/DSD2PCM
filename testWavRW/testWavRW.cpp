// testWavRW.cpp: 定义控制台应用程序的入口点。
//
#include <iostream>
using namespace std;
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

static inline float S16ToFloat_(int16_t v) {
	static const float kMaxInt16Inverse = 1.f / 32767;
	static const float kMinInt16Inverse = 1.f / -32768;
	return v * (v > 0 ? kMaxInt16Inverse : -kMinInt16Inverse);
}
int S16ToFloat(int16_t *pS16Samples , size_t nSamples, float *pFloatSamples) {
	if (pFloatSamples == NULL || nSamples <0 || pS16Samples == NULL)
	{
		return -1;
	}
	for (size_t n = 0; n < nSamples; n++)
	{
		pFloatSamples[n] = S16ToFloat_(pS16Samples[n]);
	}
	return 0;
}

static inline int16_t FloatToS16_(float v) {
//S16:      int16_t[-32768, 32767]
	if (v > 0)
		return v >= 1 ? 32767
		: (int16_t)(v * 32767 + 0.5f);
	return v <= -1 ? -32768
		: (int16_t)(-v * -32768 - 0.5f);
}
int FloatToS16(float * pFloatSamples, size_t nSamples, int16_t *pS16Samples) {
	
	if (pFloatSamples == NULL || nSamples <0 || pS16Samples ==NULL)
	{
		return -1;
	}
	for (size_t n = 0; n < nSamples; n++)
	{
		pS16Samples[n] = FloatToS16_(pFloatSamples[n]);
	}
	return 0;
}
struct wav
{
	unsigned int channels;
	unsigned int sampleRate;
	drwav_uint64 totalPCMFrameCount;	// nSamples per cahnnels
	drwav_uint64 totalSampleCount;		// left samples + ritht samples
	int16_t *pDataS16[2];
	float *pDataF16[2];
};
//int16_t* wavread_s16(const char* filename, unsigned int* channelsOut, unsigned int* sampleRateOut, drwav_uint64* totalFrameCountOut) {
int wavread(const char* filename, wav* wavfile ) {

	// return a pointer of audio data which is [LRLRLRLR ...] format
	float *pSampleF16 = drwav_open_file_and_read_pcm_frames_f32(filename, &wavfile->channels,&wavfile->sampleRate, &wavfile->totalPCMFrameCount);
	int16_t *pSampleS16 = drwav_open_file_and_read_pcm_frames_s16(filename, &wavfile->channels, &wavfile->sampleRate, &wavfile->totalPCMFrameCount);
	wavfile->totalSampleCount = wavfile->totalPCMFrameCount * wavfile->channels;

	if (pSampleF16 == NULL || pSampleS16 == NULL)
	{
		return -1;
	}
	//wavfile->pDataS16[0] = new int16_t[wavfile->totalSampleCount]{ 0 };
	wavfile->pDataS16[0] = pSampleS16;

	//wavfile->pDataF16[0] = new float[wavfile->totalSampleCount]{ 0 };
	wavfile->pDataF16[0] = pSampleF16;
	//	change audio data into [[LLLLLL][RRRRRR]]
	if (wavfile->channels >1)
	{
		wavfile->pDataS16[1] = wavfile->pDataS16[0] + wavfile->totalPCMFrameCount;	// ??
		wavfile->pDataF16[1] = wavfile->pDataF16[0] + wavfile->totalPCMFrameCount;

		for (size_t n = 0; n < wavfile->totalPCMFrameCount; n++)
		{
			wavfile->pDataS16[0][n] = pSampleS16[n * 2];
			wavfile->pDataS16[1][n] = pSampleS16[n * 2 + 1];

			wavfile->pDataF16[0][n] = pSampleF16[n * 2];
			wavfile->pDataF16[1][n] = pSampleF16[n * 2 + 1];

		}
	}
	return 0;
}
int wavwrite_s16(const char* filename, int16_t * const *pDataS16,size_t nSamples,unsigned int nChannels,unsigned int sampleRate) {
	drwav_data_format format;
	format.container = drwav_container_riff;     // <-- drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
	format.format = DR_WAVE_FORMAT_PCM;          // <-- Any of the DR_WAVE_FORMAT_* codes.
	format.channels = nChannels;
	format.sampleRate = sampleRate;
	format.bitsPerSample = 16;
	drwav* pWav = drwav_open_file_write(filename, &format);
	if (pWav == NULL)
	{
		return -1;
	}
	if (nChannels > 1 )
	{
		int16_t tmp = 0;
		//int16_t *data = new int16_t[nSamples * 2]{};
		int16_t *data = (int16_t *) malloc( sizeof(int16_t) * nSamples * 2);

		for (size_t n = 0; n < nSamples ; n++)
		{
			data[n * 2] = pDataS16[0][n];		// even part left channel
			data[n * 2 + 1] = pDataS16[1][n];	//  odd part, ritht channel
		}
		drwav_uint64 samplesWritten = drwav_write_pcm_frames(pWav, nSamples, data);
		//drwav_uint64 samplesWritten = drwav_write_pcm_frames(pWav, nSamples, pDataS16[0]);

		//delete[] data;
		free(data);

	}
	else
	{
		//drwav_uint64 samplesWritten = drwav_write_pcm_frames(pWav, nSamples, pDataS16[0]);
		//drwav_uint64 samplesWritten = drwav_write_pcm_frames(pWav, nSamples, pDataS16[0]);	// convert
		drwav_uint64 samplesWritten = drwav_write_raw(pWav, nSamples * 2 , pDataS16[0]);	// convert
		if (samplesWritten != nSamples)
		{
			printf("written failed\n");
			//return -1;
		}
	}

	drwav_close(pWav);
	return 0;

}
int main()
{
	unsigned int channels;
	unsigned int sampleRate;
	drwav_uint64 totalPCMFrameCount;
	drwav_uint64 totalSampleCount;

	//float* pSampleData = drwav_open_file_and_read_pcm_frames_f32("dukou_noReverb.wav", &channels, &sampleRate, &totalPCMFrameCount);
	float* pSampleData = drwav_open_file_and_read_pcm_frames_f32("audioCut_2.wav", &channels, &sampleRate, &totalPCMFrameCount);
	if (pSampleData == NULL) {
		cout << "can not open the file";
		return -1;
		// Error opening and reading WAV file.
	}
	totalSampleCount = channels * totalPCMFrameCount;

	wav mywav;
	wavread("audioCut_2.wav", &mywav);
	cout << mywav.channels << endl;
	cout << mywav.totalPCMFrameCount << endl;
	for (size_t n = 0; n < 10; n++)
	{
		cout << n << " : ";
		cout << mywav.pDataF16[0][totalPCMFrameCount - n] ;
		if (mywav.channels >1)
		{
			cout<<" - "<<mywav.pDataF16[1][totalPCMFrameCount - n];
		}
		cout << endl;
	}


	for (size_t n = 0; n < 20; n++)
	{
		cout << n << " : " << pSampleData[channels * n] << endl;
	}

	int16_t *pOutput = new int16_t[totalSampleCount]{};
	FloatToS16(pSampleData, totalSampleCount, pOutput);

    drwav_data_format format;
    format.container = drwav_container_riff;     // <-- drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
    format.format = DR_WAVE_FORMAT_PCM;          // <-- Any of the DR_WAVE_FORMAT_* codes.
    format.channels = 2;
    format.sampleRate = 44100;
    format.bitsPerSample = 16;
    drwav* pWav = drwav_open_file_write("recording v4.wav", &format);

	drwav_uint64 samplesWritten = drwav_write_pcm_frames(pWav, totalPCMFrameCount  , pOutput);
	//drwav_uint64 samplesWritten = drwav_write_raw(pWav, totalSampleCount * 2, pOutput);// bytesToWrite = sample * bitsPerSample / 8  * channels = totalPCMFrameCount * bitsPerSample / 8  * channels

	cout << samplesWritten << endl;


	//wavwrite_s16("test my writter.wav", mywav.pDataS16, mywav.totalPCMFrameCount, 1, mywav.sampleRate);
	wavwrite_s16("test my writter.wav", mywav.pDataS16, mywav.totalPCMFrameCount, 1, mywav.sampleRate);

	drwav_close(pWav);
	drwav_free(pSampleData);

    return 0;
}


//FILE *fp = NULL;
//fopen_s(&fp, "dukou_noReverb.wav","rb");
//assert(fp != NULL);
//Header header;

//fread(&header, sizeof(header), 1, fp);
//fseek(fp, sizeof(header), 0);

//short data[10];
//fread(data, 10 * sizeof(short), 1,fp);

//for (size_t i = 0; i < 10; i++)
//{
//	cout << data[i] << endl;
//}
//cout << header.sample_rate;

//fclose(fp);
//typedef struct Header {
//struct Header {
//	/* WAV-header parameters				Memory address - Occupied space - Describes */
//	char chunk_ID[4];                           // 0x00 4 byte - RIFF string
//	uint32_t  chunk_size;                  // 0x04 4 byte - overall size of
//	char format[4];                             // 0x08 4 byte - WAVE string
//	char fmt_chunk_marker[4];                   // 0x0c 4 byte - fmt string with trailing null char
//	uint32_t length_of_fmt;					  // 0x10 4 byte - length of the format data,the next part
//	uint16_t  format_type;					// 0x14 2 byte - format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
//	uint16_t  channels;						 // 0x16 2 byte - nunbers of channels
//	uint32_t  sample_rate;					// 0x18 4 byte - sampling rate (blocks per second)
//	uint32_t  byte_rate;                   // 0x1c 4 byte - SampleRate * NumChannels * BitsPerSample/8 [比特率]
//	uint16_t  block_align;                 // 0x20 2 byte - NumChannels * BitsPerSample/8 [块对齐=通道数*每次采样得到的样本位数/8]
//	uint16_t  bits_per_sample;				 // 0x22 2 byte - bits per sample, 8- 8bits, 16- 16 bits etc [位宽]
//	char data_chunk_header[4];				// 0x24 4 byte - DATA string or FLLR string
//	uint32_t  data_size;					 // 0x28 4 byte - NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk 
//											 //				that will be read,that is the size of PCM data.
//};