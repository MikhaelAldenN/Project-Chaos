#include "AudioManager.h"
#include <iostream>
#include <algorithm> 

AudioManager& AudioManager::Instance() {
    static AudioManager instance;
    return instance;
}

bool AudioManager::Initialize() {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_Log("SDL Audio Init Failed: %s", SDL_GetError());
        return false;
    }

    m_deviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!m_deviceId) {
        SDL_Log("Failed to open audio device: %s", SDL_GetError());
        return false;
    }

    SDL_ResumeAudioDevice(m_deviceId);
    SDL_Log("Audio System Initialized (SDL3 Core)");
    return true;
}

void AudioManager::Finalize() {
    StopMusic();

    for (auto stream : m_activeSFXStreams) {
        SDL_DestroyAudioStream(stream);
    }
    m_activeSFXStreams.clear();

    for (auto& pair : m_soundCache) {
        SDL_free(pair.second.buffer);
    }
    m_soundCache.clear();

    SDL_CloseAudioDevice(m_deviceId);
}

void AudioManager::Update() {
    auto it = std::remove_if(m_activeSFXStreams.begin(), m_activeSFXStreams.end(),
        [](SDL_AudioStream* stream) {
            if (SDL_GetAudioStreamAvailable(stream) == 0) {
                SDL_DestroyAudioStream(stream);
                return true;
            }
            return false;
        });
    m_activeSFXStreams.erase(it, m_activeSFXStreams.end());

    
    if (m_musicStream && m_isMusicLooping && m_currentMusicData) 
    {
        if (SDL_GetAudioStreamAvailable(m_musicStream) == 0) 
        {
            int bytesPerSample = SDL_AUDIO_BITSIZE(m_currentMusicData->spec.format) / 8;
            int frameSize = bytesPerSample * m_currentMusicData->spec.channels;
            int bytesPerSecond = m_currentMusicData->spec.freq * frameSize;

            Uint32 offsetBytes = (Uint32)(m_musicLoopStart * bytesPerSecond);

            if (offsetBytes >= m_currentMusicData->length) {
                offsetBytes = 0; 
            }

            Uint8* loopStartPointer = m_currentMusicData->buffer + offsetBytes;
            Uint32 loopLength = m_currentMusicData->length - offsetBytes;

            SDL_PutAudioStreamData(m_musicStream, loopStartPointer, loopLength);
        }
    }
}

AudioManager::SoundData* AudioManager::LoadWav(const std::string& path) {
    if (m_soundCache.find(path) == m_soundCache.end()) {
        SoundData data;
        if (SDL_LoadWAV(path.c_str(), &data.spec, &data.buffer, &data.length) < 0) {
            SDL_Log("Failed to load WAV: %s", path.c_str());
            return nullptr;
        }
        m_soundCache[path] = data;
    }
    return &m_soundCache[path];
}

void AudioManager::PlayMusic(const std::string& filePath, bool loop, float loopStartSeconds) {
    StopMusic();

    SoundData* data = LoadWav(filePath);
    if (!data) return;

    m_musicStream = SDL_CreateAudioStream(&data->spec, NULL);
    if (!m_musicStream) return;

    SDL_BindAudioStream(m_deviceId, m_musicStream);
    SDL_PutAudioStreamData(m_musicStream, data->buffer, data->length);

    m_currentMusicData = data;
    m_isMusicLooping = loop;
    m_musicLoopStart = loopStartSeconds; 
}

void AudioManager::StopMusic() {
    if (m_musicStream) {
        SDL_UnbindAudioStream(m_musicStream);
        SDL_DestroyAudioStream(m_musicStream);
        m_musicStream = nullptr;
    }
    m_currentMusicData = nullptr;
}

void AudioManager::PlaySFX(const std::string& filePath, float volume) {
    SoundData* data = LoadWav(filePath);
    if (!data) return;

    SDL_AudioStream* stream = SDL_CreateAudioStream(&data->spec, NULL);
    if (stream) {
        SDL_BindAudioStream(m_deviceId, stream);
        SDL_PutAudioStreamData(stream, data->buffer, data->length);
        m_activeSFXStreams.push_back(stream);
    }
}