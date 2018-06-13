/////////////////////////////////
// VERTVER, 2018 (C)
// OpenAu, Open Audio Utility
// MIT-License
/////////////////////////////////
// AuEngine.cpp:
// methods for AuEngine
/////////////////////////////////

/*************************************
* How to use:
*
* Firstly, we need to read audiofile
* with method ReadAudioFile(filename).
*
* After this, we create a output 
* (void CreateOutput()), and after,
* we need to close out stream
* (void CloseStream()).
**************************************/

#include "AuEngine.h"
#include <thread>
#include <iomanip>
#define SAMPLE_RATE 44100
#define CHANNEL_COUNT (2)

// Validating and checking
#define CHECK(x)			{ if(!(x))				{  Msg("failure:"); } }
#define PA_CHECK(x, y)		if (x != paNoError)		{ THROW_EXCEPTION(y); }

FILE* oFile;
int numChannels;
int sampleRate;
PaSampleFormat sampleFormat;
int bytesPerSample, bitsPerSample;

std::string HostDefault;

/***********************************************
* Msg(string):
* To DebugStringOutput (string)
***********************************************/
void Msg(std::string szMessage)
{
#ifdef WIN32
	OutputDebugStringA(szMessage.c_str());
	OutputDebugStringA("\n");
#endif
}

/***********************************************
* Msg(string, num):
* To DebugStringOutput (string + num)
***********************************************/
void Msg(std::string szMessage, int iNum)
{
#ifdef WIN32
	OutputDebugStringA(szMessage.c_str());
	OutputDebugStringA(std::to_string(iNum).c_str());
	OutputDebugStringA("\n");
#endif
}

/***********************************************
* streamCallback():
* Method for Stream Callback
***********************************************/
int streamCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	size_t numRead = fread(outputBuffer, bytesPerSample * numChannels, framesPerBuffer, oFile);
	outputBuffer = (uint8_t*)outputBuffer + numRead * numChannels * bytesPerSample;
	framesPerBuffer -= numRead;

	if (framesPerBuffer > 0) 
	{
		memset(outputBuffer, 0, framesPerBuffer * numChannels * bytesPerSample);
		return paComplete;
	}
	return paContinue;
}

/***********************************************
* paCallback():
* Static method for PortAudio Callback
***********************************************/
static int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	return streamCallback(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags, userData);
}


/***********************************************
* freadNum():
* Checking for bad number
***********************************************/
template<typename T> T freadNum(FILE* f)
{
	T value;
	CHECK(fread(&value, sizeof(value), 1, f) == 1);
	return value; // no endian-swap for now... WAV is LE anyway...
}

/***********************************************
* ReadFmtChunk():
* Reading FMT chunk with information
***********************************************/
void ReadFmtChunk(uint32_t chunkLen) 
{

	CHECK(chunkLen >= 16);
	uint16_t fmttag = freadNum<uint16_t>(oFile);
	CHECK(fmttag == 1 || fmttag == 3);

	numChannels = freadNum<uint16_t>(oFile);
	CHECK(numChannels > 0);
	Msg("channels:", numChannels);
	sampleRate = freadNum<uint32_t>(oFile);
	Msg("Hz:", sampleRate);

	uint32_t byteRate = freadNum<uint32_t>(oFile);
	uint16_t blockAlign = freadNum<uint16_t>(oFile);

	bitsPerSample = freadNum<uint16_t>(oFile);
	bytesPerSample = bitsPerSample / 8;
	CHECK(byteRate == sampleRate * numChannels * bytesPerSample);
	CHECK(blockAlign == numChannels * bytesPerSample);

	// 1: PCM  - int
	// 3: IEEE - float
	if (fmttag == 1) 
	{
		switch (bitsPerSample) 
		{
		case 8: sampleFormat  = paInt8;  break;
		case 16: sampleFormat = paInt16; break;
		case 32: sampleFormat = paInt32; break;
		default: CHECK(false);
		}
		Msg("PCM bit int: ", bitsPerSample);
	}
	else 
	{
		CHECK(fmttag == 3);
		CHECK(bitsPerSample == 32);
		sampleFormat = paFloat32;
		Msg("32bit float");
	}
	if (chunkLen > 16) 
	{
		uint16_t extendedSize = freadNum<uint16_t>(oFile);
		CHECK(chunkLen == 18 + extendedSize);
		fseek(oFile, extendedSize, SEEK_CUR);
	}
}

/***********************************************
* freadStr():
* Checking for bad string
***********************************************/
std::string freadStr(FILE* f, size_t len) 
{
	std::string s(len, '\0');
	CHECK(fread(&s[0], 1, len, f) == len);
	return s;
}
int IFileTypeGlobal;

/***********************************************
* ReadAudioFile():
* Reading audiofile for output thread
*
* iFileType:
* 1 - .wav, 2 - .aif, 3 - .aiff, 4 - .mp3
* Just because we need to convert data to PCM
***********************************************/
void AuEngine::Input::ReadAudioFile(const char* lpFileName)
{
	//#TODO: fopen()->CreateFile()
	oFile = fopen(lpFileName, "rb");
	if (!oFile)
	{
		// Without UI this is a critical error
		THROW_EXCEPTION(AuEngine::OpSet::ENGINE_ERROR);
	}

	if (freadStr(oFile, 4) == "RIFF")
	{
		uint32_t wavechunksize = freadNum<uint32_t>(oFile);
		CHECK(freadStr(oFile, 4) == "WAVE");
		IFileTypeGlobal = 1;
	}
	else if (freadStr(oFile, 4) == "FORM")
	{
		uint32_t wavechunksize = freadNum<uint32_t>(oFile);
		CHECK(freadStr(oFile, 4) == "AIFF");
		IFileTypeGlobal == 2;
	}
	else if (freadStr(oFile, 3) == "ID3")
	{
		IFileTypeGlobal == 3;
	}
	else if (freadStr(oFile, 12) == "   ftypmp42")
	{
		IFileTypeGlobal = 4;
	}
	else if (freadStr(oFile, 11) == "    ftypM4A")
	{
		IFileTypeGlobal == 5;
	}
	else if (freadStr(oFile, 4) == "fLaC")
	{
		IFileTypeGlobal == 6;
	}
}

/***********************************************
* ReadChunks():
* Checking valid chunks
***********************************************/
void AuEngine::Output::ReadChunks()
{
	while (TRUE)
	{
		if(IFileTypeGlobal == WAV_FILE)
		{
			std::string chunkName = " ";
			chunkName = freadStr(oFile, 4);
			uint32_t chunkLen = freadNum<uint32_t>(oFile);

			if (chunkName == "fmt ")
				ReadFmtChunk(chunkLen);
			else if (chunkName == "data")
			{
				CHECK(sampleRate != 0);
				CHECK(numChannels > 0);
				CHECK(bytesPerSample > 0);
				break; // start playing now
			}
			else
			{
				CHECK(fseek(oFile, chunkLen, SEEK_CUR) == 0);		// skip chunk
			}
		}
		else if(IFileTypeGlobal == AIF_FILE | IFileTypeGlobal == AIFF_FILE)
		{
			std::string chunkName = " ";
			chunkName = freadStr(oFile, 4);
			uint32_t chunkLen = freadNum<uint32_t>(oFile);

			if (chunkName == "COMM")
				ReadFmtChunk(chunkLen);
			else if (chunkName == "SSND")
			{
				CHECK(sampleRate != 0);
				CHECK(numChannels > 0);
				CHECK(bytesPerSample > 0);
				break; // start playing now
			}
			else
			{
				CHECK(fseek(oFile, chunkLen, SEEK_CUR) == 0);		// skip chunk
			}
		}
	}
}

/***********************************************
* CreateStream():
* Creating output stream with device (default)
***********************************************/
void AuEngine::Output::CreateStream(PaDeviceIndex paDevice)
{
	PaStreamParameters outputParameters;
	outputParameters.device = paDevice;

	if (paDevice != paNoDevice)
	{
		const PaDeviceInfo* pInfo = Pa_GetDeviceInfo(paDevice);
		std::string toMsg = pInfo->name;		
		if (pInfo) { Msg("Output device name: " + toMsg); }
		int iSampleRate = (int)pInfo->defaultSampleRate;
		HostDefault = "All done! Default device sample rate: " + std::to_string(iSampleRate) + "Hz.";


		// Create our output with max that PortAudio can
		outputParameters.channelCount = 2;
		outputParameters.sampleFormat = paFloat32;
		outputParameters.suggestedLatency = Pa_GetDeviceInfo(paDevice)->defaultLowOutputLatency;
		outputParameters.hostApiSpecificStreamInfo = NULL;


		// Needy for validate type of chunks (fmt or data)
		ReadChunks();	

		PaError err = Pa_OpenStream(&stream, NULL, &outputParameters, iSampleRate,
			paFramesPerBufferUnspecified, NULL, &paCallback, NULL);


		// Check stream for current state
		PA_CHECK(err, AuEngine::OpSet::STREAM_ERROR);

		// Start stream with our output params
		err = Pa_StartStream(stream);

		PA_CHECK(err, AuEngine::OpSet::STREAM_ERROR);

		VUMeterInit();

;	}
	else
	{
		// Device doesn't support MME
		THROW_EXCEPTION(AuEngine::OpSet::MME_ERROR);		
	}
}

/***********************************************
* FinishedCallbackMsg():
* ==UNUSED==
***********************************************/
static void FinishedCallbackMsg(void *userData)
{
	return Msg("");
}

/***********************************************
* CloseStream():
* Close Output Stream
***********************************************/
void AuEngine::Output::CloseStream()
{
	fclose(oFile);
	Pa_CloseStream(stream);
}

/***********************************************
* OutputThread():
* Create output thread with stream
***********************************************/
void AuEngine::Output::OutputThread(const char* lpName)
{
	AuEngine::Input input;
	PaDeviceIndex outDevice = Pa_GetDefaultOutputDevice();
	input.ReadAudioFile(lpName);
	CreateStream(outDevice);
}

/***********************************************
* CreateOutput():
* Create output stream and thread 
***********************************************/
void AuEngine::Output::CreateOutput(const char* lpName)
{
	try
	{
		AuEngine::Output output;
		AuEngine::Input input;
		input.GetListOfDevices();
		output.OutputThread(lpName);
		Msg("AuEngine: Output was created");
	}
	catch (...)
	{
		THROW_EXCEPTION(AuEngine::OpSet::ENGINE_ERROR);
	}
}

/***********************************************
* GetOutputDevice():
* Return name of (default) device
***********************************************/
const char* AuEngine::Output::GetOutputDevice()
{
	return HostDefault.c_str();
}

/***********************************************
* GetCPULoadStream():
* Return CPU load by stream
***********************************************/
int AuEngine::Output::GetCPULoadStream(float fLoad)
{
	return Pa_GetStreamCpuLoad(stream) * 100;		// don't use if stream doesn't exist
}

/***********************************************
* GetListOfDevices():
* Print to DebugString (all audiodevices)
***********************************************/
void AuEngine::Input::GetListOfDevices()
{
	PaError err = Pa_Initialize();

	PA_CHECK(err, AuEngine::OpSet::INIT_ERROR);

	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		Msg("AuEngine: No devices");
		THROW_EXCEPTION(AuEngine::OpSet::NO_AUDIO_DEVICE);
	}
	else
		Msg("AuEngine: List of devices: ", numDevices);

	for (int i = 0; i < numDevices; i++)
	{
		deviceInfo = Pa_GetDeviceInfo(i);
		Msg("AuEngine: Device number: ", i);

		// Mark global and API specific default devices 
		defaultDisplayed = 0;
		if (i == Pa_GetDefaultInputDevice())
		{
			Msg("AuEngine: Default Input");
			defaultDisplayed = 1;
		}
		else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultInputDevice)
		{
			const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
			std::string hName = hostInfo->name;
			Msg("AuEngine: Default Input: " + hName);
			defaultDisplayed = 1;
		}

		if (i == Pa_GetDefaultOutputDevice())
		{
			Msg("AuEngine: Default Output");
			defaultDisplayed = 1;
		}
		else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultOutputDevice)
		{
			const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
			std::string hName = hostInfo->name;
			Msg("AuEngine: Default Output: " + hName);
			defaultDisplayed = 1;
		}

		std::string szDevice = deviceInfo->name;
		std::string szDeviceInfoHostAPI = Pa_GetHostApiInfo(deviceInfo->hostApi)->name;
#ifdef DEBUG
		Msg("Name: " + szDevice);

		Msg("Host API = " + szDeviceInfoHostAPI);
		Msg("Max inputs = ", deviceInfo->maxInputChannels);
		Msg("Max outputs = ", deviceInfo->maxOutputChannels);
		Msg("Default low input latency = ", deviceInfo->defaultLowInputLatency);
		Msg("Default low output latency  = ", deviceInfo->defaultLowOutputLatency);
		Msg("Default high input latency  = ", deviceInfo->defaultHighInputLatency);
		Msg("Default high output latency = ", deviceInfo->defaultHighOutputLatency);
#endif

#ifdef WIN32 & PA_USE_ASIO
		// ASIO specific latency information 
		if (Pa_GetHostApiInfo(deviceInfo->hostApi)->type == paASIO) 
		{
			long minLatency, maxLatency, preferredLatency, granularity;

			err = PaAsio_GetAvailableLatencyValues(i,
				&minLatency, &maxLatency, &preferredLatency, &granularity);
#ifdef DEBUG
			Msg("ASIO minimum buffer size    = %ld\n", minLatency);
			Msg("ASIO maximum buffer size    = %ld\n", maxLatency);
			Msg("ASIO preferred buffer size  = %ld\n", preferredLatency);
#endif
		}
#endif

		static double standardSampleRates[] = { 8000.0, 9600.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0,
												32000.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0, -1 };

		int printCount = 0;

		inputParameters.device = i;
		inputParameters.channelCount = deviceInfo->maxInputChannels;
		inputParameters.sampleFormat = paInt16;
		inputParameters.suggestedLatency = 0;			// ignored by Pa_IsFormatSupported()
		inputParameters.hostApiSpecificStreamInfo = NULL;

		outputParameters.device = i;
		outputParameters.channelCount = deviceInfo->maxOutputChannels;
		outputParameters.sampleFormat = paInt16;
		outputParameters.suggestedLatency = 0;			// ignored by Pa_IsFormatSupported()
		outputParameters.hostApiSpecificStreamInfo = NULL;

		for (int u = 0; standardSampleRates[u] > 0; u++)
		{
			err = Pa_IsFormatSupported(&inputParameters, &outputParameters, standardSampleRates[u]);
			if (err == paFormatIsSupported)
			{
#ifdef DEBUG
				if (printCount == 0)
				{
					Msg("Sample rates: ", standardSampleRates[u]);
					printCount = 1;
				}
				else if (printCount == 4)
				{
					Msg("Sample rates: ", standardSampleRates[u]);
					printCount = 1;
				}
				else
				{
					Msg("Sample rates: ", standardSampleRates[u]);
					++printCount;
				}
#endif
			}
		}
	}
}
