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

    void PlayMusic(const std::string& filePath, float volume = 1.0f, bool loop = true, float loopStartSeconds = 0.0f);
    void PlaySFX(const std::string& filePath, float volume = 1.0f);
    void PlaySFXDelayed(const std::string& filePath, float volume, float delaySeconds);
    void StopMusic();
    void FadeOutMusic(float duration);
    void PlayAmbientSFX(const std::string& filePath, float targetVolume = 1.0f, float fadeDuration = 1.0f);
    void FadeOutAmbientSFX(float duration);

    void SetMusicVolume(float volume);
private:
    AudioManager() = default;

    SDL_AudioDeviceID m_deviceId = 0;

    struct SoundData {
        Uint8* buffer;
        Uint32 length;
        SDL_AudioSpec spec;
    };

    struct DelayedSound {
        std::string filePath;
        float volume;
        float delayTimer;
    };
    std::vector<DelayedSound> m_delayedSounds;

    SoundData* LoadWav(const std::string& path);

    std::map<std::string, SoundData> m_soundCache;

    SDL_AudioStream* m_musicStream = nullptr;
    SoundData* m_currentMusicData = nullptr;

    float m_currentMusicVolume = 1.0f;
    bool m_isMusicLooping = false;
    float m_musicLoopStart = 0.0f;

    // Fading Variables
    bool m_isFadingOut = false;
    float m_fadeTimer = 0.0f;
    float m_fadeDuration = 0.0f;

    SDL_AudioStream* m_ambientStream{ nullptr };
    float m_ambientVolume{ 0.0f };
    float m_ambientTargetVolume{ 0.0f };
    float m_ambientFadeSpeed{ 0.0f };
    int m_ambientFadeState{ 0 };

    std::vector<SDL_AudioStream*> m_activeSFXStreams;
};