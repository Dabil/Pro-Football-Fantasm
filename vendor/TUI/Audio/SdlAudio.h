#pragma once

#include <filesystem>
#include <memory>

namespace Audio
{
    class SdlAudio
    {
    public:
        SdlAudio();
        ~SdlAudio();

        bool initialize();
        void shutdown();
        bool playMusic(const std::filesystem::path& path);
        bool musicPlaying() const;
        void pauseMusic();
        void resumeMusic();
        void playEffect(const std::filesystem::path& path);
        void stopEffect();

    private:
        struct State;
        std::unique_ptr<State> m_state;
    };
}
