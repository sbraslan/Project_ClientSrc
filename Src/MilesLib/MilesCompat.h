#pragma once

#if !defined(_WIN64)
# error "MilesCompat is only for the native Win64 audio backend."
#endif

#if !__has_include("../../Extern/miniaudio/miniaudio.h")
# error "Missing Extern/miniaudio/miniaudio.h (expected miniaudio v0.11.25)."
#endif

#include <windows.h>
#include <cstdint>
#include "../../Extern/miniaudio/miniaudio.h"

using U32 = std::uint32_t;
using S32 = std::int32_t;

#ifndef AILCALLBACK
# define AILCALLBACK
#endif

constexpr S32 SMP_DONE = 1;
constexpr S32 M3D_NOERR = 0;
constexpr U32 HPROENUM_FIRST = 0;

struct MilesCompatEngine;
struct MilesCompatProvider;
struct MilesCompatSound;

using HDIGDRIVER = MilesCompatEngine*;
using HPROVIDER = MilesCompatProvider*;
using HPROENUM = U32;
using HSAMPLE = MilesCompatSound*;
using H3DSAMPLE = MilesCompatSound*;
using HSTREAM = MilesCompatSound*;
using H3DPOBJECT = MilesCompatEngine*;

void AIL_startup();
void AIL_shutdown();
void AIL_set_redist_directory(const char* directory);

HDIGDRIVER AIL_open_digital_driver(S32 frequency, S32 bits, S32 channels, U32 flags);
void AIL_close_digital_driver(HDIGDRIVER driver);

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER driver);
void AIL_release_sample_handle(HSAMPLE sample);
void AIL_init_sample(HSAMPLE sample);
S32 AIL_set_sample_file(HSAMPLE sample, const void* data, U32 size);
S32 AIL_sample_status(HSAMPLE sample);
void AIL_set_sample_loop_count(HSAMPLE sample, S32 count);
void AIL_start_sample(HSAMPLE sample);
void AIL_stop_sample(HSAMPLE sample);
void AIL_resume_sample(HSAMPLE sample);
void AIL_end_sample(HSAMPLE sample);
void AIL_sample_volume_pan(HSAMPLE sample, float* volume, float* pan);
void AIL_set_sample_volume_pan(HSAMPLE sample, float volume, float pan);

S32 AIL_enumerate_3D_providers(HPROENUM* enumeration, HPROVIDER* provider, char** name);
S32 AIL_open_3D_provider(HPROVIDER provider);
void AIL_close_3D_provider(HPROVIDER provider);
H3DPOBJECT AIL_open_3D_listener(HPROVIDER provider);
void AIL_close_3D_listener(H3DPOBJECT listener);

H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER provider);
void AIL_release_3D_sample_handle(H3DSAMPLE sample);
S32 AIL_set_3D_sample_file(H3DSAMPLE sample, const void* data);
S32 AIL_3D_sample_status(H3DSAMPLE sample);
void AIL_set_3D_sample_loop_count(H3DSAMPLE sample, S32 count);
void AIL_start_3D_sample(H3DSAMPLE sample);
void AIL_stop_3D_sample(H3DSAMPLE sample);
void AIL_resume_3D_sample(H3DSAMPLE sample);
void AIL_end_3D_sample(H3DSAMPLE sample);
float AIL_3D_sample_volume(H3DSAMPLE sample);
void AIL_set_3D_sample_volume(H3DSAMPLE sample, float volume);

void AIL_set_3D_position(H3DSAMPLE sample, float x, float y, float z);
void AIL_set_3D_velocity(H3DSAMPLE sample, float x, float y, float z, float magnitude);
void AIL_auto_update_3D_position(H3DSAMPLE sample, S32 enabled);
void AIL_update_3D_position(H3DSAMPLE sample, float elapsed);

void AIL_set_3D_position(H3DPOBJECT listener, float x, float y, float z);
void AIL_set_3D_orientation(H3DPOBJECT listener, float faceX, float faceY, float faceZ, float upX, float upY, float upZ);
void AIL_set_3D_velocity(H3DPOBJECT listener, float x, float y, float z, float magnitude);

HSTREAM AIL_open_stream(HDIGDRIVER driver, const char* filename, S32 streamMem);
void AIL_close_stream(HSTREAM stream);
S32 AIL_stream_status(HSTREAM stream);
void AIL_set_stream_loop_count(HSTREAM stream, S32 count);
void AIL_start_stream(HSTREAM stream);
void AIL_pause_stream(HSTREAM stream, S32 pause);
void AIL_stream_volume_levels(HSTREAM stream, float* left, float* right);
void AIL_set_stream_volume_levels(HSTREAM stream, float left, float right);

const char* AIL_last_error();
