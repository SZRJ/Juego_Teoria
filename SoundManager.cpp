#include "SoundManager.h"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

bool SoundManager::init() {
    // 1. Inicializar el framework de Mixer
    if (!MIX_Init()) {
        SDL_Log("MIX_Init Error: %s", SDL_GetError());
        return false;
    }

    // 2. Crear el dispositivo de mezcla
    // Usamos SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK para el dispositivo de salida por defecto
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer) {
        SDL_Log("MIX_CreateMixerDevice Error: %s", SDL_GetError());
        return false;
    }

    // 3. Crear los tracks (son como los antiguos "canales", pero más potentes)
    musicTrack = MIX_CreateTrack(mixer);
    sfxTrack = MIX_CreateTrack(mixer);

    if (!musicTrack || !sfxTrack) {
        SDL_Log("Error al crear tracks de audio en SoundManager");
        return false;
    }

    return true;
}

void SoundManager::load(const std::string& id, const std::string& path, bool predecode) {
    // MIX_LoadAudio carga el archivo. 
    // predecode: true (SFX en RAM), false (Música en streaming)
    MIX_Audio* audio = MIX_LoadAudio(mixer, path.c_str(), predecode);

    if (audio) {
        clips[id] = audio;
    }
    else {
        SDL_Log("SoundManager: No se pudo cargar '%s'. Error: %s", path.c_str(), SDL_GetError());
    }
}

void SoundManager::playMusic(const std::string& id, bool loop) {
    if (clips.find(id) == clips.end()) return;

    // Asignar el audio al track de música
    MIX_SetTrackAudio(musicTrack, clips[id]);

    // En SDL3_mixer 3.2, los loops se pasan mediante propiedades
    SDL_PropertiesID props = SDL_CreateProperties();
    // -1 para loop infinito, 0 para tocar una sola vez
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0);

    MIX_PlayTrack(musicTrack, props);

    SDL_DestroyProperties(props);
}

void SoundManager::playSfx(const std::string& id, float volume) {
    if (clips.find(id) == clips.end()) return;

    // En la API moderna el volumen (Gain) es float de 0.0 a 1.0 (o más para amplificar)
    MIX_SetTrackGain(sfxTrack, volume);

    // Montar el SFX en el track y reproducir
    MIX_SetTrackAudio(sfxTrack, clips[id]);
    MIX_PlayTrack(sfxTrack, 0); // 0 significa usar propiedades por defecto
}

void SoundManager::stopMusic() {
    if (musicTrack) {
        MIX_StopTrack(musicTrack, 0); // 0 es el tiempo de fade-out
    }
}

void SoundManager::clear() {
    // 1. Liberar todos los clips de audio cargados
    for (auto& pair : clips) {
        if (pair.second) {
            MIX_DestroyAudio(pair.second);
        }
    }
    clips.clear();

    // 2. Destruir el mixer (esto limpia automáticamente sus tracks)
    if (mixer) {
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
        musicTrack = nullptr;
        sfxTrack = nullptr;
    }

    // 3. Salida limpia de Mixer
    MIX_Quit();
}