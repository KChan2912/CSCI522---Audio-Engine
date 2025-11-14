## Implementing the XAUDIO 3D Audio Engine within a Custom Engine for USC's CSCI-522

Integrated XAudio into the engine. 

**Usage:**

Bind an emitter to a SceneNode, 

Use *m_pContext->getSoundManager()->play3DSound(Filename, Emitter, SOUND_OPTION)* to play a sound.

*(View ClientCharacterControl.cpp for reference.)*

Use Audacity to create/convert files to the correct format.

Remember to initialize the SoundManager and add the SoundManager to the environment context.