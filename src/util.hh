#pragma once

#include <time.h>
#include <stdint.h>

uint32_t micros();
uint32_t microDiff(uint32_t start, uint32_t end);

class Random {
  uint64_t seed;
public:
  Random(uint64_t seed=0);

  uint64_t operator()();

  uint64_t operator()(int n);

  float fraction();
};

class Stopwatch {
  timespec start;
public:
  Stopwatch();
  void reset();
  void resetWithDelta(float deltaSeconds);
  float elapsedSeconds(bool reset = false);
};

