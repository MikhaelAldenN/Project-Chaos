#pragma once
#include <string>
#include <map>
#include <vector>
#include <SDL3/SDL.h>

class AudioManager
{
public:
    static AudioManager& Instance();

    bool Initialize();
    void Finalize();

    void Update(float elapsedTime);

    void PlayMusic(const std::string& filePath, bool loop = true, float loopStartSeconds = 0.0f);
    void PlaySFX(const std::string& filePath, float volume = 1.0f);
    void StopMusic();
    void FadeOutMusic(float duration);

private:
    AudioManager() = default;

    SDL_AudioDeviceID m_deviceId = 0;

    struct SoundData {
        Uint8* buffer;
        Uint32 length;
        SDL_AudioSpec spec;
    };

    SoundData* LoadWav(const std::string& path);

    std::map<std::string, SoundData> m_soundCache;

    SDL_AudioStream* m_musicStream = nullptr;
    SoundData* m_currentMusicData = nullptr;

    bool m_isMusicLooping = false;
    float m_musicLoopStart = 0.0f;

    // Fading Variables
    bool m_isFadingOut = false;
    float m_fadeTimer = 0.0f;
    float m_fadeDuration = 0.0f;

    std::vector<SDL_AudioStream*> m_activeSFXStreams;
};