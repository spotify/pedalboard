/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include "juce_PatchedFLACAudioFormat.h"

#if defined _WIN32 && !defined __CYGWIN__
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined _MSC_VER || defined __BORLANDC__ || defined __MINGW32__
#include <sys/types.h> /* for off_t */
#endif

#if HAVE_INTTYPES_H
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#endif

#if defined _MSC_VER || defined __MINGW32__ || defined __CYGWIN__ ||           \
    defined __EMX__
#include <fcntl.h> /* for _O_BINARY */
#include <io.h>    /* for _setmode(), chmod() */
#else
#include <unistd.h> /* for chown(), unlink() */
#endif

#if defined _MSC_VER || defined __BORLANDC__ || defined __MINGW32__
#if defined __BORLANDC__
#include <utime.h> /* for utime() */
#else
#include <sys/utime.h> /* for utime() */
#endif
#else
#include <sys/types.h> /* some flavors of BSD (like OS X) require this to get time_t */
#include <utime.h> /* for utime() */
#endif

#if defined _MSC_VER
#if _MSC_VER >= 1600
#include <stdint.h>
#else
#include <limits.h>
#endif
#endif

#ifdef _WIN32
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <windows.h>
#endif

#ifdef DEBUG
#include <assert.h>
#endif

#include <stdio.h>
#include <stdlib.h>

namespace juce {

namespace PatchedFlacNamespace {
#if JUCE_INCLUDE_FLAC_CODE || !defined(JUCE_INCLUDE_FLAC_CODE)

#undef VERSION
#define VERSION "1.3.1"

#define FLAC__NO_DLL 1

JUCE_BEGIN_IGNORE_WARNINGS_MSVC(
    4267 4127 4244 4996 4100 4701 4702 4013 4133 4206 4312 4505 4365 4005 4334 181 111 6340 6308 6297 6001 6320)
#if !JUCE_MSVC
#define HAVE_LROUND 1
#endif

#if JUCE_MAC
#define FLAC__SYS_DARWIN 1
#endif

#ifndef SIZE_MAX
#define SIZE_MAX 0xffffffff
#endif

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wconversion", "-Wshadow",
                                    "-Wdeprecated-register", "-Wswitch-enum",
                                    "-Wswitch-default",
                                    "-Wimplicit-fallthrough",
                                    "-Wzero-as-null-pointer-constant",
                                    "-Wsign-conversion", "-Wredundant-decls",
                                    "-Wlanguage-extension-token")

#if JUCE_INTEL
#if JUCE_32BIT
#define FLAC__CPU_IA32 1
#endif
#if JUCE_64BIT
#define FLAC__CPU_X86_64 1
#endif
#define FLAC__HAS_X86INTRIN 1
#endif

#undef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#define flac_max jmax
#define flac_min jmin
#undef DEBUG // (some flac code dumps debug trace if the app defines this macro)
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/all.h"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/bitmath.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/bitreader.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/bitwriter.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/cpu.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/crc.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/fixed.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/float.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/format.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/lpc_flac.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/md5.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/memory.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/stream_decoder.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/stream_encoder.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/stream_encoder_framing.c"
#include "../../JUCE/modules/juce_audio_formats/codecs/flac/libFLAC/window_flac.c"
#include "../../vendors/libFLAC/metadata_object.c"
#undef VERSION
#else
#include <FLAC/all.h>
#endif

JUCE_END_IGNORE_WARNINGS_GCC_LIKE
JUCE_END_IGNORE_WARNINGS_MSVC
} // namespace PatchedFlacNamespace

#undef max
#undef min

//==============================================================================
static const char *const flacFormatName = "FLAC file";

template <typename Item> auto emptyRange(Item item) {
  return Range<Item>::emptyRange(item);
}

//==============================================================================
class PatchedFlacReader : public AudioFormatReader {
public:
  PatchedFlacReader(InputStream *in) : AudioFormatReader(in, flacFormatName) {
    lengthInSamples = 0;
    decoder = PatchedFlacNamespace::FLAC__stream_decoder_new();

    ok = FLAC__stream_decoder_init_stream(
             decoder, readCallback_, seekCallback_, tellCallback_,
             lengthCallback_, eofCallback_, writeCallback_, metadataCallback_,
             errorCallback_,
             this) == PatchedFlacNamespace::FLAC__STREAM_DECODER_INIT_STATUS_OK;

    if (ok) {
      FLAC__stream_decoder_process_until_end_of_metadata(decoder);

      if (lengthInSamples == 0 && sampleRate > 0) {
        // the length hasn't been stored in the metadata, so we'll need to
        // work it out the length the hard way, by scanning the whole file..
        scanningForLength = true;
        FLAC__stream_decoder_process_until_end_of_stream(decoder);
        scanningForLength = false;
        auto tempLength = lengthInSamples;

        FLAC__stream_decoder_reset(decoder);
        FLAC__stream_decoder_process_until_end_of_metadata(decoder);
        lengthInSamples = tempLength;
      }
    }
  }

  ~PatchedFlacReader() override {
    PatchedFlacNamespace::FLAC__stream_decoder_delete(decoder);
  }

  void useMetadata(
      const PatchedFlacNamespace::FLAC__StreamMetadata_StreamInfo &info) {
    sampleRate = info.sample_rate;
    bitsPerSample = info.bits_per_sample;
    lengthInSamples = (unsigned int)info.total_samples;
    numChannels = info.channels;

    reservoir.setSize((int)numChannels, 2 * (int)info.max_blocksize, false,
                      false, true);
  }

  bool readSamples(int **destSamples, int numDestChannels,
                   int startOffsetInDestBuffer, int64 startSampleInFile,
                   int numSamples) override {
    if (!ok)
      return false;

    const auto getBufferedRange = [this] { return bufferedRange; };

    const auto readFromReservoir =
        [this, &destSamples, &numDestChannels, &startOffsetInDestBuffer,
         &startSampleInFile](const Range<int64> rangeToRead) {
          const auto bufferIndices = rangeToRead - bufferedRange.getStart();
          const auto writePos = (int64)startOffsetInDestBuffer +
                                (rangeToRead.getStart() - startSampleInFile);

          for (int i = jmin(numDestChannels, reservoir.getNumChannels());
               --i >= 0;) {
            if (destSamples[i] != nullptr) {
              memcpy(destSamples[i] + writePos,
                     reservoir.getReadPointer(i) + bufferIndices.getStart(),
                     (size_t)bufferIndices.getLength() * sizeof(int));
            }
          }
        };

    const auto fillReservoir = [this](const int64 requestedStart) {
      if (requestedStart >= lengthInSamples) {
        bufferedRange = emptyRange(requestedStart);
        return;
      }

      if (requestedStart < bufferedRange.getStart() ||
          requestedStart > bufferedRange.getEnd()) {
        bufferedRange = emptyRange(requestedStart);
        FLAC__stream_decoder_seek_absolute(
            decoder,
            (PatchedFlacNamespace::FLAC__uint64)bufferedRange.getStart());
        return;
      }

      bufferedRange = emptyRange(bufferedRange.getEnd());
      FLAC__stream_decoder_process_single(decoder);
    };

    const auto remainingSamples = Reservoir::doBufferedRead(
        Range<int64>{startSampleInFile, startSampleInFile + numSamples},
        getBufferedRange, readFromReservoir, fillReservoir);

    if (!remainingSamples.isEmpty())
      for (int i = numDestChannels; --i >= 0;)
        if (destSamples[i] != nullptr)
          zeromem(destSamples[i] + startOffsetInDestBuffer,
                  (size_t)remainingSamples.getLength() * sizeof(int));

    return true;
  }

  void useSamples(const PatchedFlacNamespace::FLAC__int32 *const buffer[],
                  int numSamples) {
    if (scanningForLength) {
      lengthInSamples += numSamples;
    } else {
      if (numSamples > reservoir.getNumSamples())
        reservoir.setSize((int)numChannels, numSamples, false, false, true);

      auto bitsToShift = 32 - bitsPerSample;

      for (int i = 0; i < (int)numChannels; ++i) {
        auto *src = buffer[i];
        int n = i;

        while (src == nullptr && n > 0)
          src = buffer[--n];

        if (src != nullptr) {
          auto *dest = reinterpret_cast<int *>(reservoir.getWritePointer(i));

          for (int j = 0; j < numSamples; ++j)
            dest[j] = src[j] << bitsToShift;
        }
      }

      bufferedRange.setLength(numSamples);
    }
  }

  //==============================================================================
  static PatchedFlacNamespace::FLAC__StreamDecoderReadStatus
  readCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
                PatchedFlacNamespace::FLAC__byte buffer[], size_t *bytes,
                void *client_data) {
    *bytes = (size_t) static_cast<const PatchedFlacReader *>(client_data)
                 ->input->read(buffer, (int)*bytes);
    return PatchedFlacNamespace::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  }

  static PatchedFlacNamespace::FLAC__StreamDecoderSeekStatus
  seekCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
                PatchedFlacNamespace::FLAC__uint64 absolute_byte_offset,
                void *client_data) {
    static_cast<const PatchedFlacReader *>(client_data)
        ->input->setPosition((int)absolute_byte_offset);
    return PatchedFlacNamespace::FLAC__STREAM_DECODER_SEEK_STATUS_OK;
  }

  static PatchedFlacNamespace::FLAC__StreamDecoderTellStatus
  tellCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
                PatchedFlacNamespace::FLAC__uint64 *absolute_byte_offset,
                void *client_data) {
    *absolute_byte_offset =
        (uint64) static_cast<const PatchedFlacReader *>(client_data)
            ->input->getPosition();
    return PatchedFlacNamespace::FLAC__STREAM_DECODER_TELL_STATUS_OK;
  }

  static PatchedFlacNamespace::FLAC__StreamDecoderLengthStatus
  lengthCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
                  PatchedFlacNamespace::FLAC__uint64 *stream_length,
                  void *client_data) {
    *stream_length =
        (uint64) static_cast<const PatchedFlacReader *>(client_data)
            ->input->getTotalLength();
    return PatchedFlacNamespace::FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
  }

  static PatchedFlacNamespace::FLAC__bool
  eofCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
               void *client_data) {
    return static_cast<const PatchedFlacReader *>(client_data)
        ->input->isExhausted();
  }

  static PatchedFlacNamespace::FLAC__StreamDecoderWriteStatus
  writeCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
                 const PatchedFlacNamespace::FLAC__Frame *frame,
                 const PatchedFlacNamespace::FLAC__int32 *const buffer[],
                 void *client_data) {
    static_cast<PatchedFlacReader *>(client_data)
        ->useSamples(buffer, (int)frame->header.blocksize);
    return PatchedFlacNamespace::FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  }

  static void
  metadataCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
                    const PatchedFlacNamespace::FLAC__StreamMetadata *metadata,
                    void *client_data) {
    static_cast<PatchedFlacReader *>(client_data)
        ->useMetadata(metadata->data.stream_info);
  }

  static void
  errorCallback_(const PatchedFlacNamespace::FLAC__StreamDecoder *,
                 PatchedFlacNamespace::FLAC__StreamDecoderErrorStatus, void *) {
  }

private:
  PatchedFlacNamespace::FLAC__StreamDecoder *decoder;
  AudioBuffer<float> reservoir;
  Range<int64> bufferedRange;
  bool ok = false, scanningForLength = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchedFlacReader)
};

//==============================================================================
class PatchedFlacWriter : public AudioFormatWriter {
public:
  PatchedFlacWriter(OutputStream *out, double rate, uint32 numChans,
                    uint32 bits, int qualityOptionIndex)
      : AudioFormatWriter(out, flacFormatName, rate, numChans, bits),
        streamStartPos(output != nullptr ? jmax(output->getPosition(), 0ll)
                                         : 0ll) {
    encoder = PatchedFlacNamespace::FLAC__stream_encoder_new();

    if (qualityOptionIndex > 0)
      FLAC__stream_encoder_set_compression_level(
          encoder, (uint32)jmin(8, qualityOptionIndex));

    FLAC__stream_encoder_set_do_mid_side_stereo(encoder, numChannels == 2);
    FLAC__stream_encoder_set_loose_mid_side_stereo(encoder, numChannels == 2);
    FLAC__stream_encoder_set_channels(encoder, numChannels);
    FLAC__stream_encoder_set_bits_per_sample(
        encoder, jmin((unsigned int)24, bitsPerSample));
    FLAC__stream_encoder_set_sample_rate(encoder, (unsigned int)sampleRate);
    FLAC__stream_encoder_set_blocksize(encoder, 0);
    FLAC__stream_encoder_set_do_escape_coding(encoder, true);

    // Create a seek table, which is empty by default:
    seektable = PatchedFlacNamespace::FLAC__metadata_object_new(
        PatchedFlacNamespace::FLAC__METADATA_TYPE_SEEKTABLE);
    if (!seektable)
      return;

    // Write a single placeholder to the seek table.
    if (!PatchedFlacNamespace::
            FLAC__metadata_object_seektable_template_append_placeholders(
                seektable, /* number of placeholder elements */ 1))
      return;

    if (!PatchedFlacNamespace::FLAC__metadata_object_seektable_template_sort(
            seektable, /*compact=*/true))
      return;

    if (!PatchedFlacNamespace::FLAC__stream_encoder_set_metadata(
            encoder, &seektable, 1)) {
      return;
    }

    ok = FLAC__stream_encoder_init_stream(encoder, encodeWriteCallback,
                                          encodeSeekCallback,
                                          encodeTellCallback, nullptr, this) ==
         PatchedFlacNamespace::FLAC__STREAM_ENCODER_INIT_STATUS_OK;
  }

  ~PatchedFlacWriter() override {
    if (ok) {
      PatchedFlacNamespace::FLAC__stream_encoder_finish(encoder);
      output->flush();
    } else {
      output = nullptr; // to stop the base class deleting this, as it needs to
                        // be returned to the caller of createWriter()
    }

    PatchedFlacNamespace::FLAC__stream_encoder_delete(encoder);
    if (seektable) {
      PatchedFlacNamespace::FLAC__metadata_object_delete(seektable);
      seektable = nullptr;
    }
  }

  //==============================================================================
  bool write(const int **samplesToWrite, int numSamples) override {
    if (!ok)
      return false;

    HeapBlock<int *> channels;
    HeapBlock<int> temp;
    auto bitsToShift = 32 - (int)bitsPerSample;

    if (bitsToShift > 0) {
      temp.malloc(numChannels * (size_t)numSamples);
      channels.calloc(numChannels + 1);

      for (unsigned int i = 0; i < numChannels; ++i) {
        if (samplesToWrite[i] == nullptr)
          break;

        auto *destData = temp.get() + i * (size_t)numSamples;
        channels[i] = destData;

        for (int j = 0; j < numSamples; ++j)
          destData[j] = (samplesToWrite[i][j] >> bitsToShift);
      }

      samplesToWrite = const_cast<const int **>(channels.get());
    }

    return FLAC__stream_encoder_process(
               encoder,
               (const PatchedFlacNamespace::FLAC__int32 **)samplesToWrite,
               (unsigned)numSamples) != 0;
  }

  bool writeData(const void *const data, const int size) const {
    return output->write(data, (size_t)size);
  }

  static void packUint32(PatchedFlacNamespace::FLAC__uint32 val,
                         PatchedFlacNamespace::FLAC__byte *b, const int bytes) {
    b += bytes;

    for (int i = 0; i < bytes; ++i) {
      *(--b) = (PatchedFlacNamespace::FLAC__byte)(val & 0xff);
      val >>= 8;
    }
  }

  //==============================================================================
  static PatchedFlacNamespace::FLAC__StreamEncoderWriteStatus
  encodeWriteCallback(const PatchedFlacNamespace::FLAC__StreamEncoder *,
                      const PatchedFlacNamespace::FLAC__byte buffer[],
                      size_t bytes, unsigned int /*samples*/,
                      unsigned int /*current_frame*/, void *client_data) {
    return static_cast<PatchedFlacWriter *>(client_data)
                   ->writeData(buffer, (int)bytes)
               ? PatchedFlacNamespace::FLAC__STREAM_ENCODER_WRITE_STATUS_OK
               : PatchedFlacNamespace::
                     FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
  }

  static PatchedFlacNamespace::FLAC__StreamEncoderSeekStatus
  encodeSeekCallback(const PatchedFlacNamespace::FLAC__StreamEncoder *,
                     PatchedFlacNamespace::FLAC__uint64 position,
                     void *client_data) {
    if (client_data == nullptr)
      return PatchedFlacNamespace::FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;
    auto *writer = static_cast<PatchedFlacWriter *>(client_data);
    return writer->output->setPosition(writer->streamStartPos + position)
               ? PatchedFlacNamespace::FLAC__STREAM_ENCODER_SEEK_STATUS_OK
               : PatchedFlacNamespace::FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
  }

  static PatchedFlacNamespace::FLAC__StreamEncoderTellStatus
  encodeTellCallback(const PatchedFlacNamespace::FLAC__StreamEncoder *,
                     PatchedFlacNamespace::FLAC__uint64 *absolute_byte_offset,
                     void *client_data) {
    if (client_data == nullptr)
      return PatchedFlacNamespace::FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED;

    auto *writer = static_cast<PatchedFlacWriter *>(client_data);
    *absolute_byte_offset =
        (PatchedFlacNamespace::FLAC__uint64)writer->output->getPosition() -
        writer->streamStartPos;
    return PatchedFlacNamespace::FLAC__STREAM_ENCODER_TELL_STATUS_OK;
  }

  bool ok = false;

private:
  PatchedFlacNamespace::FLAC__StreamEncoder *encoder;
  PatchedFlacNamespace::FLAC__StreamMetadata *seektable;
  int64 streamStartPos;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchedFlacWriter)
};

//==============================================================================
PatchedFlacAudioFormat::PatchedFlacAudioFormat()
    : AudioFormat(flacFormatName, ".flac") {}
PatchedFlacAudioFormat::~PatchedFlacAudioFormat() {}

Array<int> PatchedFlacAudioFormat::getPossibleSampleRates() {
  return {8000,  11025, 12000, 16000,  22050,  32000,  44100,
          48000, 88200, 96000, 176400, 192000, 352800, 384000};
}

Array<int> PatchedFlacAudioFormat::getPossibleBitDepths() { return {16, 24}; }

bool PatchedFlacAudioFormat::canDoStereo() { return true; }
bool PatchedFlacAudioFormat::canDoMono() { return true; }
bool PatchedFlacAudioFormat::isCompressed() { return true; }

AudioFormatReader *
PatchedFlacAudioFormat::createReaderFor(InputStream *in,
                                        const bool deleteStreamIfOpeningFails) {
  std::unique_ptr<PatchedFlacReader> r(new PatchedFlacReader(in));

  if (r->sampleRate > 0)
    return r.release();

  if (!deleteStreamIfOpeningFails)
    r->input = nullptr;

  return nullptr;
}

AudioFormatWriter *PatchedFlacAudioFormat::createWriterFor(
    OutputStream *out, double sampleRate, unsigned int numberOfChannels,
    int bitsPerSample, const StringPairArray & /*metadataValues*/,
    int qualityOptionIndex) {
  if (out != nullptr && getPossibleBitDepths().contains(bitsPerSample)) {
    std::unique_ptr<PatchedFlacWriter> w(
        new PatchedFlacWriter(out, sampleRate, numberOfChannels,
                              (uint32)bitsPerSample, qualityOptionIndex));
    if (w->ok)
      return w.release();
  }

  return nullptr;
}

StringArray PatchedFlacAudioFormat::getQualityOptions() {
  return {"0 (Fastest)",        "1", "2", "3", "4", "5 (Default)", "6", "7",
          "8 (Highest quality)"};
}

} // namespace juce
