#pragma once

class Smoother {
public:
  static constexpr int MAX_BUFFER_SIZE = 120;

  Smoother(int bufferSize);
  ~Smoother();

  int smooth(int newValue);
  void fillBufferOnStart(int initialValue);
  void reset();

  void setBufferSize(int size);
  bool isReady() const;
  int size() const;

private:
  int* buffer;
  int bufferSize;
  int bufferIndex;

  static int clampBufferSize(int size);
};
