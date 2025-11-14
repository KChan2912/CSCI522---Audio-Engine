#ifndef __PYENGINE_2_0_SOUND_MANAGER__
#define __PYENGINE_2_0_SOUND_MANAGER__

#define NOMINMAX
// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"
#include<format>
#include<stdio.h>
#include "../Scene/SceneNode.h"

// Outer-Engine includes
#include <D3DCommon.h>
#include <D3DCompiler.h>
#include<xaudio2.h>
#include<x3daudio.h>
#include <d3d9.h>


// Inter-Engine includes
#include "PrimeEngine/MemoryManagement/Handle.h"
#include "../Events/Component.h"
#include "../Events/Event.h"
#include "../Events/StandardEvents.h"

#define BIT(i) 1<<i

// Sibling/Children includes
namespace PE {
namespace Components {
	
struct SoundManager : public Component
{
	PE_DECLARE_CLASS(SoundManager);
	/*



	dspSettings.DstChannelCount = OUTPUT_CHANNELS * sourceVoiceDetails.InputChannels; 
	dspSettings.pMatrixCoefficients = outputMatrix;
	*/



	//XAudio2 Audio Graph
	//
	//
	//	 ________				  ________				  ________
	//	|		 |				 |		  |				 |		  |
	//	| Buffer |	<----------> | Source | -----------> | Master | -----------> | Out
	//	|________|				 |________|				 |________|
	//
	//



	//X3DAudio Constants (Azimuth and Other Values needed for 2 or more Channel Spatial Audio)
	//           FRONT
	//             | 0  <-- azimuth
	//             |
	//    7pi/4 \  |  / pi/4
	//           \ | /
	// LEFT       \|/      RIGHT
	// 3pi/2-------0-------pi/2
	//            /|\
	//           / | \
	//    5pi/4 /  |  \ 3pi/4
	//             |
	//             | pi
	//           BACK
	//

	//Constants
	static constexpr int MAX_CONCURRENT_SOUNDS = 16; //Essentially the number of Source Voices (Including Emitters).
	static constexpr int SOUNDS_BUFFER_SIZE = 536870912; //Amount in Bytes that the Sound Manager can use. Need to implement better mem management.
	static constexpr int PATH_LENGTH = 256; //Maximum Length of a file name.
	static constexpr float FADE = 100.0f; //Fade Duration in ms.
	static constexpr float VOL = 0.05f; //Volume in percentage of 1.
	static constexpr int NUM_CHANNELS = 2; //2 for Stereo.
	static constexpr int BIT_RATE = 16; //Bit Rate of the wav files.
	static constexpr int SAMPLE_RATE = 44100; //Number of Samples per second of the wav file.
	//X3DAUDIO
	static constexpr float CURVEDIST_SCALER = 0.1f; //Used for Dist Attenuation Calcs
	static constexpr float DOPPLER_SCALER = 0.1f; //Used For Doppler Effect Calcs
	static constexpr float CHANNEL_RADIUS = 10.0f; //Distance From EmitterPosition
	static constexpr float CHANNEL_INNERRADIUS = 10.0f; //InnerRadius of Spehere(?)
	static constexpr float CHANNEL_INNERRADIUS_ANGLE = X3DAUDIO_PI / 4.0f; //Angle for InnerRadius Calcs
	static constexpr float LEFT_AZIMUTH = 3 * X3DAUDIO_PI / 2;		//
	static constexpr float RIGHT_AZIMUTH = X3DAUDIO_PI / 2;			//
	static constexpr float FRONT_LEFT_AZIMUTH = 7 * X3DAUDIO_PI / 4;//
	static constexpr float FRONT_RIGHT_AZIMUTH = X3DAUDIO_PI / 4;	//
	static constexpr float FRONT_CENTER_AZIMUTH = 0.0f;				// -> Azimuth Values as per graph above.
	static constexpr float LOW_FREQUENCY_AZIMUTH = X3DAUDIO_2PI;	//
	static constexpr float BACK_LEFT_AZIMUTH = 5 * X3DAUDIO_PI / 4; //
	static constexpr float BACK_RIGHT_AZIMUTH = 3 * X3DAUDIO_PI / 4;//
	static constexpr float BACK_CENTER_AZIMUTH = X3DAUDIO_PI;		//

	static constexpr float c_channelAzimuths[9][8] =
	{
		/* 0 */   { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },
		/* 1 */   { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },
		/* 2 */   { FRONT_LEFT_AZIMUTH, FRONT_RIGHT_AZIMUTH, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },
		/* 2.1 */ { FRONT_LEFT_AZIMUTH, FRONT_RIGHT_AZIMUTH, LOW_FREQUENCY_AZIMUTH, 0.f, 0.f, 0.f, 0.f, 0.f },
		/* 4.0 */ { FRONT_LEFT_AZIMUTH, FRONT_RIGHT_AZIMUTH, BACK_LEFT_AZIMUTH, BACK_RIGHT_AZIMUTH, 0.f, 0.f, 0.f, 0.f },
		/* 4.1 */ { FRONT_LEFT_AZIMUTH, FRONT_RIGHT_AZIMUTH, LOW_FREQUENCY_AZIMUTH, BACK_LEFT_AZIMUTH, BACK_RIGHT_AZIMUTH, 0.f, 0.f, 0.f },
		/* 5.1 */ { FRONT_LEFT_AZIMUTH, FRONT_RIGHT_AZIMUTH, FRONT_CENTER_AZIMUTH, LOW_FREQUENCY_AZIMUTH, BACK_LEFT_AZIMUTH, BACK_RIGHT_AZIMUTH, 0.f, 0.f },
		/* 6.1 */ { FRONT_LEFT_AZIMUTH, FRONT_RIGHT_AZIMUTH, FRONT_CENTER_AZIMUTH, LOW_FREQUENCY_AZIMUTH, BACK_LEFT_AZIMUTH, BACK_RIGHT_AZIMUTH, BACK_CENTER_AZIMUTH, 0.f },
		/* 7.1 */ { FRONT_LEFT_AZIMUTH, FRONT_RIGHT_AZIMUTH, FRONT_CENTER_AZIMUTH, LOW_FREQUENCY_AZIMUTH, BACK_LEFT_AZIMUTH, BACK_RIGHT_AZIMUTH, LEFT_AZIMUTH, RIGHT_AZIMUTH }
	};
	

	//Structs

	enum SoundOptionsBits //Bitwise for combining options.
	{
		SOUND_OPTION_FADE_OUT = BIT(0),
		SOUND_OPTION_FADE_IN = BIT(1),
		SOUND_OPTION_START = BIT(2),
		SOUND_OPTION_LOOP = BIT(3),
		SOUND_OPTION_IMPORTANT = BIT(4),
	};

	typedef int SoundOptions;


	struct xAudioVoice : IXAudio2VoiceCallback //SourceVoice
	{
		IXAudio2SourceVoice* voice;
		SoundOptions options;
		float fadeTimer;
		char* soundPath;
		int playing;

		void STDMETHODCALLTYPE OnStreamEnd() override
		{
			voice->Stop();
			playing = false;
			voice->FlushSourceBuffers();
		}

		void STDMETHODCALLTYPE OnBufferStart(void* pBufferContext) override
		{
			playing = true;
		}

		void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
		void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 SamplesRequired) override {}
		void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override {}
		void STDMETHODCALLTYPE OnLoopEnd(void* pBufferContext) override {}
		void STDMETHODCALLTYPE OnVoiceError(void* pBufferContext, HRESULT Error) override {}

	};


	struct Sound
	{
		char path[PATH_LENGTH];
		SoundOptions options;
		int size;
		char* data = nullptr;
	};

	struct SpatialSound
	{
		Sound Sound;
		xAudioVoice* Voice;
	};
	
	
	struct wavheader //WAV Header as per canonical wav file documentation.
	{
		unsigned int riffChunkID;
		unsigned int riffChunkSize;
		unsigned int format;

		unsigned int formatChunkID;
		unsigned int formatChunkSize;
		unsigned short audioFormat;
		unsigned short numChannels;
		unsigned int sampleRate;
		unsigned int bitRate;
		unsigned short blockAlign;
		unsigned short bitsPerSample;

		unsigned char dataChunkID[4];
		unsigned int dataChunkSize;
	};

	struct wavfile //WAVFile
	{
		wavheader header;
		char dataBegin;
	};

	struct listener //X3DAUDIO Listener
	{
		X3DAUDIO_LISTENER* Listener;
		SceneNode* LSN = nullptr;
	};

	struct emitter //X3DAUDIO Emitter
	{
		X3DAUDIO_EMITTER Emitter;
		SceneNode* ESN = nullptr;
		int EmitIndex;
		xAudioVoice* BoundVoice;
	};
	
	//Methods
	//Constructors
	SoundManager(PE::GameContext& context, PE::MemoryArena arena, Handle hMyself);
	virtual ~SoundManager() {}
	virtual void addDefaultComponents()
	{
		Component::addDefaultComponents();
		
		PE_REGISTER_EVENT_HANDLER(Events::Event_UPDATE, SoundManager::do_UPDATE);
	}

	PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_UPDATE);
	virtual void do_UPDATE(Events::Event* pEvt);


	bool initXAudio(); //Initializes both X and X3D AUDIO
	void playSound(char* name, SoundOptions options = 0); //Method that puts a sound into the queue.
	void play3DSound(char* name, emitter* Emitter, SoundOptions options = 0); //Method that puts a sound into the queue.
	void stopSound(char* name); //Just Plays Sound with Fade Out as an Option.
	void stop3DSound(char* name, emitter* Emitter); //Just Plays Spatial Sound with Fade Out as an Option.
	void load_wav(char* path); //Method that parses a .wav file.
	void createListener(Handle HSN); //Creates an X3DAudio Listener (Called by and Bound to Camera for now).
	emitter* createEmitter(SceneNode* PSN, float m_CURVEDIST_SCALER = 0.1f); //Creates an X3DAudio Emitter that is bound to the SN Passed.
	void updateCameraListener(); //Updates Cam Position.
	void updateEmitters(); //Updates Emitter Positions.
	void calculateMatrices(); //Calculates and Sets Output Matrices per X3DAUDIO Emitter.

	//Vars
	listener cam; //X3DAUDIO Listener that is to be bound to the camera. (Moved to .cpp; pointer issues :/) 
																	   //(Moved Back here; Legendary W)
	IXAudio2* m_xaudio2; //Pointer to the XAudio2 Engine
	IXAudio2MasteringVoice* m_master_voice; //Pointer to the Master Voice
	DWORD m_dwChannelMask; //Pointer to the MasterVoice's Channel Mask, needed for X3DAudio.
	XAUDIO2_VOICE_DETAILS* m_masterVoiceDetails; //Pointer to the Master Voice's Details.
	X3DAUDIO_HANDLE X3DInstance; // Pointer to the X3DAUDIO Engine Instance

	int bytesUsed = 0; //Number of bytes used to allocate .wav data.
	int emittersCreated = 0; //Number of X3DAudio Emitters present, needed for updates and playback buffer settings.
	int availableVoices = MAX_CONCURRENT_SOUNDS; //Number of Source Voices that are available, is reduced when an Emitter is created and binds itself to a voice.
	char* allocatedsoundsBuffer; //Buffer that holds all of the loaded .wav data, Sounds point to this buffer.
	wavfile* pfile = nullptr; //temp var that points to a .wav file being loaded into the buffer above.

	Array<Sound> allocatedSounds; //Stores the names of sounds already loaded.
	Array<Sound> playingSounds; //Is the list of sounds that need to be pushed onto the buffer.
	Array<SpatialSound> playing3DSounds; //Is the list of Spatial sounds that need to be pushed onto the buffer.

	/* Old Constructor. Here for posterity.
	* 
	* 
	* 
	* 
	SoundManager(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself, const char* wbFilename, bool isActive = true): Component(context, arena, hMyself)
	{
		#if APIABSTRACTION_D3D9 || APIABSTRACTION_D3D11
		#endif
	}


	static void Construct(PE::GameContext &context, PE::MemoryArena arena, const char* wbFilename, bool isActive = true)
	{
		Handle h("SOUND_MANAGER", sizeof(SoundManager));
		SoundManager *pSoundManager = new(h) SoundManager(context, arena, h);
		pSoundManager->addDefaultComponents();
		
		

		
		
		
		

		SetInstance(h);
		s_isActive = isActive;
	}*/



	//Methods and Vars that I did not create
	static void SetInstance(Handle h){s_hInstance = h;}
	static SoundManager *Instance() {return s_hInstance.getObject<SoundManager>();}
	static Handle InstanceHandle() {return s_hInstance;}
	static Handle s_hInstance;
	static bool s_isActive;



};
}; // namespace Components
}; // namespace PE
#endif
