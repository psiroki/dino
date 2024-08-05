#include <string.h>

#include "util.hh"

uint32_t micros() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec % 1000 * 1000000 + t.tv_nsec / 1000;
}

uint32_t microDiff(uint32_t start, uint32_t end) {
  return end < start ? 1000000000 - end + start : end - start;
}

uint64_t nextSeed(uint64_t seed) {
  return (seed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
}

float seedToFloat(uint64_t seed) {
  return (seed & 0xffffff) / static_cast<float>(0xffffff);
}

Random::Random(uint64_t seed): seed(nextSeed(seed)) {}

uint64_t Random::operator()() {
  return seed = nextSeed(seed);
}

uint64_t Random::operator()(int n) {
  return (seed = nextSeed(seed)) % n;
}

float Random::fraction() {
  return seedToFloat(seed = nextSeed(seed));
}

namespace {
  const float nanoToSecond = 1.0e-9f;
}

Stopwatch::Stopwatch() {
  clock_gettime(CLOCK_MONOTONIC, &start);
}

void Stopwatch::reset() {
  clock_gettime(CLOCK_MONOTONIC, &start);
}

void Stopwatch::resetWithDelta(float deltaSeconds) {
  reset();
  long secs = static_cast<long>(deltaSeconds);
  long rest = static_cast<long>((deltaSeconds - secs) * 1000000000L);
  start.tv_sec += secs;
  // we want to subtract more than what is in the tv_nsec field
  if (start.tv_nsec < -rest) {
    --start.tv_sec;
    rest += 1000000000L;
  }
  // the amount required for a full second is
  // less than what we're about to add to it
  if (1000000000L - start.tv_nsec < rest) {
    rest -= 1000000000L - start.tv_nsec;
    ++start.tv_sec;
  }
  start.tv_nsec += rest;
}

float Stopwatch::elapsedSeconds(bool reset) {
  timespec then(start), now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (reset) start = now;
  int32_t secDiff = now.tv_sec - then.tv_sec;
  if (now.tv_nsec < then.tv_nsec) {
    // like 1.8 to 2.4
    --secDiff;  // secDiff becomes 0
    return secDiff + nanoToSecond * (1000000000L - then.tv_nsec + now.tv_nsec);
  } else {
    return secDiff + nanoToSecond * (now.tv_nsec - then.tv_nsec);
  }
}
