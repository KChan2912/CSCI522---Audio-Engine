#include "SoundManager.h"
#include "PrimeEngine/MemoryManagement/Handle.h"
#include "PrimeEngine/PrimitiveTypes/PrimitiveTypes.h"
#include "PrimeEngine/FileSystem/FileReader.h"
#include "PrimeEngine/Utils/StringOps.h"
#include "PrimeEngine/MainFunction/MainFunctionArgs.h"
#include "PrimeEngine/Lua/LuaEnvironment.h"
#include "PrimeEngine/Scene/CameraSceneNode.h"

static PE::Components::SoundManager::xAudioVoice voiceArr[PE::Components::SoundManager::MAX_CONCURRENT_SOUNDS];
static PE::Components::SoundManager::emitter emitterArr[PE::Components::SoundManager::MAX_CONCURRENT_SOUNDS];

namespace PE {
namespace Components {
	using namespace PE::Events;
	Handle SoundManager::s_hInstance;
	bool SoundManager::s_isActive;

	PE_IMPLEMENT_CLASS1(SoundManager, Component);
	

	SoundManager::SoundManager(PE::GameContext& context, PE::MemoryArena arena, Handle hMyself)
		: Component(context, arena, hMyself), allocatedSounds(context, arena, 64),
		playingSounds(context, arena, MAX_CONCURRENT_SOUNDS), playing3DSounds(context, arena, MAX_CONCURRENT_SOUNDS)
	{
		bool init = initXAudio();
	}

	bool SoundManager::initXAudio() {
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(hr)) {
			return false;
		}
		static IXAudio2* xaudio2 = nullptr;
		hr = XAudio2Create(&xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(hr)) {
			return false;
		}
		m_xaudio2 = xaudio2;
		static IXAudio2MasteringVoice* master_voice = nullptr;
		hr = xaudio2->CreateMasteringVoice(&master_voice);
		if (FAILED(hr)) {
			return false;
		}
		m_master_voice = master_voice;

		WAVEFORMATEX wave = {};
		wave.wFormatTag = WAVE_FORMAT_PCM;
		wave.nChannels = NUM_CHANNELS;
		wave.nSamplesPerSec = SAMPLE_RATE;
		wave.wBitsPerSample = BIT_RATE;
		wave.nBlockAlign = (wave.nChannels * wave.wBitsPerSample)/8;
		wave.nAvgBytesPerSec = wave.nSamplesPerSec * wave.nBlockAlign;

		for (int i = 0; i < MAX_CONCURRENT_SOUNDS; i++)
		{
			xAudioVoice* voice = &voiceArr[i];
			hr = xaudio2->CreateSourceVoice(&voice->voice, &wave, 0, XAUDIO2_DEFAULT_FREQ_RATIO, voice, nullptr, nullptr);
			if (FAILED(hr)) {
				return false;
			}
			voice->voice->SetVolume(VOL);
			

		}

		static char* sBuffer = new char[SOUNDS_BUFFER_SIZE];
		allocatedsoundsBuffer = sBuffer;

		DWORD dwChannelMask;
		master_voice->GetChannelMask(&dwChannelMask);
		m_dwChannelMask = dwChannelMask;

		static XAUDIO2_VOICE_DETAILS masterVoiceDetails = {};
		master_voice->GetVoiceDetails(&masterVoiceDetails);
		m_masterVoiceDetails = &masterVoiceDetails;

		hr = X3DAudioInitialize(dwChannelMask, X3DAUDIO_SPEED_OF_SOUND, X3DInstance);
		if (FAILED(hr)) {
			return false;
		}
		
		return true;
	
	}

	void SoundManager::playSound(char* name, SoundOptions options)//plays with no spatial data, most likely
		//move this to a bgSoundManager instead, keep this file for soundfx only.
	{
		PEASSERT(name, "NO FILENAME");

		options = options ? options : SOUND_OPTION_START;
		if (!(options & SOUND_OPTION_START) &&
			!(options & SOUND_OPTION_FADE_IN) &&
			!(options & SOUND_OPTION_FADE_OUT))
		{
			options |= SOUND_OPTION_START;
		}

		Sound sound = {};
		sound.options = options;
		sprintf(sound.path, "%s.wav", name);

		//Check Loaded Sounds
		for (int i = 0; i < allocatedSounds.m_size; i++)
		{
			Sound loadedSound = allocatedSounds[i];
			if (strcmp(loadedSound.path, sound.path) == 0)
			{
				loadedSound.options = sound.options;
				playingSounds.add(loadedSound);
				return;
			}
		}
		//Else Load new Sound
		load_wav(sound.path);
		if (pfile)
		{
			if (pfile->header.dataChunkSize > SOUNDS_BUFFER_SIZE - bytesUsed)
			{
				PEASSERT(0,"Out of Memory");
				pfile = nullptr;
				return;
			}
			sound.size = pfile->header.dataChunkSize;
			sound.data = &allocatedsoundsBuffer[bytesUsed];
			bytesUsed += sound.size;
			memcpy(sound.data, &pfile->dataBegin, sound.size);

			allocatedSounds.add(sound);
			playingSounds.add(sound);
			pfile = nullptr;
			
		}
	}

	void SoundManager::stopSound(char* name)
	{
		playSound(name, SOUND_OPTION_FADE_OUT);
	}

	void SoundManager::play3DSound(char* name, emitter* Emitter, SoundOptions options)
	{
		PEASSERT(name, "NO FILENAME");

		options = options ? options : SOUND_OPTION_START;
		if (!(options & SOUND_OPTION_START) &&
			!(options & SOUND_OPTION_FADE_IN) &&
			!(options & SOUND_OPTION_FADE_OUT))
		{
			options |= SOUND_OPTION_START;
		}

		Sound sound = {};
		sound.options = options;
		sprintf(sound.path, "%s.wav", name);

		//Check Loaded Sounds
		for (int i = 0; i < allocatedSounds.m_size; i++)
		{
			Sound loadedSound = allocatedSounds[i];
			if (strcmp(loadedSound.path, sound.path) == 0)
			{
				loadedSound.options = sound.options;

				SpatialSound spatsound = {};
				spatsound.Voice = Emitter->BoundVoice;
				spatsound.Sound = loadedSound;
				playing3DSounds.add(spatsound);
				return;
			}
		}
		//Else Load new Sound
		load_wav(sound.path);
		if (pfile)
		{
			if (pfile->header.dataChunkSize > SOUNDS_BUFFER_SIZE - bytesUsed)
			{
				PEASSERT(0, "Out of Memory");
				pfile = nullptr;
				return;
			}
			sound.size = pfile->header.dataChunkSize;
			sound.data = &allocatedsoundsBuffer[bytesUsed];
			bytesUsed += sound.size;
			memcpy(sound.data, &pfile->dataBegin, sound.size);

			allocatedSounds.add(sound);
			SpatialSound spatsound = {};
			spatsound.Voice = Emitter->BoundVoice;
			spatsound.Sound = sound;
			playing3DSounds.add(spatsound);
			pfile = nullptr;

		}
	}

	void SoundManager::stop3DSound(char* name, emitter* Emitter)
	{
		play3DSound(name, Emitter, SOUND_OPTION_FADE_OUT);
	}

	void SoundManager::load_wav(char* path)
	{
		PEString::generatePathname(*m_pContext, path, "Default", "Sounds", PEString::s_buf, PEString::BUF_SIZE);
		FileReader f(PEString::s_buf);

		char* pFileContents = 0;
		PrimitiveTypes::UInt32 fileSize = 0;
		f.readIntoBuffer(pFileContents, fileSize);

		wavfile* nfile = (wavfile*)pFileContents;
		if (!nfile)
		{
			PEASSERT(0, "Failed to load wav");
		}
		PEASSERT(nfile->header.numChannels == NUM_CHANNELS, "Only 2 Channels are supported");
		PEASSERT(nfile->header.sampleRate == SAMPLE_RATE, "44100Hz Needed");
		PEASSERT(memcmp(&nfile->header.dataChunkID, "data", 4) == 0, "ImproperFormat");

		pfile = nfile;
		nfile = nullptr;
	}



	SoundManager::emitter* SoundManager::createEmitter(SceneNode* PSN, float m_CURVEDIST_SCALER)
	{
		//work here
		emitter Emitter = {};
		Emitter.ESN = PSN;
		
		X3DAUDIO_EMITTER XEmitter = {};

		XEmitter.ChannelCount = NUM_CHANNELS;
		XEmitter.pChannelAzimuths = (NUM_CHANNELS <= 8 && NUM_CHANNELS > 1) ? (FLOAT32*)&c_channelAzimuths[NUM_CHANNELS] : nullptr;
		XEmitter.CurveDistanceScaler = m_CURVEDIST_SCALER;
		XEmitter.DopplerScaler = DOPPLER_SCALER;
		Vector3 pos = PSN->m_base.getPos();
		Vector3 front = PSN->m_base.getN();
		Vector3 up = PSN->m_base.getU();
		XEmitter.Position = { pos.getX(), pos.getY(), pos.getZ() };
		XEmitter.OrientFront = { front.getX(), front.getY(), front.getZ() };
		XEmitter.OrientTop = {up.getX(), up.getY(), up.getZ()};
		XEmitter.Velocity = { 0.0f, 0.0f, 0.0f };
		XEmitter.OrientFront = { 1.0f, 0.0f, 0.0f };
		XEmitter.OrientTop = { 0.0f, 0.0f, 1.0f };
		XEmitter.ChannelRadius = CHANNEL_RADIUS;
		XEmitter.InnerRadius = CHANNEL_INNERRADIUS;
		XEmitter.InnerRadiusAngle = CHANNEL_INNERRADIUS_ANGLE;

		Emitter.Emitter = XEmitter;
		int EmitterVIndex = availableVoices - 1;
		Emitter.BoundVoice = &voiceArr[EmitterVIndex];

		//xAudioVoice* voicetemp = &voiceArr[EmitterVIndex]; //sanity was not maintained :/
		//xAudioVoice* voicetemp2 = &voiceArr[15];

		availableVoices = availableVoices - 1;

		Emitter.EmitIndex = emittersCreated;
		emittersCreated = emittersCreated + 1;

		emitterArr[Emitter.EmitIndex] = Emitter;
		
		emitter* emitterAdd = &emitterArr[Emitter.EmitIndex];
		

		return emitterAdd;//Return Pointer to the Emitter, allows for the object to play spatial audio
		//without having to run through the whole list of emitters just to find the one it is bound to.
	}

	void SoundManager::createListener(Handle HSN)
	{
		CameraSceneNode* CSN = HSN.getObject<CameraSceneNode>();
		cam.LSN = CSN;

		Vector3 pos = CSN->m_base.getPos();
		Vector3 front = CSN->m_base.getN();
		Vector3 up = CSN->m_base.getU();
		
		static X3DAUDIO_LISTENER listener{};
		listener.Position = {pos.getX(), pos.getY(), pos.getZ()};
		listener.Velocity = { 0.0f, 0.0f, 0.0f };
		listener.OrientFront = { front.getX(), front.getY(), front.getZ()};
		listener.OrientTop = { up.getX(), up.getY(), up.getZ()};

		cam.Listener = &listener;
	}

	
	

	void SoundManager::updateCameraListener() 
	{
		Vector3 pos = cam.LSN->m_base.getPos();
		Vector3 front = cam.LSN->m_base.getN();
		Vector3 up = cam.LSN->m_base.getU();
		cam.Listener->Position = { pos.getX(),pos.getY(),pos.getZ() };
		cam.Listener->OrientFront = { front.getX(), front.getY(), front.getZ() };
		cam.Listener->OrientTop = { front.getX(), front.getY(), front.getZ() };
		//std::cout << pos.getX()<<std::endl; //sanity maintained
	}

	void SoundManager::updateEmitters() 
	{
		emitter* CurrEmit = nullptr;
		for (int i = 0; i < emittersCreated; i++) //For Every Emitter Created
		{
			CurrEmit = &emitterArr[i];
			Vector3 pos = CurrEmit->ESN->m_base.getPos();
			Vector3 front = CurrEmit->ESN->m_base.getN();
			Vector3 up = CurrEmit->ESN->m_base.getU();
			CurrEmit->Emitter.Position = { pos.getX(),pos.getY(),pos.getZ() };
			CurrEmit->Emitter.OrientFront = { front.getX(),front.getY(),front.getZ() };
			CurrEmit->Emitter.OrientTop = { up.getX(),up.getY(),up.getZ() };
			//std::cout << pos.getX() << std::endl; //sanity maintained
		}
		CurrEmit = nullptr;
	}

	void SoundManager::calculateMatrices()
	{
		emitter* CurrEmit = nullptr;
		for (int i = 0; i < emittersCreated; i++) //For Every Emitter Created
		{
			CurrEmit = &emitterArr[i];
			X3DAUDIO_DSP_SETTINGS DSPSettings = {}; //Matrix that receives the result of 3DCalc.
			FLOAT32* matrix = new FLOAT32[NUM_CHANNELS*m_masterVoiceDetails->InputChannels]; //Resultant O/P Matrix. Must be of size (Input Channels * Dest Channels) at the least.
			DSPSettings.SrcChannelCount = NUM_CHANNELS; // Number of Channels of Input.
			DSPSettings.DstChannelCount = m_masterVoiceDetails->InputChannels; //Number of Channels of Destination.
			DSPSettings.pMatrixCoefficients = matrix; //Pointer of the Result Matrix.
			X3DAUDIO_LISTENER* list = cam.Listener;

			X3DAudioCalculate(X3DInstance, cam.Listener, &CurrEmit->Emitter, //X3DAudioInst, Listener, Emitter
				X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER |  //Options
				X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_REVERB, //Options
				&DSPSettings);//Where to Store Result

			CurrEmit->BoundVoice->voice->SetOutputMatrix(m_master_voice, //Destination
				NUM_CHANNELS, //Source Channels
				m_masterVoiceDetails->InputChannels, //Dest. Channels
				DSPSettings.pMatrixCoefficients //Result of Calc
			);
			CurrEmit->BoundVoice->voice->SetFrequencyRatio(DSPSettings.DopplerFactor);
		}

	}

	void SoundManager::do_UPDATE(PE::Events::Event* pEvt)
	{
		updateCameraListener(); //need new pos
		updateEmitters();//need new pos
		//Calculate and Set the Output Matrices for each Emitter and Voice Pair Here...

		calculateMatrices();



		//3D Sounds
		for (int i = 0; i < playing3DSounds.m_size; i++)
		{
			SpatialSound& SpatSound = playing3DSounds[i];
			Sound sound = SpatSound.Sound;
			
			PEASSERT(sound.size > 0, "Sound has no Samples Size");
			PEASSERT(sound.data, "Sound has no Data");
			
			//Bind Sound to the Emitter's Voice
			if (sound.options & SOUND_OPTION_START || sound.options & SOUND_OPTION_FADE_IN)
			{	
				
				xAudioVoice* voice;
				
				voice = SpatSound.Voice;

				
				if (voice->playing)
				{
					//if new sound is important and the bound voice 
					//is currently busy, free the bound voice and then play the new sound. //FIX LATER
					if (sound.options & SOUND_OPTION_IMPORTANT)
					{
						if (strcmp(voice->soundPath, sound.path) != 0)
						{
							voice->voice->Stop();
							voice->voice->FlushSourceBuffers();
							voice->playing = false;
							voice->voice->SetVolume(VOL);
							voice->fadeTimer = 0.0f;
						}
						else
						{
							continue;
						}
					}
					else 
					{
						continue;
					}
					
				}
				
				XAUDIO2_BUFFER buffer = {};
				
				buffer.Flags = XAUDIO2_END_OF_STREAM;
				
				buffer.AudioBytes = sound.size;
				buffer.pAudioData = (BYTE*)sound.data;
				
				buffer.LoopCount = sound.options & SOUND_OPTION_LOOP ? XAUDIO2_MAX_LOOP_COUNT : 0;
				
				HRESULT hr = voice->voice->SubmitSourceBuffer(&buffer);

				
				if (!FAILED(hr))
				{
					voice->voice->Start();
					voice->soundPath = sound.path;
					voice->options = sound.options;
					InterlockedExchange((LONG*)&voice->playing, true);//Ensure Atomicity of Read/Writes.

				}
			}
			
			if (sound.options & SOUND_OPTION_FADE_OUT)
			{
				xAudioVoice* voice = SpatSound.Voice;
				voice->options = SOUND_OPTION_FADE_OUT;
			}
			
		}
		playing3DSounds.m_size = 0;
		
		//Non-Spatial Audio
		for (int i = 0; i < playingSounds.m_size; i++)
		{
			Sound& sound = playingSounds[i];
			PEASSERT(sound.size > 0, "Sound has no Samples Size");
			PEASSERT(sound.data, "Sound has no Data");
			//Bind Sound to a voice upon starting
			if (sound.options & SOUND_OPTION_START || sound.options & SOUND_OPTION_FADE_IN)
			{
				xAudioVoice* voice = nullptr;
				for (int voiceIndex = 0; voiceIndex < availableVoices; voiceIndex++)
				{
					xAudioVoice* pVoice = &voiceArr[voiceIndex];
					if (!pVoice->playing)
					{
						voice = pVoice;
						break;
					}
				}

				if (voice != nullptr)
				{
					XAUDIO2_BUFFER buffer = {};
					buffer.Flags = XAUDIO2_END_OF_STREAM;
					buffer.AudioBytes = sound.size;
					buffer.pAudioData = (BYTE*)sound.data;
					buffer.LoopCount = sound.options & SOUND_OPTION_LOOP ? XAUDIO2_MAX_LOOP_COUNT : 0;

					HRESULT hr = voice->voice->SubmitSourceBuffer(&buffer);
					if (!FAILED(hr))
					{
						voice->voice->Start();
						voice->soundPath = sound.path;
						voice->options = sound.options;
						InterlockedExchange((LONG*)&voice->playing, true);//Ensure Atomicity of Read/Writes.

					}
				}
			}

			//Unbind a voice when ending
			if (sound.options & SOUND_OPTION_FADE_OUT)
			{
				xAudioVoice* voice = nullptr;
				for (int voiceIndex = 0; voiceIndex < availableVoices; voiceIndex++)
				{
					xAudioVoice* pVoice = &voiceArr[voiceIndex];
					if (!pVoice->playing)
					{
						continue;
					}

					if (strcmp(pVoice->soundPath, sound.path) == 0)
					{
						pVoice->options = SOUND_OPTION_FADE_OUT;
					}
				}
			}

			
			
		}
		playingSounds.m_size = 0;

		//Update Loop for Fade in and Fade Out
		for (int voiceIndex = 0; voiceIndex < MAX_CONCURRENT_SOUNDS; voiceIndex++)
		{
			xAudioVoice* voice = &voiceArr[voiceIndex];
			if (!voice->playing)
			{
				continue;
			}
			if (voice->options & SOUND_OPTION_FADE_IN)
			{
				Event_UPDATE* pRealEvt = (Event_UPDATE*)(pEvt);
				voice->fadeTimer = std::min(voice->fadeTimer + pRealEvt->m_frameTime, FADE);
				float fade = voice->fadeTimer / FADE;
				voice->voice->SetVolume(fade * VOL);

				if (voice->fadeTimer == FADE)
				{
					voice->options ^= SOUND_OPTION_FADE_IN; //Get Rid of Fade In so it stops entering this loop.
					voice->fadeTimer = 0.0f;

				}

				continue; // Assuming that a sound cannot have both Fade In and Fade Out.
				//Even if it does, sound first fades in and then fades out, instead of immediately hitting the fade out loop.

			}

			if (voice->options & SOUND_OPTION_FADE_OUT) 
			{
				Event_UPDATE* pRealEvt = (Event_UPDATE*)(pEvt);
				voice->fadeTimer = std::min(voice->fadeTimer + pRealEvt->m_frameTime, FADE);
				float fade = 1.0f - voice->fadeTimer / FADE;
				voice->voice->SetVolume(fade * VOL);

				if (voice->fadeTimer == FADE)
				{
					voice->options ^= SOUND_OPTION_FADE_OUT; //Get Rid of Fade Out so it stops entering this loop.
					voice->voice->Stop();
					voice->voice->FlushSourceBuffers();
					voice->playing = false;
					voice->voice->SetVolume(VOL);
					voice->fadeTimer = 0.0f;

				}

			}
		}
	}


}; // namespace Components
}; // namespace PE