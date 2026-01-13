/*
 * dr_wav configuration for Pedalboard
 *
 * dr_wav is a public domain / MIT-0 licensed single-header WAV decoder
 * by David Reid (mackron@gmail.com).
 *
 * GitHub: https://github.com/mackron/dr_libs
 *
 * We use dr_wav to decode ADPCM-compressed WAV files, which are not
 * natively supported by JUCE's WavAudioFormat.
 */

#pragma once

// We don't need stdio support - we use JUCE's InputStream
#define DR_WAV_NO_STDIO

// We don't need wchar support
#define DR_WAV_NO_WCHAR
