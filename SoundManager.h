#pragma once

#include <string>
#include <unordered_map>

// Declaraciones adelantadas para no saturar el header
struct MIX_Mixer;
struct MIX_Track;
struct MIX_Audio;

class SoundManager {
public:
    SoundManager() = default;

    // Inicializa el sistema de audio y crea los tracks (canales)
    bool init();

    // Carga un archivo de audio (WAV, OGG, MP3)
    // predecode = true para SFX cortos (RAM), false para música (Stream)
    void load(const std::string& id, const std::string& path, bool predecode);

    // Reproduce música en el track dedicado
    void playMusic(const std::string& id, bool loop = true);

    // Reproduce un efecto de sonido (SFX)
    void playSfx(const std::string& id, float volume = 1.0f);

    // Detiene toda la música
    void stopMusic();

    // Limpia la memoria y apaga el sistema
    void clear();

private:
    MIX_Mixer* mixer = nullptr;
    MIX_Track* musicTrack = nullptr;
    MIX_Track* sfxTrack = nullptr;

    // Diccionario para guardar los audios cargados por nombre (ID)
    std::unordered_map<std::string, MIX_Audio*> clips;
};
