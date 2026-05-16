#include "Smoother.h"

Smoother::Smoother(int size)
  : bufferSize(size > 0 ? size : 1),
    bufferIndex(0)
{
  buffer = new int[bufferSize];
  fillBufferOnStart(0);
}

Smoother::~Smoother() {
  delete[] buffer;
}

void Smoother::fillBufferOnStart(int initialValue) {
  for (int i = 0; i < bufferSize; ++i) {
    buffer[i] = initialValue;
  }
  bufferIndex = 0;
}

void Smoother::reset() {
  fillBufferOnStart(0);
}

void Smoother::setBufferSize(int size) {
  if (size <= 0 || size == bufferSize) {
    return;
  }

  delete[] buffer;
  bufferSize = size;
  buffer = new int[bufferSize];
  fillBufferOnStart(0);
}

int Smoother::smooth(int newValue) {
  buffer[bufferIndex] = newValue;
  bufferIndex = (bufferIndex + 1) % bufferSize;

  long sum = 0;
  for (int i = 0; i < bufferSize; ++i) {
    sum += buffer[i];
  }

  int average = sum / bufferSize;
  return average;
}
