#pragma once

class Smoother {
public:
  Smoother(int bufferSize, int correctionOffset = 0, int min = -99999, int max = 99999);
  ~Smoother();

  int smooth(int newValue);
  void fillBufferOnStart(int initialValue);
  void reset();

  // Setters to update config at runtime
  void setCorrectionOffset(int offset);
  void setLimits(int min, int max);

private:
  int* buffer;
  int bufferSize;
  int bufferIndex;

  int correctionOffset;
  int minLimit;
  int maxLimit;

  int applyLimiter(int value);
};
