/*
 * pedalboard
 * Copyright 2026 Spotify AB
 *
 * Licensed under the GNU Public License, Version 3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    https://www.gnu.org/licenses/gpl-3.0.html
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../JuceHeader.h"
#include "LameMP3AudioFormat.h"
#include "WriteableAudioFileFlags.h"

/**
 * Parallel MP3 encoding by chunk-encode-and-splice.
 *
 * MP3 is a stream of (mostly) independent frames of 1152 samples each. Given an
 * entire PCM buffer up front, we can split it into contiguous, frame-aligned
 * regions, encode each region with its own LAME instance on a separate thread,
 * and splice the resulting frames back together. For long files this scales
 * close to linearly with core count.
 *
 * Three MP3 details make the splice correct:
 *
 *   1. Bit reservoir: normally a frame's main_data may begin in a previous
 *      frame, so frames are not independent. We disable the reservoir per worker
 *      so every frame is self-contained and safe to splice.
 *
 *   2. Encoder delay + filterbank warmup: each fresh LAME instance emits
 *      `enc_delay` samples of priming and needs a little audio *before* a region
 *      to encode its first frames correctly. We read the true encoder delay from
 *      the LAME tag and feed each worker frame-aligned pre-roll (and a little
 *      post-roll for the trailing MDCT lookahead), then discard the frames that
 *      cover the warmup regions.
 *
 *   3. The Xing/Info header: exactly one belongs at the front of the file. We
 *      drop the workers' individual tag frames and synthesize a single header
 *      with the correct total frame/byte counts for the merged stream.
 *
 * The output is a valid MP3 whose decoded audio is perceptually identical to a
 * single-threaded encode. It is *not* byte-identical, because LAME's
 * psychoacoustic model carries state across the whole stream; reproducing that
 * exactly would require feeding each worker the entire preceding signal, which
 * would defeat the parallelism.
 */

namespace Pedalboard {

// MPEG-1 Layer III frame size in samples. MP3 writing only supports 32/44.1/48
// kHz (all MPEG-1), so this is constant for every file we produce.
static constexpr int MP3_SAMPLES_PER_FRAME = 1152;

struct ParsedMP3Frame {
  size_t offset;  // byte offset of the frame within its buffer
  size_t length;  // frame length in bytes
  int samples;    // decoded samples represented by this frame
  bool isTag;     // true if this is a Xing/Info/LAME metadata frame
};

/**
 * Parse an MPEG Layer III frame header at `pos`. Returns true and populates
 * `frameLength` / `samplesPerFrame` if a valid header is present.
 */
inline bool parseMP3FrameHeader(const uint8_t *data, size_t pos, size_t len,
                                size_t &frameLength, int &samplesPerFrame) {
  if (pos + 4 > len)
    return false;

  const uint8_t b0 = data[pos];
  const uint8_t b1 = data[pos + 1];
  const uint8_t b2 = data[pos + 2];

  // 11-bit frame sync:
  if (b0 != 0xFF || (b1 & 0xE0) != 0xE0)
    return false;

  const int version = (b1 >> 3) & 0b11; // 0=2.5, 1=reserved, 2=2, 3=1
  const int layer = (b1 >> 1) & 0b11;   // 1 == Layer III
  if (version == 0b01 || layer != 0b01)
    return false;

  const bool isMpeg1 = (version == 0b11);
  const int bitrateIndex = (b2 >> 4) & 0x0F;
  const int sampleRateIndex = (b2 >> 2) & 0b11;
  const int padding = (b2 >> 1) & 0b1;

  static const int kBitrateMpeg1[] = {0,   32,  40,  48,  56,  64,  80,  96,
                                      112, 128, 160, 192, 224, 256, 320, 0};
  static const int kBitrateMpeg2[] = {0,  8,  16, 24, 32,  40,  48, 56,
                                      64, 80, 96, 112, 128, 144, 160, 0};
  static const int kSampleRateMpeg1[] = {44100, 48000, 32000, 0};
  static const int kSampleRateMpeg2[] = {22050, 24000, 16000, 0};
  static const int kSampleRateMpeg25[] = {11025, 12000, 8000, 0};

  const int bitrate = (isMpeg1 ? kBitrateMpeg1 : kBitrateMpeg2)[bitrateIndex];
  const int sampleRate = (version == 0b11   ? kSampleRateMpeg1
                          : version == 0b10 ? kSampleRateMpeg2
                                            : kSampleRateMpeg25)[sampleRateIndex];
  if (bitrate == 0 || sampleRate == 0)
    return false;

  samplesPerFrame = isMpeg1 ? 1152 : 576;
  const int coefficient = samplesPerFrame / 8; // 144 (MPEG-1) or 72 (MPEG-2)
  frameLength =
      static_cast<size_t>((coefficient * bitrate * 1000) / sampleRate) + padding;
  return frameLength > 4;
}

/** Return true if the frame at [offset, offset+length) carries a Xing/Info tag. */
inline bool mp3FrameIsTag(const uint8_t *data, size_t offset, size_t length) {
  const size_t end = offset + length;
  for (size_t i = offset; i + 4 <= end; i++) {
    if (std::memcmp(data + i, "Xing", 4) == 0 ||
        std::memcmp(data + i, "Info", 4) == 0)
      return true;
  }
  return false;
}

/** Walk an MP3 byte buffer and return every frame it contains. */
inline std::vector<ParsedMP3Frame> parseMP3Frames(const uint8_t *data,
                                                  size_t len) {
  std::vector<ParsedMP3Frame> frames;
  size_t pos = 0;
  while (pos < len) {
    size_t length = 0;
    int samples = 0;
    if (!parseMP3FrameHeader(data, pos, len, length, samples)) {
      // Skip ID3 tags or resync on unexpected bytes by scanning for the next
      // sync byte.
      size_t next = pos + 1;
      while (next < len && data[next] != 0xFF)
        next++;
      if (next >= len)
        break;
      pos = next;
      continue;
    }
    if (pos + length > len)
      break;
    frames.push_back(
        {pos, length, samples, mp3FrameIsTag(data, pos, length)});
    pos += length;
  }
  return frames;
}

/**
 * Read the encoder delay (in samples) from a LAME/Xing tag, if present.
 * Returns the LAME default of 576 if the tag can't be found or parsed.
 */
inline int readMP3EncoderDelay(const uint8_t *data, size_t len) {
  static constexpr int kLameDefaultEncoderDelay = 576;

  // Find the Xing/Info magic string.
  size_t idx = 0;
  bool found = false;
  for (; idx + 4 <= len; idx++) {
    if (std::memcmp(data + idx, "Xing", 4) == 0 ||
        std::memcmp(data + idx, "Info", 4) == 0) {
      found = true;
      break;
    }
  }
  if (!found || idx + 8 > len)
    return kLameDefaultEncoderDelay;

  const uint32_t flags = (static_cast<uint32_t>(data[idx + 4]) << 24) |
                         (static_cast<uint32_t>(data[idx + 5]) << 16) |
                         (static_cast<uint32_t>(data[idx + 6]) << 8) |
                         static_cast<uint32_t>(data[idx + 7]);
  size_t off = idx + 8;
  if (flags & 0x1)
    off += 4; // frame count
  if (flags & 0x2)
    off += 4; // byte count
  if (flags & 0x4)
    off += 100; // TOC
  if (flags & 0x8)
    off += 4; // quality

  // The LAME extension begins with the version string "LAME....".
  if (off + 4 > len || std::memcmp(data + off, "LAME", 4) != 0)
    return kLameDefaultEncoderDelay;

  const size_t delayOffset = off + 21; // 3 bytes: 12-bit delay + 12-bit padding
  if (delayOffset + 3 > len)
    return kLameDefaultEncoderDelay;

  const int encDelay =
      (data[delayOffset] << 4) | (data[delayOffset + 1] >> 4);
  return encDelay;
}

namespace detail {

/** Encode a fully-prepared PCM block to MP3 bytes using a single LAME writer. */
inline juce::MemoryBlock encodeBlockToMP3(const juce::AudioBuffer<float> &block,
                                          double sampleRate, int numChannels,
                                          int qualityOptionIndex) {
  juce::MemoryBlock output;
  // The LAME writer takes ownership of this stream and deletes it in its
  // destructor, so it must be heap-allocated. `output` outlives the writer.
  // We hold it in a unique_ptr until the writer is successfully constructed so
  // that a throwing constructor doesn't leak the stream.
  auto stream = std::make_unique<juce::MemoryOutputStream>(output, false);

  // The bit reservoir must be disabled so each frame is independently
  // splice-able.
  CodecOptionsMap codecOptions;
  codecOptions[WriteableAudioFileFlag::Mp3EnableBitReservoir] = false;

  {
    LameMP3AudioFormat::Writer writer(stream.get(), sampleRate, numChannels,
                                      qualityOptionIndex, codecOptions);
    stream.release(); // the writer now owns the stream
    if (!writer.writeFromFloatArrays(block.getArrayOfReadPointers(),
                                     numChannels, block.getNumSamples())) {
      throw std::runtime_error("Parallel MP3 encoder failed to encode a chunk.");
    }
    // Writer destructor here flushes LAME, writes the VBR tag, and deletes the
    // stream.
  }

  return output;
}

/**
 * Build the PCM block for one region: [coreStart - pre, coreEnd + post),
 * zero-padded wherever the requested range falls outside the source buffer.
 */
inline juce::AudioBuffer<float>
buildRegionBlock(const juce::AudioBuffer<float> &audio, int numChannels,
                 int coreStart, int coreEnd, int pre, int post) {
  const int total = audio.getNumSamples();
  const int start = coreStart - pre;
  const int end = coreEnd + post;
  const int length = end - start;

  juce::AudioBuffer<float> block(numChannels, length);
  block.clear();

  const int validStart = std::max(0, start);
  const int validEnd = std::min(total, end);
  const int validLength = validEnd - validStart;
  if (validLength > 0) {
    const int destOffset = validStart - start;
    for (int c = 0; c < numChannels; c++) {
      block.copyFrom(c, destOffset, audio, c, validStart, validLength);
    }
  }
  return block;
}

inline void writeBigEndian32(uint8_t *dest, uint32_t value) {
  dest[0] = static_cast<uint8_t>(value >> 24);
  dest[1] = static_cast<uint8_t>(value >> 16);
  dest[2] = static_cast<uint8_t>(value >> 8);
  dest[3] = static_cast<uint8_t>(value);
}

/**
 * Produce a single Xing/Info header frame for the merged stream by patching a
 * worker's tag frame with the correct total frame/byte counts. Because we strip
 * all encoder priming when splicing, the gapless encoder-delay/padding fields
 * are zeroed (the stream already starts at true sample zero).
 */
inline std::vector<uint8_t>
buildMergedHeaderFrame(const uint8_t *tagFrame, size_t tagFrameLength,
                       uint32_t totalAudioFrames, uint32_t totalBytes) {
  std::vector<uint8_t> header(tagFrame, tagFrame + tagFrameLength);

  size_t idx = 0;
  bool found = false;
  for (; idx + 4 <= header.size(); idx++) {
    if (std::memcmp(header.data() + idx, "Xing", 4) == 0 ||
        std::memcmp(header.data() + idx, "Info", 4) == 0) {
      found = true;
      break;
    }
  }
  if (!found || idx + 8 > header.size())
    return header;

  const uint32_t flags = (static_cast<uint32_t>(header[idx + 4]) << 24) |
                         (static_cast<uint32_t>(header[idx + 5]) << 16) |
                         (static_cast<uint32_t>(header[idx + 6]) << 8) |
                         static_cast<uint32_t>(header[idx + 7]);
  size_t off = idx + 8;
  if ((flags & 0x1) && off + 4 <= header.size()) {
    writeBigEndian32(header.data() + off, totalAudioFrames);
    off += 4;
  }
  if ((flags & 0x2) && off + 4 <= header.size()) {
    writeBigEndian32(header.data() + off, totalBytes);
    off += 4;
  }
  if (flags & 0x4)
    off += 100;
  if (flags & 0x8)
    off += 4;

  if (off + 4 <= header.size() &&
      std::memcmp(header.data() + off, "LAME", 4) == 0) {
    const size_t delayOffset = off + 21;
    if (delayOffset + 3 <= header.size()) {
      header[delayOffset] = 0;
      header[delayOffset + 1] = 0;
      header[delayOffset + 2] = 0;
    }
  }

  return header;
}

} // namespace detail

/**
 * Encode `audio` to MP3 using `numWorkers` threads and return the full file
 * bytes. `qualityOptionIndex` is the LAME quality index (as returned by
 * `determineQualityOptionIndex`). This function does not touch Python and must
 * be called with the GIL released.
 */
inline std::string encodeMP3InParallel(const juce::AudioBuffer<float> &audio,
                                       double sampleRate, int numChannels,
                                       int qualityOptionIndex, int numWorkers,
                                       int warmupFrames = 1,
                                       int postrollFrames = 1) {
  const int total = audio.getNumSamples();
  constexpr int FRAME = MP3_SAMPLES_PER_FRAME;

  // Probe a short encode to learn this LAME build's encoder delay.
  const int probeLength = std::min(total, FRAME * 4);
  juce::AudioBuffer<float> probeBlock(numChannels, std::max(probeLength, 1));
  probeBlock.clear();
  if (probeLength > 0) {
    for (int c = 0; c < numChannels; c++)
      probeBlock.copyFrom(c, 0, audio, c, 0, probeLength);
  }
  const juce::MemoryBlock probe =
      detail::encodeBlockToMP3(probeBlock, sampleRate, numChannels, qualityOptionIndex);
  const int encoderDelay = readMP3EncoderDelay(
      static_cast<const uint8_t *>(probe.getData()), probe.getSize());

  // Choose pre-roll so (encoderDelay + pre) is a whole number of frames; this
  // makes each region's kept audio begin exactly on a frame boundary in the
  // decoded timeline.
  const int align = ((-encoderDelay) % FRAME + FRAME) % FRAME;
  const int pre = align + warmupFrames * FRAME;
  const int post = postrollFrames * FRAME;
  const int leadDrop = (encoderDelay + pre) / FRAME;

  const int totalFrames = (total + FRAME - 1) / FRAME;
  numWorkers = std::max(1, std::min(numWorkers, std::max(1, totalFrames)));
  const int framesPerChunk = (totalFrames + numWorkers - 1) / numWorkers;

  struct Region {
    int coreStart;
    int coreEnd;
    int keepFrames;
  };
  std::vector<Region> regions;
  for (int w = 0; w < numWorkers; w++) {
    const int f0 = w * framesPerChunk;
    const int f1 = std::min((w + 1) * framesPerChunk, totalFrames);
    if (f0 >= f1)
      break;
    const int coreStart = f0 * FRAME;
    const int coreEnd = std::min(f1 * FRAME, total);
    const int keepFrames = (coreEnd - coreStart + FRAME - 1) / FRAME;
    regions.push_back({coreStart, coreEnd, keepFrames});
  }

  // Encode each region on its own thread.
  std::vector<juce::MemoryBlock> encoded(regions.size());
  std::vector<std::exception_ptr> errors(regions.size());
  std::vector<std::thread> threads;
  threads.reserve(regions.size());
  for (size_t i = 0; i < regions.size(); i++) {
    threads.emplace_back([&, i]() {
      try {
        const Region &r = regions[i];
        juce::AudioBuffer<float> block = detail::buildRegionBlock(
            audio, numChannels, r.coreStart, r.coreEnd, pre, post);
        encoded[i] = detail::encodeBlockToMP3(block, sampleRate, numChannels,
                                              qualityOptionIndex);
      } catch (...) {
        errors[i] = std::current_exception();
      }
    });
  }
  for (auto &t : threads)
    t.join();
  for (auto &e : errors) {
    if (e)
      std::rethrow_exception(e);
  }

  // Trim each region to just the frames covering its core, and remember the
  // first tag frame we see so we can build the merged header.
  std::vector<std::pair<const uint8_t *, size_t>> body; // (ptr, length) segments
  const uint8_t *headerTagFrame = nullptr;
  size_t headerTagFrameLength = 0;
  uint32_t totalAudioFrames = 0;
  size_t bodyBytes = 0;

  // Keep the parsed frames alive alongside the encoded buffers.
  std::vector<std::vector<ParsedMP3Frame>> parsedPerRegion(regions.size());

  for (size_t i = 0; i < regions.size(); i++) {
    const auto *data = static_cast<const uint8_t *>(encoded[i].getData());
    const size_t size = encoded[i].getSize();
    parsedPerRegion[i] = parseMP3Frames(data, size);
    const auto &frames = parsedPerRegion[i];

    std::vector<const ParsedMP3Frame *> audioFrames;
    for (const auto &f : frames) {
      if (f.isTag) {
        if (headerTagFrame == nullptr) {
          headerTagFrame = data + f.offset;
          headerTagFrameLength = f.length;
        }
      } else {
        audioFrames.push_back(&f);
      }
    }

    const int keep = regions[i].keepFrames;
    for (int k = 0; k < keep; k++) {
      const int index = leadDrop + k;
      if (index < 0 || index >= static_cast<int>(audioFrames.size()))
        continue;
      const ParsedMP3Frame *f = audioFrames[index];
      body.emplace_back(data + f->offset, f->length);
      bodyBytes += f->length;
      totalAudioFrames++;
    }
  }

  std::vector<uint8_t> header;
  if (headerTagFrame != nullptr) {
    const uint32_t totalBytes =
        static_cast<uint32_t>(headerTagFrameLength + bodyBytes);
    header = detail::buildMergedHeaderFrame(headerTagFrame, headerTagFrameLength,
                                            totalAudioFrames, totalBytes);
  }

  std::string result;
  result.reserve(header.size() + bodyBytes);
  result.append(reinterpret_cast<const char *>(header.data()), header.size());
  for (const auto &segment : body) {
    result.append(reinterpret_cast<const char *>(segment.first),
                  segment.second);
  }
  return result;
}

} // namespace Pedalboard
