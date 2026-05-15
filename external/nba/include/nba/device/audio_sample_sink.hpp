#pragma once

#include <nba/integer.hpp>

namespace nba {

struct AudioSampleSink {
  virtual ~AudioSampleSink() = default;
  virtual bool IsActive() const { return false; }
  virtual void OnBufferReady(const s8* left, const s8* right, int count) = 0;
};

struct NullAudioSampleSink final : AudioSampleSink {
  void OnBufferReady(const s8*, const s8*, int) override {}
};

} // namespace nba
