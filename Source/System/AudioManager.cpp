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

void AudioManager::Update(float elapsedTime) {
    
    if (m_musicStream && m_isFadingOut) 
    {
        m_fadeTimer -= elapsedTime;

        if (m_fadeTimer <= 0.0f) 
        {
            StopMusic();
        }
        else 
        {
            float fadeRatio = m_fadeTimer / m_fadeDuration;
            float currentVolume = fadeRatio * m_currentMusicVolume; 

            SDL_SetAudioStreamGain(m_musicStream, currentVolume);
        }
    }

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

    for (auto it = m_delayedSounds.begin(); it != m_delayedSounds.end(); ) {
        it->delayTimer -= elapsedTime;
        if (it->delayTimer <= 0.0f) {
            PlaySFX(it->filePath, it->volume); // Waktunya habis, mainkan!
            it = m_delayedSounds.erase(it);    // Hapus dari antrean
        }
        else {
            ++it;
        }
    }


    if (m_ambientFadeState != 0 && m_ambientStream)
    {
        if (m_ambientFadeState == 1) 
        {
            m_ambientVolume += m_ambientFadeSpeed * elapsedTime;
            if (m_ambientVolume >= m_ambientTargetVolume)
            {
                m_ambientVolume = m_ambientTargetVolume;
                m_ambientFadeState = 0; 
            }
        }
        else if (m_ambientFadeState == -1)
        {
            m_ambientVolume -= m_ambientFadeSpeed * elapsedTime;
            if (m_ambientVolume <= 0.0f)
            {
                m_ambientVolume = 0.0f;
                m_ambientFadeState = 0;
                SDL_UnbindAudioStream(m_ambientStream);
                SDL_DestroyAudioStream(m_ambientStream);
                m_ambientStream = nullptr;
            }
        }
        SDL_SetAudioStreamGain(m_ambientStream, m_ambientVolume);
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

void AudioManager::PlayMusic(const std::string& filePath, float volume, bool loop, float loopStartSeconds) {
    StopMusic();

    SoundData* data = LoadWav(filePath);
    if (!data) return;

    m_musicStream = SDL_CreateAudioStream(&data->spec, NULL);
    if (!m_musicStream) return;

    SDL_BindAudioStream(m_deviceId, m_musicStream);
    SDL_PutAudioStreamData(m_musicStream, data->buffer, data->length);

    // [FIX] Gunakan parameter volume, bukan angka baku 1.0f
    SDL_SetAudioStreamGain(m_musicStream, volume);

    m_currentMusicVolume = volume;
    m_currentMusicData = data;
    m_isMusicLooping = loop;
    m_musicLoopStart = loopStartSeconds;
    m_isFadingOut = false;
}

void AudioManager::StopMusic() {
    if (m_musicStream) {
        SDL_UnbindAudioStream(m_musicStream);
        SDL_DestroyAudioStream(m_musicStream);
        m_musicStream = nullptr;
    }
    m_currentMusicData = nullptr;
    m_isFadingOut = false;
}

void AudioManager::FadeOutMusic(float duration) {
    if (m_musicStream && !m_isFadingOut) {
        m_isFadingOut = true;
        m_fadeDuration = duration;
        m_fadeTimer = duration;
    }
}

void AudioManager::PlaySFX(const std::string& filePath, float volume) {
    SoundData* data = LoadWav(filePath);
    if (!data) return;

    SDL_AudioStream* stream = SDL_CreateAudioStream(&data->spec, NULL);
    if (stream) {
        SDL_BindAudioStream(m_deviceId, stream);
        SDL_PutAudioStreamData(stream, data->buffer, data->length);
        SDL_SetAudioStreamGain(stream, volume);
        m_activeSFXStreams.push_back(stream);
    }
}

void AudioManager::PlayAmbientSFX(const std::string& filePath, float targetVolume, float fadeDuration)
{
    if (m_ambientStream)
    {
        SDL_UnbindAudioStream(m_ambientStream);
        SDL_DestroyAudioStream(m_ambientStream);
        m_ambientStream = nullptr;
    }

    SoundData* data = LoadWav(filePath);
    if (!data) return;

    m_ambientStream = SDL_CreateAudioStream(&data->spec, NULL);
    if (!m_ambientStream) return;

    SDL_BindAudioStream(m_deviceId, m_ambientStream);
    SDL_PutAudioStreamData(m_ambientStream, data->buffer, data->length);

    m_ambientVolume = 0.0f; 
    m_ambientTargetVolume = targetVolume;
    m_ambientFadeSpeed = targetVolume / fadeDuration;
    m_ambientFadeState = 1; // Flag Fading In

    SDL_SetAudioStreamGain(m_ambientStream, m_ambientVolume);
}

void AudioManager::FadeOutAmbientSFX(float duration)
{
    if (!m_ambientStream || m_ambientFadeState == -1) return;

    m_ambientTargetVolume = 0.0f;
    m_ambientFadeSpeed = m_ambientVolume / duration;
    m_ambientFadeState = -1; // Flag Fading Out
}


void AudioManager::PlaySFXDelayed(const std::string& filePath, float volume, float delaySeconds) {
    m_delayedSounds.push_back({ filePath, volume, delaySeconds });
}

void AudioManager::SetMusicVolume(float volume) {
    if (m_musicStream) {
        // Ini akan secara instan mengubah volume musik yang sedang berputar di SDL3
        SDL_SetAudioStreamGain(m_musicStream, volume);
        m_currentMusicVolume = volume;
    }
}