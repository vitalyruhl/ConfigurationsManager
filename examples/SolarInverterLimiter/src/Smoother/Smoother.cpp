#include "Smoother.h"

#include <new>

Smoother::Smoother(int size)
  : bufferSize(clampBufferSize(size)),
    bufferIndex(0)
{
  buffer = new (std::nothrow) int[bufferSize];
  fillBufferOnStart(0);
}

Smoother::~Smoother() {
  delete[] buffer;
}

void Smoother::fillBufferOnStart(int initialValue) {
  if (buffer == nullptr || bufferSize <= 0) {
    return;
  }

  for (int i = 0; i < bufferSize; ++i) {
    buffer[i] = initialValue;
  }
  bufferIndex = 0;
}

void Smoother::reset() {
  fillBufferOnStart(0);
}

void Smoother::setBufferSize(int size) {
  const int nextSize = clampBufferSize(size);
  if (nextSize == bufferSize) {
    return;
  }

  int* nextBuffer = new (std::nothrow) int[nextSize];
  if (nextBuffer == nullptr) {
    return;
  }

  delete[] buffer;
  bufferSize = nextSize;
  buffer = nextBuffer;
  fillBufferOnStart(0);
}

int Smoother::smooth(int newValue) {
  if (buffer == nullptr || bufferSize <= 0) {
    return newValue;
  }

  buffer[bufferIndex] = newValue;
  bufferIndex = (bufferIndex + 1) % bufferSize;

  long sum = 0;
  for (int i = 0; i < bufferSize; ++i) {
    sum += buffer[i];
  }

  int average = sum / bufferSize;
  return average;
}

bool Smoother::isReady() const {
  return buffer != nullptr && bufferSize > 0;
}

int Smoother::size() const {
  return bufferSize;
}

int Smoother::clampBufferSize(int size) {
  if (size < 1) {
    return 1;
  }
  if (size > MAX_BUFFER_SIZE) {
    return MAX_BUFFER_SIZE;
  }
  return size;
}
