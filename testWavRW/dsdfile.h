#pragma once
#include <stdio.h>
#include <stdint.h>
#include "dsd2pcm.h"
#include "state2.h"
#ifdef __cplusplus
extern "C" {
#endif // _cplusplus

// if have noise shape
	//	#include "noise_shape_test.h"
	//  #define HAVE_NOISE_SHAPE
typedef struct 
{
	// uint64_t  a 8 bytes,64 bits unsigned int, 
	// uint32_t  a 4 bytes,32 bits unsigned int, 

	char dsd_chunk_header[4];		// DSD mask
	char chunk_size[8];				// size of this chunk [28]
									//uint64_t chunk_size;			// size of this chunk [28] test 
	uint64_t total_size;			// total file size
	char pMetadataChunk[4];			// the pointer point to metadata if exist,otherwise set 0.
									// 8?
	char fmt_chunk_header[4];		// fmt chunk header
	uint64_t fmt_size;				// size of this fmt chunk,usually [52] bytes
	uint32_t fmt_version;			// gersion of this format [1]
	uint32_t fmt_id;				// [0]: DSD raw
	uint32_t channel_type;			// [1]:mono [1]:stereo [3]: 3 channels [4] quad [5] 4chs [6] 5chs [7] 5.1chs
	uint32_t channel_num;			// [1]-[6] : mono~6 chs
	uint32_t sample_rate;			//  [2822400] or [5644800] hz  64fs or 128fs
	uint32_t bits_per_sample;		// [1] or [8]
	uint64_t sample_count;			// samples per channels, n second data: sample count would be fs * n
	uint32_t block_per_channel;		// fixed [4096] Bytes
	uint32_t reverd;				// fill zero

	char	data_chunk_header[4];	// [ d a t a]
	uint64_t data_size;			// equal to [n] + 12 , n is the next [n] bytes contains data 
								// next n bytes is the data
								//char* pSampleData = 0;
								// m bytes meta data chunk if have
	uint8_t* pSampleData = 0;	// a pointer points to the sample data (byte types)
	unsigned int nBytes;
}DSD;

DSD* dsd_create() {
	DSD* handles = (DSD*)malloc(sizeof(DSD));
	return handles;
}
int dsd_destory(DSD* handles) {
	if (handles !=NULL)
	{
		if (handles->pSampleData != NULL) {
			free(handles->pSampleData);
		}
		free(handles);
		return 0;
	}
	return -1;
}


//int dsd_read(DSD *dsdfile, const char* file_name) {
DSD* dsd_read(const char* file_name) {

	FILE *fp = NULL;
#if defined(_MSC_VER) && _MSC_VER >= 1400
	if (fopen_s(&fp, file_name, "rb") != 0) {
		return NULL;
	}
#else
	fp = fopen(file_name, "rb");
	if (pFile == NULL) {
		return NULL;
	}
#endif

	DSD *dsdfile = NULL;
	dsdfile = dsd_create();
	if (dsdfile == NULL)
	{
		printf("dsd initialized failed\n");
		return NULL;
	}
	fread(dsdfile->dsd_chunk_header, 84, 1, fp);
	fread(&dsdfile->data_size, sizeof(dsdfile->data_size), 1, fp);
	unsigned int nSamples = dsdfile->data_size - 12;					// the total size of data, nSamples / nCh
	if (nSamples != dsdfile->sample_count / 8 * dsdfile->channel_num)
	{
		//printf("Samples not match\n");
		nSamples = (dsdfile->sample_count / 8 * dsdfile->channel_num / 4096) * 4096;
	}
	// read the raw DSD data
	//dsdfile->pSampleData = new uint8_t[nSamples]{};						// Initialize
	dsdfile->pSampleData = (uint8_t*)malloc(sizeof(uint8_t) * nSamples);
	dsdfile->nBytes = nSamples;

	fread(dsdfile->pSampleData, nSamples, 1, fp);						// read

	printf("     DSD file messages:\n");
	printf("================================\n");
	printf("Sample rate	:	%d\n", dsdfile->sample_rate);
	printf("DSD version	:	%d\n", dsdfile->fmt_version);
	printf("Channels	:	%d\n", dsdfile->channel_num);
	printf("Bit per sample	:	%d\n", dsdfile->bits_per_sample);
	printf("Samples	count	:	%d\n", dsdfile->sample_count);
	printf("Total bytes	:	%d\n", dsdfile->nBytes);
	printf("Block size	:	%d\n", dsdfile->block_per_channel);
	printf("Duration(s)	:	%d\n", dsdfile->sample_count / dsdfile->sample_rate);
	printf("================================\n");

	fclose(fp);
	return dsdfile;
}

int dsd_decode(DSD *dsdfile, float *pFloat_out[2], size_t &samples_decoded) {

	// initialize the noise shape
#ifdef HAVE_NOISE_SHAPE
	noise_shape_ctx_s* pNs[2];
	pNs[0] = (noise_shape_ctx_s *)malloc(sizeof(noise_shape_ctx_s));
	pNs[1] = (noise_shape_ctx_s *)malloc(sizeof(noise_shape_ctx_s));
	noise_shape_init(pNs[0], my_ns_soscount, my_ns_coeffs);
	noise_shape_init(pNs[1], my_ns_soscount, my_ns_coeffs);
#endif // HAVE_NOISE_SHAPE

	// initialize the decode module
	dsd2pcm_ctx *d2p[2]{};
	d2p[0] = dsd2pcm_init();
	d2p[1] = dsd2pcm_init();
	if (d2p[0] == NULL)
	{
		printf("D2P initialize erroes\n");
		return -1;
	}
	// set the decode parameters
	const int block = 4096;									// default is |4096| for DSF file.
	int channels = dsdfile->channel_num;
	int lsbitfirst = 1;										// default is |1| for DSF file.

	float *fTmp = (float *)malloc(dsdfile->nBytes * sizeof(float));
	pFloat_out[0] = fTmp;
	pFloat_out[1] = fTmp + dsdfile->nBytes / 2;				// right channel

	samples_decoded = dsdfile->nBytes / 2;
	size_t nFrames = dsdfile->nBytes / (block * channels);
	size_t upIndex[2] = { 0,0 };

	// decode to 352khz 
	for (size_t n = 0; n < nFrames; n++)
	{
		for (int c = 0; c < channels; ++c) {

			dsd2pcm_translate(
				d2p[c],
				block,
				dsdfile->pSampleData + n * block * channels + c * block,
				1,
				lsbitfirst,
				pFloat_out[c] + upIndex[c],
				1
			);

#ifdef HAVE_NOISE_SHAPE
			// Note that noise shape test not pass.
			// Noise shaping
			//*(pFloat_out[c] + upIndex[c]) += noise_shape_get(pNs[c]);

			//long smp =CLIP(-32768, myround(*(pFloat_out[c] + upIndex[c]) ) , 32767);

			//noise_shape_update(pNs[c], CLIP(-1, smp - *(pFloat_out[c] + upIndex[c]) , 1) );
#endif // HAVE_NOISE_SHAPE

			upIndex[c] += block;
		}
	}
	dsd2pcm_destroy(d2p[0]);
	dsd2pcm_destroy(d2p[1]);


#ifdef HAVE_NOISE_SHAPE
	noise_shape_destroy(pNs[0]);
	noise_shape_destroy(pNs[1]);
#endif // HAVE_NOISE_SHAPE

	samples_decoded = upIndex[0];		// return the number of samples have been read.
	return 0;
}

int dsd_decode_882(DSD *dsdfile, float *pFloat_out[2], size_t &samples_decoded) {
	// decode dsd file from dsd data into 88.2kHz float data
	// dsd data-> decode to 352kHz->decode to 88.2kHz(state 2)->output float data
	float *pFloat_352[2]{ 0 };
	int error = dsd_decode(dsdfile, pFloat_352, samples_decoded);
	if (error !=0)
	{
		printf("dsd decode state 1 failed\n");
		return -1;
	}
	int channels = dsdfile->channel_num;
	FIR *lpf_882[2]{0};
	size_t nStep = 4;
	for (size_t n = 0; n < channels; n++)
	{
		lpf_882[n] = state2_create(48, state2_fir_coeffs);
		pFloat_out[n] = (float*)malloc(sizeof(float) * samples_decoded / nStep );
		state2_process(lpf_882[n], pFloat_352[n], 0, pFloat_out[n], 0, samples_decoded, nStep);
	}
	samples_decoded = samples_decoded / nStep;

	free(pFloat_352[0]);	// free memory in state1
	return 0;
}


// a fir structure for reference
float fir_smpl_circle_f32(int order, float sample, const float* coeffs, float* buffer, unsigned int* state) {
	float accu = 0.0f;
	int i = order - 1;
	buffer[*state] = sample;
	if (++(*state) >= order) { *state = 0; }
	for (; i >= 0; --i)
	{
		accu += buffer[*state] * coeffs[i];
		if (++(*state) >= order)
			*state = 0;
	}
	return accu;
}



#ifdef __cplusplus
}
#endif // _cplusplus


/* =============================== */
// to be deleted 
/*

struct  Biquad
{
	float coeffs[2][3]{};
	float state[3]{};
};
void biquad(Biquad *handles, float in, float &out) {
	// wrapped methods
	handles->state[2] = handles->state[1];
	handles->state[1] = handles->state[0];
	handles->state[0] = in + (-handles->coeffs[1][1]) * handles->state[1] + (-handles->coeffs[1][2]) * handles->state[2];
	// caculate the output
	out = handles->coeffs[0][0] * handles->state[0] + handles->coeffs[0][1] * handles->state[1] + handles->coeffs[0][2] * handles->state[2];
}

namespace {

	const float my_ns_coeffs[] = {
		//     b1           b2           a1           a2
		-1.62666423,  0.79410094,  0.61367127,  0.23311013,  // section 1
		-1.44870017,  0.54196219,  0.03373857,  0.70316556   // section 2
	};

	const int my_ns_soscount = sizeof(my_ns_coeffs) / (sizeof(my_ns_coeffs[0]) * 4);

	inline long myround(float x)
	{
		return static_cast<long>(x + (x >= 0 ? 0.5f : -0.5f));
	}

	template<typename T>
	struct id { typedef T type; };

	template<typename T>
	inline T clip(
		typename id<T>::type min,
		T v,
		typename id<T>::type max)
	{
		if (v < min) return min;
		if (v > max) return max;
		return v;
	}

	inline void write_intel16(unsigned char * ptr, unsigned word)
	{
		ptr[0] = word & 0xFF;
		ptr[1] = (word >> 8) & 0xFF;
	}

	inline void write_intel24(unsigned char * ptr, unsigned long word)
	{
		ptr[0] = word & 0xFF;
		ptr[1] = (word >> 8) & 0xFF;
		ptr[2] = (word >> 16) & 0xFF;
	}

} // anonymous namespace
*/
