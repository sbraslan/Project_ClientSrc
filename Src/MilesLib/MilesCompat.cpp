#define MINIAUDIO_IMPLEMENTATION
#include "MilesCompat.h"

#include <algorithm>
#include <cstring>
#include <new>

struct MilesCompatEngine
{
    ma_engine engine{};
    bool initialized = false;
};

struct MilesCompatProvider
{
    HDIGDRIVER driver = nullptr;
    char name[64]{};
};

struct MilesCompatSound
{
    HDIGDRIVER driver = nullptr;
    ma_sound sound{};
    ma_decoder decoder{};
    bool soundInitialized = false;
    bool decoderInitialized = false;
    bool is3D = false;
    bool paused = false;
    bool stopped = true;
};

namespace
{
HDIGDRIVER g_driver = nullptr;
MilesCompatProvider g_provider{};
thread_local char g_lastError[256] = {};

void SetLastErrorText(const char* text)
{
    strncpy_s(g_lastError, text ? text : "unknown audio error", _TRUNCATE);
}

void ResetSound(MilesCompatSound* sample)
{
    if (!sample)
        return;

    if (sample->soundInitialized)
    {
        ma_sound_uninit(&sample->sound);
        sample->soundInitialized = false;
    }

    if (sample->decoderInitialized)
    {
        ma_decoder_uninit(&sample->decoder);
        sample->decoderInitialized = false;
    }

    sample->paused = false;
    sample->stopped = true;
}

bool InitSoundFromMemory(MilesCompatSound* sample, const void* data, size_t size)
{
    if (!sample || !sample->driver || !data || size == 0)
        return false;

    ResetSound(sample);

    if (ma_decoder_init_memory(data, size, nullptr, &sample->decoder) != MA_SUCCESS)
    {
        SetLastErrorText("miniaudio: failed to decode sound from memory");
        return false;
    }
    sample->decoderInitialized = true;

    const ma_uint32 flags = sample->is3D ? 0u : MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (ma_sound_init_from_data_source(
            &sample->driver->engine,
            reinterpret_cast<ma_data_source*>(&sample->decoder),
            flags,
            nullptr,
            &sample->sound) != MA_SUCCESS)
    {
        SetLastErrorText("miniaudio: failed to create sound from decoder");
        ResetSound(sample);
        return false;
    }

    sample->soundInitialized = true;
    sample->stopped = true;
    ma_sound_set_spatialization_enabled(&sample->sound, sample->is3D ? MA_TRUE : MA_FALSE);
    return true;
}

MilesCompatSound* AllocateSound(HDIGDRIVER driver, bool is3D)
{
    if (!driver)
        return nullptr;

    auto* sample = new (std::nothrow) MilesCompatSound;
    if (!sample)
        return nullptr;

    sample->driver = driver;
    sample->is3D = is3D;
    return sample;
}

void SetLoopMode(MilesCompatSound* sample, S32 count)
{
    if (!sample || !sample->soundInitialized)
        return;

    // Metin2 uses 1 for one-shot and 0 for infinite music/ambience.
    // Counts above 1 are treated as looping during the first migration pass.
    ma_sound_set_looping(&sample->sound, (count == 0 || count > 1) ? MA_TRUE : MA_FALSE);
}

void StartSound(MilesCompatSound* sample)
{
    if (!sample || !sample->soundInitialized)
        return;

    if (sample->stopped || ma_sound_at_end(&sample->sound))
        ma_sound_seek_to_pcm_frame(&sample->sound, 0);

    if (ma_sound_start(&sample->sound) == MA_SUCCESS)
    {
        sample->paused = false;
        sample->stopped = false;
    }
}

void PauseSound(MilesCompatSound* sample)
{
    if (!sample || !sample->soundInitialized)
        return;

    ma_sound_stop(&sample->sound);
    sample->paused = true;
    sample->stopped = false;
}

void EndSound(MilesCompatSound* sample)
{
    if (!sample || !sample->soundInitialized)
        return;

    ma_sound_stop(&sample->sound);
    ma_sound_seek_to_pcm_frame(&sample->sound, 0);
    sample->paused = false;
    sample->stopped = true;
}

S32 SoundStatus(const MilesCompatSound* sample)
{
    if (!sample || !sample->soundInitialized || sample->stopped)
        return SMP_DONE;

    return ma_sound_at_end(&sample->sound) ? SMP_DONE : 0;
}
}

void AIL_startup() {}
void AIL_shutdown() {}

void AIL_set_redist_directory(const char*) {}

HDIGDRIVER AIL_open_digital_driver(S32 frequency, S32, S32, U32)
{
    if (g_driver)
        return g_driver;

    auto* driver = new (std::nothrow) MilesCompatEngine;
    if (!driver)
        return nullptr;

    ma_engine_config config = ma_engine_config_init();
    if (frequency > 0)
        config.sampleRate = static_cast<ma_uint32>(frequency);

    if (ma_engine_init(&config, &driver->engine) != MA_SUCCESS)
    {
        SetLastErrorText("miniaudio: failed to initialize engine");
        delete driver;
        return nullptr;
    }

    driver->initialized = true;
    g_driver = driver;

    g_provider.driver = driver;
    strcpy_s(g_provider.name, "Miles Fast 2D Positional Audio");
    return driver;
}

void AIL_close_digital_driver(HDIGDRIVER driver)
{
    if (!driver)
        return;

    if (driver->initialized)
        ma_engine_uninit(&driver->engine);

    if (g_driver == driver)
    {
        g_driver = nullptr;
        g_provider.driver = nullptr;
    }

    delete driver;
}

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER driver)
{
    return AllocateSound(driver, false);
}

void AIL_release_sample_handle(HSAMPLE sample)
{
    ResetSound(sample);
    delete sample;
}

void AIL_init_sample(HSAMPLE sample)
{
    ResetSound(sample);
}

S32 AIL_set_sample_file(HSAMPLE sample, const void* data, U32 size)
{
    return InitSoundFromMemory(sample, data, size) ? 1 : 0;
}

S32 AIL_sample_status(HSAMPLE sample) { return SoundStatus(sample); }
void AIL_set_sample_loop_count(HSAMPLE sample, S32 count) { SetLoopMode(sample, count); }
void AIL_start_sample(HSAMPLE sample) { StartSound(sample); }
void AIL_stop_sample(HSAMPLE sample) { PauseSound(sample); }
void AIL_resume_sample(HSAMPLE sample) { StartSound(sample); }
void AIL_end_sample(HSAMPLE sample) { EndSound(sample); }

void AIL_sample_volume_pan(HSAMPLE sample, float* volume, float* pan)
{
    if (!sample || !sample->soundInitialized)
        return;

    if (volume)
        *volume = ma_sound_get_volume(&sample->sound);
    if (pan)
        *pan = (ma_sound_get_pan(&sample->sound) + 1.0f) * 0.5f;
}

void AIL_set_sample_volume_pan(HSAMPLE sample, float volume, float pan)
{
    if (!sample || !sample->soundInitialized)
        return;

    ma_sound_set_volume(&sample->sound, std::clamp(volume, 0.0f, 1.0f));
    ma_sound_set_pan(&sample->sound, std::clamp(pan * 2.0f - 1.0f, -1.0f, 1.0f));
}

S32 AIL_enumerate_3D_providers(HPROENUM* enumeration, HPROVIDER* provider, char** name)
{
    if (!enumeration || !provider || !name || !g_driver || *enumeration != HPROENUM_FIRST)
        return 0;

    g_provider.driver = g_driver;
    strcpy_s(g_provider.name, "Miles Fast 2D Positional Audio");

    *provider = &g_provider;
    *name = g_provider.name;
    *enumeration = 1;
    return 1;
}

S32 AIL_open_3D_provider(HPROVIDER provider) { return provider && provider->driver ? M3D_NOERR : -1; }
void AIL_close_3D_provider(HPROVIDER) {}

H3DPOBJECT AIL_open_3D_listener(HPROVIDER provider)
{
    return provider ? provider->driver : nullptr;
}

void AIL_close_3D_listener(H3DPOBJECT) {}

H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER provider)
{
    return provider ? AllocateSound(provider->driver, true) : nullptr;
}

void AIL_release_3D_sample_handle(H3DSAMPLE sample)
{
    ResetSound(sample);
    delete sample;
}

S32 AIL_set_3D_sample_file(H3DSAMPLE sample, const void* data)
{
    if (!data)
        return 0;

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    const U32 size = *reinterpret_cast<const U32*>(bytes - sizeof(U32));
    return InitSoundFromMemory(sample, data, size) ? 1 : 0;
}

S32 AIL_3D_sample_status(H3DSAMPLE sample) { return SoundStatus(sample); }
void AIL_set_3D_sample_loop_count(H3DSAMPLE sample, S32 count) { SetLoopMode(sample, count); }
void AIL_start_3D_sample(H3DSAMPLE sample) { StartSound(sample); }
void AIL_stop_3D_sample(H3DSAMPLE sample) { PauseSound(sample); }
void AIL_resume_3D_sample(H3DSAMPLE sample) { StartSound(sample); }
void AIL_end_3D_sample(H3DSAMPLE sample) { EndSound(sample); }

float AIL_3D_sample_volume(H3DSAMPLE sample)
{
    return sample && sample->soundInitialized ? ma_sound_get_volume(&sample->sound) : 0.0f;
}

void AIL_set_3D_sample_volume(H3DSAMPLE sample, float volume)
{
    if (sample && sample->soundInitialized)
        ma_sound_set_volume(&sample->sound, std::clamp(volume, 0.0f, 1.0f));
}

void AIL_set_3D_position(H3DSAMPLE sample, float x, float y, float z)
{
    if (sample && sample->soundInitialized)
        ma_sound_set_position(&sample->sound, x, y, z);
}

void AIL_set_3D_velocity(H3DSAMPLE sample, float x, float y, float z, float magnitude)
{
    if (sample && sample->soundInitialized)
        ma_sound_set_velocity(&sample->sound, x * magnitude, y * magnitude, z * magnitude);
}

void AIL_auto_update_3D_position(H3DSAMPLE, S32) {}
void AIL_update_3D_position(H3DSAMPLE, float) {}

void AIL_set_3D_position(H3DPOBJECT listener, float x, float y, float z)
{
    if (listener)
        ma_engine_listener_set_position(&listener->engine, 0, x, y, z);
}

void AIL_set_3D_orientation(H3DPOBJECT listener, float faceX, float faceY, float faceZ, float upX, float upY, float upZ)
{
    if (!listener)
        return;

    ma_engine_listener_set_direction(&listener->engine, 0, faceX, faceY, faceZ);
    ma_engine_listener_set_world_up(&listener->engine, 0, upX, upY, upZ);
}

void AIL_set_3D_velocity(H3DPOBJECT listener, float x, float y, float z, float magnitude)
{
    if (listener)
        ma_engine_listener_set_velocity(&listener->engine, 0, x * magnitude, y * magnitude, z * magnitude);
}

HSTREAM AIL_open_stream(HDIGDRIVER driver, const char* filename, S32)
{
    if (!driver || !filename)
        return nullptr;

    auto* stream = AllocateSound(driver, false);
    if (!stream)
        return nullptr;

    if (ma_sound_init_from_file(
            &driver->engine,
            filename,
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
            nullptr,
            nullptr,
            &stream->sound) != MA_SUCCESS)
    {
        SetLastErrorText("miniaudio: failed to open music stream");
        delete stream;
        return nullptr;
    }

    stream->soundInitialized = true;
    stream->stopped = true;
    return stream;
}

void AIL_close_stream(HSTREAM stream)
{
    ResetSound(stream);
    delete stream;
}

S32 AIL_stream_status(HSTREAM stream)
{
    return SoundStatus(stream) == SMP_DONE ? -1 : 0;
}

void AIL_set_stream_loop_count(HSTREAM stream, S32 count) { SetLoopMode(stream, count); }
void AIL_start_stream(HSTREAM stream) { StartSound(stream); }

void AIL_pause_stream(HSTREAM stream, S32 pause)
{
    if (pause)
        PauseSound(stream);
    else
        StartSound(stream);
}

void AIL_stream_volume_levels(HSTREAM stream, float* left, float* right)
{
    const float volume = stream && stream->soundInitialized ? ma_sound_get_volume(&stream->sound) : 0.0f;
    if (left) *left = volume;
    if (right) *right = volume;
}

void AIL_set_stream_volume_levels(HSTREAM stream, float left, float right)
{
    if (stream && stream->soundInitialized)
        ma_sound_set_volume(&stream->sound, std::clamp((left + right) * 0.5f, 0.0f, 1.0f));
}

const char* AIL_last_error()
{
    return g_lastError[0] ? g_lastError : "miniaudio backend";
}
