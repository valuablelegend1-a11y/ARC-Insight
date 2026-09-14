#include "mp3_decode.h"

#include <string.h>

namespace arcv {

bool Mp3Decoder::begin() {
  if (h_) return true;
  h_ = MP3InitDecoder();
  if (!h_) return false;
  reset();
  return true;
}

void Mp3Decoder::reset() {
  len_ = 0;
  rd_ = 0;
}

void Mp3Decoder::push(const uint8_t* data, size_t len) {
  if (!h_ || len == 0) return;
  if (len_ + len > sizeof(buf_)) {
    if (len_ > rd_) memmove(buf_, buf_ + rd_, len_ - rd_);
    len_ -= rd_;
    rd_ = 0;
  }
  memcpy(buf_ + len_, data, len);
  len_ += len;
  decodeAll();
}

void Mp3Decoder::decodeAll() {
  while (rd_ < len_) {
    unsigned char* p = buf_ + rd_;
    int bytesLeft = (int)(len_ - rd_);
    int err = MP3Decode(h_, &p, &bytesLeft, out_, 0);
    size_t consumed = (len_ - rd_) - (size_t)bytesLeft;
    rd_ += consumed;

    if (err == ERR_MP3_NONE) {
      MP3FrameInfo info;
      MP3GetLastFrameInfo(h_, &info);
      emitPcm();
      continue;
    }
    if (err == ERR_MP3_INDATA_UNDERFLOW) break;
    if (consumed == 0) {
      // Corrupt stream: skip one byte and let the decoder re-sync.
      ++rd_;
      if (rd_ >= len_) break;
    }
  }
  if (rd_) {
    size_t keep = len_ - rd_;
    if (keep) memmove(buf_, buf_ + rd_, keep);
    len_ = keep;
    rd_ = 0;
  }
}

void Mp3Decoder::emitPcm() {
  MP3FrameInfo info;
  MP3GetLastFrameInfo(h_, &info);
  int per = info.outputSamps;
  if (per <= 0) return;
  if (per > 1152) per = 1152;
  if (info.nChans == 2) {
    for (int i = 0; i < per; ++i) {
      int32_t l = out_[2 * i];
      int32_t r = out_[2 * i + 1];
      mono_[i] = (int16_t)((l + r) >> 1);
    }
    if (sink_) sink_(mono_, (size_t)per, ctx_);
  } else {
    if (sink_) sink_(out_, (size_t)per, ctx_);
  }
}

}  // namespace arcv