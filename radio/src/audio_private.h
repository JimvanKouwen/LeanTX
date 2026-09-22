/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

// Audio implementation and driver integration only. Callers use audio.h.
#include "audio.h"
#include "ff.h"
#include <string.h>

/*
  Implements a bit field, number of bits is set by the template,
  each bit can be modified and read by the provided methods.
*/
template <unsigned int NUM_BITS> class BitField
{
  private:
    uint8_t bits[(NUM_BITS + 7) / 8];
  public:
    BitField()
    {
      reset();
    }

    void reset()
    {
      memset(bits, 0, sizeof(bits));
    }

    void setBit(unsigned int bitNo)
    {
      if (bitNo >= NUM_BITS) return;
      bits[bitNo >> 3] = bits[bitNo >> 3] | (1 << (bitNo & 0x07));
    }

    bool getBit(unsigned int bitNo) const
    {
      if (bitNo >= NUM_BITS) return false;
      return bits[bitNo >> 3] & (1 << (bitNo & 0x07));
    }

    unsigned int getSize() const
    {
      return NUM_BITS;
    }
};

#define AUDIO_QUEUE_LENGTH             (16) // must be a power of 2!

#define AUDIO_BUFFER_DURATION          (10)
#define AUDIO_BUFFER_SIZE              (AUDIO_SAMPLE_RATE*AUDIO_BUFFER_DURATION/1000)

#if !defined(AUDIO_SAMPLE_FMT)
  #if defined(SIMU) || defined(AUDIO_SPI)
    #define AUDIO_SAMPLE_FMT AUDIO_SAMPLE_FMT_S16
  #else
    #define AUDIO_SAMPLE_FMT AUDIO_SAMPLE_FMT_U16
  #endif
#endif

#if defined(SIMU)
  #define AUDIO_BUFFER_COUNT           (10) // simulator needs more buffers for smooth audio
#elif defined(AUDIO_SPI)
  #define AUDIO_BUFFER_COUNT           (2)  // smaller than Taranis since there is also a buffer on the ADC chip
#elif defined(STORAGE_USE_SPI_FLASH)
  #define AUDIO_BUFFER_COUNT           (10) // SPI Flash need more buffer for smooth audio
#else
  #define AUDIO_BUFFER_COUNT           (3)
#endif

#if AUDIO_SAMPLE_FMT == AUDIO_SAMPLE_FMT_S16
  typedef int16_t audio_data_t;
  #define AUDIO_DATA_SILENCE 0
#elif AUDIO_SAMPLE_FMT == AUDIO_SAMPLE_FMT_U16
  typedef uint16_t audio_data_t;
  #define AUDIO_DATA_SILENCE 0x8000
#else
  #error "Unknown audio sample format"
#endif

struct AudioBuffer {
  audio_data_t data[AUDIO_BUFFER_SIZE];
  uint16_t size;
};

extern AudioBuffer audioBuffers[AUDIO_BUFFER_COUNT];

enum FragmentTypes {
  FRAGMENT_EMPTY,
  FRAGMENT_TONE,
  FRAGMENT_FILE,
};

struct Tone {
  uint16_t freq;
  uint16_t duration;
  uint16_t pause;
  int8_t   freqIncr;
  uint8_t  reset;
  uint8_t  pure;    // use the distortion-free sine LUT (e.g. CLI "beep")
  Tone() {};
  Tone(uint16_t freq, uint16_t duration, uint16_t pause, int8_t freqIncr, bool reset, bool pure):
    freq(freq),
    duration(duration),
    pause(pause),
    freqIncr(freqIncr),
    reset(reset),
    pure(pure)
  {};
};

struct AudioFragment {
  uint8_t type;
  uint8_t id;
  uint8_t repeat;
  int8_t fragmentVolume;
  union {
    Tone tone;
    char file[AUDIO_FILENAME_MAXLEN+1];
  };

  AudioFragment() { clear(); };

  AudioFragment(uint16_t freq, uint16_t duration, uint16_t pause, uint8_t repeat, int8_t freqIncr, bool reset, int8_t fragmentVolume, bool pure=false, uint8_t id=0 ):
    type(FRAGMENT_TONE),
    id(id),
    repeat(repeat),
    fragmentVolume(fragmentVolume),
    tone(freq, duration, pause, freqIncr, reset, pure)
  {};

  AudioFragment(const char * filename, uint8_t repeat, int8_t fragmentVolume, uint8_t id = 0):
    type(FRAGMENT_FILE),
    id(id),
    repeat(repeat),
    fragmentVolume(fragmentVolume)
  {
    strcpy(file, filename);
  }

  void clear()
  {
    memset(reinterpret_cast<void*>(this), 0, sizeof(AudioFragment));

    this->fragmentVolume = USE_SETTINGS_VOLUME;
  }
};

class ToneContext {
  public:

    inline void clear()
    {
      memset(reinterpret_cast<void*>(this), 0, sizeof(ToneContext));

      fragment.fragmentVolume = USE_SETTINGS_VOLUME;
    }

    bool isFree() const
    {
      return fragment.type == FRAGMENT_EMPTY;
    }

    int mixBuffer(AudioBuffer *buffer, int volume, unsigned int fade);

    void setFragment(uint16_t freq, uint16_t duration, uint16_t pause, uint8_t repeat, int8_t freqIncr, bool reset, int8_t fragmentVolume, bool pure = false, uint8_t id = 0)
    {
      fragment = AudioFragment(freq, duration, pause, repeat, freqIncr, reset, fragmentVolume, pure, id);
    }

  private:
    AudioFragment fragment;

    struct {
      float step;
      float idx;
      float  volume;
      uint16_t freq;
      uint16_t duration;
      uint16_t pause;
    } state;

};

class WavContext {
  public:

    inline void clear() { fragment.clear(); };

    int mixBuffer(AudioBuffer *buffer, int volume, unsigned int fade);
    bool hasPromptId(uint8_t id) const { return fragment.id == id; };

    void setFragment(const char * filename, uint8_t repeat, int8_t fragmentVolume, uint8_t id)
    {
      fragment = AudioFragment(filename, repeat, fragmentVolume, id);
    }

    void stop(uint8_t id)
    {
      if (fragment.id == id) {
        fragment.clear();
      }
    }

  private:
    AudioFragment fragment;

    struct {
      FIL      file;
      uint8_t  codec;
      uint32_t freq;
      uint32_t size;
      uint8_t  resampleRatio;
      uint16_t readSize;
    } state;
};

class MixedContext {
#if defined(CLI)
  friend void printAudioVars();
#endif
  public:

    MixedContext()
    {
      clear();
    }

    void setFragment(const AudioFragment * frag)
    {
      if (frag) {
        fragment = *frag;
      }
    }

    inline void clear()
    {
      tone.clear();   // the biggest member of the uninon
    }

    bool isEmpty() const { return fragment.type == FRAGMENT_EMPTY; };
    bool isTone() const { return fragment.type == FRAGMENT_TONE; };
    bool isFile() const { return fragment.type == FRAGMENT_FILE; };
    bool hasPromptId(uint8_t id) const { return fragment.id == id; };

    int mixBuffer(AudioBuffer *buffer, int toneVolume, int wavVolume, unsigned int fade)
    {
      if (isTone())
        return tone.mixBuffer(buffer, toneVolume, fade);
      else if (isFile())
        return wav.mixBuffer(buffer, wavVolume, fade);
      return 0;
    }

  private:
    union {
      AudioFragment fragment;   // a hack: fragment is used to access the fragment members of tone and wav
      ToneContext tone;
      WavContext wav;
    };
};

class AudioBufferFifo {
#if defined(CLI)
  friend void printAudioVars();
#endif

  private:
    volatile uint8_t readIdx;
    volatile uint8_t writeIdx;

    inline uint8_t nextBufferIdx(uint8_t idx) const
    {
      return (idx >= AUDIO_BUFFER_COUNT - 1 ? 0 : idx + 1);
    }

    bool full() const { return readIdx == nextBufferIdx(writeIdx); }
    bool empty() const { return readIdx == writeIdx; }
    uint8_t used() const { return (writeIdx - readIdx) % AUDIO_BUFFER_COUNT; }

   public:
    AudioBufferFifo() : readIdx(0), writeIdx(0)
    {
      memset(audioBuffers, 0, sizeof(audioBuffers));
    }

    // returns an empty buffer to be filled with data and put back into FIFO
    // with audioPushBuffer()
    AudioBuffer *getEmptyBuffer() const
    {
      return full() ? nullptr : &audioBuffers[writeIdx];
    }

    // puts filled buffer into FIFO
    void audioPushBuffer() { writeIdx = nextBufferIdx(writeIdx); }

    // frees the last played buffer
    void freeNextFilledBuffer() { readIdx = nextBufferIdx(readIdx); }

    // returns a pointer to the audio buffer to be played
    const AudioBuffer *getNextFilledBuffer()
    {
      return empty() ? nullptr : &audioBuffers[readIdx];
    }

    bool filledAtleast(int noBuffers) const { return used() >= noBuffers; }
};

class AudioFragmentFifo
{
#if defined(CLI)
  friend void printAudioVars();
#endif
  private:
    volatile uint8_t ridx;
    volatile uint8_t widx;
    AudioFragment fragments[AUDIO_QUEUE_LENGTH];

    uint8_t nextIdx(uint8_t idx) const
    {
      return (idx + 1) & (AUDIO_QUEUE_LENGTH - 1);
    }

  public:
    AudioFragmentFifo() : ridx(0), widx(0), fragments() {};

    bool hasPromptId(uint8_t id)
    {
      uint8_t i = ridx;
      while (i != widx) {
        AudioFragment & fragment = fragments[i];
        if (fragment.id == id) return true;
        i = nextIdx(i);
      }
      return false;
    }

    bool removePromptById(uint8_t id)
    {
      uint8_t i = ridx;
      while (i != widx) {
        AudioFragment & fragment = fragments[i];
        if (fragment.id == id) fragment.clear();
        i = nextIdx(i);
      }
      return false;
    }

    bool empty() const
    {
      return ridx == widx;
    }

    bool full() const
    {
      return ridx == nextIdx(widx);
    }

    void clear()
    {
      widx = ridx;                      // clean the queue
    }

    const AudioFragment * get()
    {
      if (!empty()) {
        const AudioFragment * result = &fragments[ridx];
        if (!fragments[ridx].repeat--) {
          // repeat is done, move to the next fragment
          ridx = nextIdx(ridx);
        }
        return result;
      }
      return nullptr;
    }

    void push(const AudioFragment & fragment)
    {
      if (!full()) {
        // TRACE("fragment %d at %d", fragment.type, widx);
        fragments[widx] = fragment;
        widx = nextIdx(widx);
      }
    }

};

class AudioQueue {

#if defined(CLI)
  friend void printAudioVars();
#endif
  public:
    AudioQueue();
    void start() { _started = true; };
    void playTone(uint16_t freq, uint16_t len, uint16_t pause=0, uint8_t flags=0, int8_t freqIncr=0, int8_t fragmentVolume = USE_SETTINGS_VOLUME);
    void playFile(const char *filename, uint8_t flags=0, uint8_t id=0, int8_t fragmentVolume = USE_SETTINGS_VOLUME);
    void stopPlay(uint8_t id);
    void stopAll();
    void flush();
    void pause(uint16_t tLen);
    void setBackgroundPaused(bool paused);
    void stopSD();
    bool isPlaying(uint8_t id);
    bool isEmpty() const { return fragmentsFifo.empty(); };
    void wakeup();
    bool started() const { return _started; };
#if defined(AUDIO_UNMUTE_DELAY)
    tmr10ms_t lastAudioPlayTime = 0;
#endif

    AudioBufferFifo buffersFifo;

  private:
    volatile bool _started;
    MixedContext normalContext;
    WavContext   backgroundContext;
    bool backgroundPaused = false;
    ToneContext  priorityContext;
    ToneContext  varioContext;
    AudioFragmentFifo fragmentsFifo;
};

extern AudioQueue audioQueue;
