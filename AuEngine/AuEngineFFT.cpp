/////////////////////////////////
// VERTVER, 2018 (C)
// OpenAu, Open Audio Utility
// MIT-License
/////////////////////////////////
// AuEngineFFT.cpp:
// FFT-analyse
/////////////////////////////////

/*******************************************
* FFT-analyse:
* Firstly, we need to get massive with 
* information of stream (chunks). FFT it's
* just massive with size (int array[size]). 
*
* Popular sizes are 1024, 4096, 512 and 
* 2048. Every element of array is 
* position for counting. 
*
* Secondly, we need a window to display 
* it. There are a some windows to 
* prepare data to display: 
* Blackman, Blackman-Harris, Triangular,
* Hann and Kaiser. It's needy to 
* delete clipping and show real 
* spectre.
*******************************************/

#include "AuEngineMath.h"
#include <corecrt_math_defines.h>

DLL_API bool OpenCL_FFT = false;

AuEngine::Output wOut;
AuEngine::Input wIn;

FFTStruct* fftstruct = {};
PaStream* FFTstream = wOut.stream;

int i, j, k;
struct fft_s *fft;

void AuMath::FFTProcess(void* FFT, float a[2], float b[3], float mem1[4], float mem2[4], int winmode)
{
	float window[FFT_SIZE];
	float freqTable[FFT_SIZE];
	char * noteNameTable[FFT_SIZE];
	float notePitchTable[FFT_SIZE];

	if (winmode = 1)
		BuildHannWindow(window, FFT_SIZE);
	else if (winmode = 2)
		BuildHammingWindow(window, FFT_SIZE);
	else if (winmode = 3)
		BuildBlackmanWindow(window, FFT_SIZE);
	else if (winmode = 4)
		BuildBlackmanHarrisWindow(window, FFT_SIZE);

	FFT = FFTInit(FFT_EXP_SIZE);
	ComputeSecondOrderLowPassParameters(44100, 330, a, b);

	for (int i = 0; i < FFT_SIZE; ++i)
	{
		freqTable[i] = (44100 * i) / (float)(FFT_SIZE);
	}
	for (int i = 0; i < FFT_SIZE; ++i)
	{
		noteNameTable[i] = NULL;
		notePitchTable[i] = -1;
	}
	for (int i = 0; i < 127; ++i)
	{
		float pitch = (440.0 / 32.0) * pow(2, (i - 9.0) / 12.0);
		if (pitch > 44100 / 2.0)
			break;
		// find the closest frequency using brute force.
		float min = 1000000000.0;
		int index = -1;
		for (int j = 0; j < FFT_SIZE; ++j)
		{
			if (fabsf(freqTable[j] - pitch) < min)
			{
				min = fabsf(freqTable[j] - pitch);
				index = j;
			}
		}
		//noteNameTable[index] = NOTES[i % 12];
		//notePitchTable[index] = pitch;
	}
}
/*******************************************
* FFTInit():
* Init FFT analyser
*******************************************/
void* AuMath::FFTInit(int b)
{

	fft = (struct fft_s *) malloc(sizeof(struct FFTStruct));
	//#NOTE: fft pointer must to be free

	if (!fft) 
		Msg("Could not allocate for FFT.");

	fftstruct->bits = b;

	if (fftstruct->bits > 15)
		Msg("A lot of bits at struct");

	for (i = (1 << fftstruct->bits) - 1; i >= 0; --i) 
	{
		k = 0;
		for (j = 0; j < fftstruct->bits; ++j) 
		{
			k *= 2;

			if (i & (1 << j)) 
			{ 
				k += 1; 
			}
		}
		fftstruct->bitReverse[i] = k;
	}
	return fft;
}

void AuMath::FFTClose()
{
	free(fft);
}

/*******************************************
* ConvertToFFT():
* Convert data to FFT massive
*******************************************/
void AuMath::ConvertToFFT(void * fft, float *xr, float *xi, bool inv)
{
	if (!OpenCL_FFT)
	{
		int n, n2, i, k, kn2, l, p;
		float ang, s, c, tr, ti;

		n = 1 << fftstruct->bits;
		n2 = n / 2;

		for (l = 0; l < fftstruct->bits; ++l)
		{
			for (k = 0; k < n; k += n2)
			{
				for (i = 0; i < n2; ++i, ++k)
				{
					p = fftstruct->bitReverse[k / n2];
					ang = 6.283185f * p / n;
					c = cos(ang);
					s = sin(ang);

					kn2 = k + n2;

					if (inv) s = -s;

					tr = xr[kn2] * c + xi[kn2] * s;
					ti = xi[kn2] * c - xr[kn2] * s;
					xr[kn2] = xr[k] - tr;
					xi[kn2] = xi[k] - ti;
					xr[k] += tr;
					xi[k] += ti;
				}
			}
			n2 /= 2;
		}

		for (k = 0; k < n; ++k)
		{
			i = fftstruct->bitReverse[k];
			if (i <= k)
				continue;
			tr = xr[k];
			ti = xi[k];
			xr[k] = xr[i];
			xi[k] = xi[i];
			xr[i] = tr;
			xi[i] = ti;
		}

		// Finally, multiply each value by 1/n, if this is the forward transform. 
		if (!inv)
		{
			float f;

			f = 1.0f / n;
			for (i = 0; i < n; ++i)
			{
				xr[i] *= f;
				xi[i] *= f;
			}
		}
	}
}

/*******************************************
* BuildHammingWindow():
* Build window by Hamming method
*******************************************/
void AuMath::BuildHammingWindow(float *window, int size)
{
	for (int i = 0; i < size; ++i)
		window[i] = .54f - .46f * cos(2 * M_PI * i / (float)size);
}

/*******************************************
* BuildHammingWindow():
* Build window by Hann method
*******************************************/
void AuMath::BuildHannWindow(float *window, int size)
{
	for (int i = 0; i < size; ++i)
		window[i] = .5f * (1 - cos(2 * M_PI * i / (size - 1.0f)));
}

/*******************************************
* BuildBlackmanWindow():
* Build window by Blackman method
*******************************************/
void AuMath::BuildBlackmanWindow(float* window, int size)
{
	float alpha = 0.16f;
	float alpha1 = (1 - alpha) / 2.0f;
	float alpha2 = 0.5f;
	float alpha3 = alpha / 2;

	for (int i = 0; i < size; ++i)
	{
		window[i] = alpha1 - alpha2 * cos((2 * M_PI * i) / (size - 1.0f))
						   + alpha3 * cos((4 * M_PI * i) / (size - 1.0f));
	}
}

/*******************************************
* BuildBlackmanHarrisWindow():
* Build window by Blackman-Harris method
*******************************************/
void AuMath::BuildBlackmanHarrisWindow(float* window, int size)
{
	float alpha1 = 0.35875f;
	float alpha2 = 0.48829f;
	float alpha3 = 0.14128f;
	float alpha4 = 0.01168f;

	for (int i = 0; i < size; ++i)
	{
		window[i] = alpha1 - alpha2 * cos((2 * M_PI * i) / (size - 1.0f))
						   + alpha3 * cos((4 * M_PI * i) / (size - 1.0f))
						   - alpha4 * cos((6 * M_PI * i) / (size - 1.0f));
	}
}


/*******************************************
* BuildKaiserWindow():
* Build window by Kaiser method
*******************************************/
void AuMath::BuildKaiserWindow(float* window, float shape, int size)
{	
	//#TODO: Must to be refactored.
	const float oneOverDenom = 1.0f / ZeroEthOrderBessel(shape);

	const UINT N = sizeof(window) - 1;
	const float oneOverN = 1.0f / N;

	for (UINT n = 0; n < size; ++n)
	{
		const float K = (2.0f * n * oneOverN) - 1.0f;
		const float arg = sqrt(1.0f - (K * K));

		window[n] = ZeroEthOrderBessel(shape * arg) * oneOverDenom;
	}
}

/*******************************************
* ComputeShape():
* Counting shape by attenuation
*******************************************/
static float ComputeShape(float atten)
{
	if (atten < 0.0f)
		Msg("Error: attenuation < 0");

	float alpha;

	if (atten > 60.0f)
		alpha = 0.12438f * (atten + 6.3);
	else if (atten > 13.26f)
		alpha = 0.76609f * (pow((atten - 13.26f), 0.4f)) + 0.09834f * (atten - 13.26f);
	else
		alpha = 0.0;	//  can't have less than 13dB.

	return alpha;
}

/*******************************************
* ApplyWindow():
* Apply window type
*******************************************/
void AuMath::ApplyWindow(float *window, float *data, int size)
{
	for (int i = 0; i < size; ++i)
		data[i] *= window[i];
}

/*******************************************
* ComputeSecondOrderLowPassParameters():
* Computing second order low pass parametres
*******************************************/
void AuMath::ComputeSecondOrderLowPassParameters(float sRate, float f, float *a, float *b)
{
	float a0;
	float w0 = 2 * M_PI * f / sRate;
	float cosw0 = cos(w0);
	float sinw0 = sin(w0);

	float alpha = sinw0 / 2 * M_SQRT2;

	a0 = 1 + alpha;
	a[0] = (-2 * cosw0) / a0;
	a[1] = (1 - alpha) / a0;
	b[0] = ((1 - cosw0) / 2) / a0;
	b[1] = (1 - cosw0) / a0;
	b[2] = b[0];
}

/*******************************************
* ProcessSecondOrderFilter():
* Proccesing second order filter 
*******************************************/
float AuMath::ProcessSecondOrderFilter(float x, float *mem, float *a, float *b)
{
	float ret = b[0] * x + b[1] * mem[0] + b[2] * mem[1] - a[0] * mem[2] - a[1] * mem[3];

	mem[1] = mem[0];
	mem[0] = x;
	mem[3] = mem[2];
	mem[2] = ret;

	return ret;
}

/*******************************************
* ZeroEthOrderBessel():
* return besselValue by float x
*******************************************/
float AuMath::ZeroEthOrderBessel(float x)
{
	const float eps = 0.000001f;

	// initialize the series term for m = 0 and the result
	float besselValue = 0;
	float term = 1;
	float m = 0;

	// accumulate terms as long as they are significant
	while (term > eps * besselValue)
	{
		besselValue += term;

		// update the term
		++m;
		term *= (x*x) / (4 * m*m);
	}

	return besselValue;
}

/*******************************************
* SignalHandler():
* turn off FFT-analyse
*******************************************/
void AuMath::SignalHandler(int signum) { /*running = false;*/ }
