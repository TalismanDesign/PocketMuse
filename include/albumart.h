#pragma once

#include <cstdint>
#include <cstddef>

class AlbumArt {
public:
    static constexpr int kMaxSize = 120;

    enum Format { kNone, kJPEG, kPNG };

    AlbumArt();
    ~AlbumArt();

    // Extract APIC from mp3Path and cache it. Returns true on success.
    bool load(const char* mp3Path);
    void clear();

    bool   hasArt() const { return format_ != kNone; }
    Format format() const { return format_; }
    bool   isJPEG() const { return format_ == kJPEG; }
    bool   isPNG()  const { return format_ == kPNG; }

    int  width() const  { return img_w_; }
    int  height() const { return img_h_; }

    // Render cached art to a 1-bit e-ink buffer at (outX, outY) bounded by
    // maxSize. Uses pm_image_decode_to_1bit.
    bool render1Bit(uint8_t* einkBuf, int bufW, int bufH,
                    int outX, int outY, int maxSize = kMaxSize);

private:
    bool extractAPIC_(const char* mp3Path);
    bool detectFormat_();

    Format format_;
    int  img_w_;
    int  img_h_;
};
