#pragma once

#include <cstddef>
#include <cstdint>

class AlbumArt {
public:
  static constexpr int kMaxSize = 120;

  enum Format { kNone, kJPEG, kPNG };

  AlbumArt();
  ~AlbumArt();

  bool load(const char *mp3Path);
  void clear();

  bool hasArt() const { return format_ != kNone; }
  Format format() const { return format_; }
  bool isJPEG() const { return format_ == kJPEG; }
  bool isPNG() const { return format_ == kPNG; }

  int width() const { return img_w_; }
  int height() const { return img_h_; }

  bool render1Bit(const char *mp3Path, uint8_t *einkBuf, int bufW, int bufH,
                  int outX, int outY, int maxSize = kMaxSize);

private:
  bool extractAPIC_(const char *mp3Path);

  Format format_;
  int img_w_;
  int img_h_;
  uint32_t api_offset_;   
  uint32_t api_size_;     
};
