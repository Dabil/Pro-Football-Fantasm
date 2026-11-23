#include "Audio/SdlAudio.h"

#if defined(_M_X64)
#include <SDL3_mixer/SDL_mixer.h>

#define NOMINMAX
#include <windows.h>

#include <iostream>
#include <map>
#endif

namespace Audio
{
    struct SdlAudio::State
    {
#if defined(_M_X64)
        HMODULE sdl = nullptr;
        HMODULE library = nullptr;
        bool audioInitialized = false;
        bool mixerInitialized = false;
        MIX_Mixer* mixer = nullptr;
        MIX_Track* music = nullptr;
        MIX_Track* effect = nullptr;
        MIX_Audio* musicAudio = nullptr;
        std::map<std::filesystem::path, MIX_Audio*> effects;

        decltype(&SDL_InitSubSystem) initAudio = nullptr;
        decltype(&SDL_QuitSubSystem) quitAudio = nullptr;
        decltype(&SDL_GetError) error = nullptr;
        decltype(&MIX_Init) initMixer = nullptr;
        decltype(&MIX_Quit) quitMixer = nullptr;
        decltype(&MIX_CreateMixerDevice) createMixer = nullptr;
        decltype(&MIX_DestroyMixer) destroyMixer = nullptr;
        decltype(&MIX_CreateTrack) createTrack = nullptr;
        decltype(&MIX_SetTrackGain) setGain = nullptr;
        decltype(&MIX_LoadAudio) loadAudio = nullptr;
        decltype(&MIX_DestroyAudio) destroyAudio = nullptr;
        decltype(&MIX_SetTrackAudio) setAudio = nullptr;
        decltype(&MIX_PlayTrack) play = nullptr;
        decltype(&MIX_TrackPlaying) playing = nullptr;
        decltype(&MIX_PauseTrack) pause = nullptr;
        decltype(&MIX_ResumeTrack) resume = nullptr;
        decltype(&MIX_StopTrack) stop = nullptr;

        ~State()
        {
            if (mixer) destroyMixer(mixer);
            if (musicAudio) destroyAudio(musicAudio);
            for (const auto& entry : effects)
                if (entry.second) destroyAudio(entry.second);
            if (mixerInitialized) quitMixer();
            if (audioInitialized) quitAudio(SDL_INIT_AUDIO);
            if (library) FreeLibrary(library);
            if (sdl) FreeLibrary(sdl);
        }

        template <class Function>
        static bool load(HMODULE module, Function& function, const char* name)
        {
            function = reinterpret_cast<Function>(GetProcAddress(module, name));
            return function != nullptr;
        }

        bool loadRuntime()
        {
            wchar_t executable[32768];
            const DWORD length = GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
            if (!length || length >= ARRAYSIZE(executable)) return false;
            const auto directory = std::filesystem::path(executable).parent_path();
            constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
            sdl = LoadLibraryExW((directory / L"SDL3.dll").c_str(), nullptr, flags);
            if (!sdl) return false;
            library = LoadLibraryExW((directory / L"SDL3_mixer.dll").c_str(), nullptr, flags);
            return library &&
                load(sdl, initAudio, "SDL_InitSubSystem") &&
                load(sdl, quitAudio, "SDL_QuitSubSystem") &&
                load(sdl, error, "SDL_GetError") &&
                load(library, initMixer, "MIX_Init") &&
                load(library, quitMixer, "MIX_Quit") &&
                load(library, createMixer, "MIX_CreateMixerDevice") &&
                load(library, destroyMixer, "MIX_DestroyMixer") &&
                load(library, createTrack, "MIX_CreateTrack") &&
                load(library, setGain, "MIX_SetTrackGain") &&
                load(library, loadAudio, "MIX_LoadAudio") &&
                load(library, destroyAudio, "MIX_DestroyAudio") &&
                load(library, setAudio, "MIX_SetTrackAudio") &&
                load(library, play, "MIX_PlayTrack") &&
                load(library, playing, "MIX_TrackPlaying") &&
                load(library, pause, "MIX_PauseTrack") &&
                load(library, resume, "MIX_ResumeTrack") &&
                load(library, stop, "MIX_StopTrack");
        }

        bool openDevice()
        {
            audioInitialized = initAudio(SDL_INIT_AUDIO);
            if (!audioInitialized) return false;
            mixerInitialized = initMixer();
            if (!mixerInitialized) return false;
            mixer = createMixer(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
            if (!mixer) return false;
            music = createTrack(mixer);
            effect = createTrack(mixer);
            return music && effect && setGain(music, 0.24f) && setGain(effect, 0.60f);
        }
#endif
    };

    SdlAudio::SdlAudio() = default;
    SdlAudio::~SdlAudio() = default;

    bool SdlAudio::initialize()
    {
#if defined(_M_X64)
        if (m_state) return true;
        auto state = std::make_unique<State>();
        if (!state->loadRuntime())
        {
            std::cerr << "TUI audio unavailable. Check SDL3.dll and SDL3_mixer.dll beside the executable.\n";
            return false;
        }
        if (!state->openDevice())
        {
            std::cerr << "TUI audio unavailable. " << state->error() << '\n';
            return false;
        }
        m_state = std::move(state);
        return true;
#else
        return false;
#endif
    }

    void SdlAudio::shutdown() { m_state.reset(); }

    bool SdlAudio::playMusic(const std::filesystem::path& path)
    {
#if defined(_M_X64)
        if (!m_state) return false;
        auto& state = *m_state;
        const auto name = path.u8string();
        auto* audio = state.loadAudio(state.mixer, name.c_str(), false);
        if (audio && state.setAudio(state.music, audio))
        {
            if (state.musicAudio) state.destroyAudio(state.musicAudio);
            state.musicAudio = audio;
            if (state.play(state.music, 0)) return true;
        }
        else if (audio)
            state.destroyAudio(audio);
        std::cerr << "TUI could not play " << name << ". " << state.error() << '\n';
#else
        (void)path;
#endif
        return false;
    }

    bool SdlAudio::musicPlaying() const
    {
#if defined(_M_X64)
        return m_state && m_state->playing(m_state->music);
#else
        return false;
#endif
    }

    void SdlAudio::pauseMusic()
    {
#if defined(_M_X64)
        if (musicPlaying()) m_state->pause(m_state->music);
#endif
    }

    void SdlAudio::resumeMusic()
    {
#if defined(_M_X64)
        if (m_state && m_state->musicAudio) m_state->resume(m_state->music);
#endif
    }

    void SdlAudio::playEffect(const std::filesystem::path& path)
    {
#if defined(_M_X64)
        if (!m_state) return;
        auto& state = *m_state;
        const auto [entry, inserted] = state.effects.try_emplace(path, nullptr);
        if (inserted)
        {
            const auto name = path.u8string();
            entry->second = state.loadAudio(state.mixer, name.c_str(), true);
            if (!entry->second)
                std::cerr << "TUI could not load " << name << ". " << state.error() << '\n';
        }
        if (entry->second && state.setAudio(state.effect, entry->second))
            state.play(state.effect, 0);
#else
        (void)path;
#endif
    }

    void SdlAudio::stopEffect()
    {
#if defined(_M_X64)
        if (m_state) m_state->stop(m_state->effect, 0);
#endif
    }
}
