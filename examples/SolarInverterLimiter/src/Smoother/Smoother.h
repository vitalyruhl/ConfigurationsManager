#pragma once

class Smoother {
public:
  Smoother(int bufferSize);
  ~Smoother();

  int smooth(int newValue);
  void fillBufferOnStart(int initialValue);
  void reset();

  void setBufferSize(int size);

private:
  int* buffer;
  int bufferSize;
  int bufferIndex;
};
