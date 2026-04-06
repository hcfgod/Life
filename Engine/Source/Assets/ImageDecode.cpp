#include "Assets/ImageDecode.h"

#include "Core/Log.h"

#include "stb_image.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>

namespace Life::Assets
{
    static bool EndsWithCaseInsensitive(const std::string& s, const char* suffix)
    {
        if (!suffix) return false;
        const size_t suffixLen = std::strlen(suffix);
        if (s.size() < suffixLen) return false;
        const size_t start = s.size() - suffixLen;
        for (size_t i = 0; i < suffixLen; ++i)
        {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[start + i])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
            if (a != b) return false;
        }
        return true;
    }

    static bool IsJpegPath(const std::string& path)
    {
        return EndsWithCaseInsensitive(path, ".jpg") || EndsWithCaseInsensitive(path, ".jpeg");
    }

    static bool LooksLikeJpeg(const uint8_t* bytes, size_t byteCount)
    {
        return bytes && byteCount >= 2 && bytes[0] == 0xFF && bytes[1] == 0xD8;
    }

    static uint16_t ReadBE16(const uint8_t* p)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]));
    }

    static uint16_t ReadLE16(const uint8_t* p)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(p[1]) << 8) | static_cast<uint16_t>(p[0]));
    }

    static uint32_t ReadBE32(const uint8_t* p)
    {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8)  | (static_cast<uint32_t>(p[3]) << 0);
    }

    static uint32_t ReadLE32(const uint8_t* p)
    {
        return (static_cast<uint32_t>(p[3]) << 24) | (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[1]) << 8)  | (static_cast<uint32_t>(p[0]) << 0);
    }

    static int ReadExifOrientationFromJpegBytes(const uint8_t* bytes, size_t byteCount)
    {
        if (!LooksLikeJpeg(bytes, byteCount)) return 1;

        size_t pos = 2;
        while (pos + 4 <= byteCount)
        {
            if (bytes[pos] != 0xFF) break;
            const uint8_t marker = bytes[pos + 1];
            pos += 2;

            if (marker == 0xDA || marker == 0xD9) break;
            if (pos + 2 > byteCount) break;

            const uint16_t segLen = ReadBE16(bytes + pos);
            pos += 2;
            if (segLen < 2) break;

            const size_t segPayloadStart = pos;
            const size_t segPayloadEnd = pos + (static_cast<size_t>(segLen) - 2u);
            if (segPayloadEnd > byteCount) break;

            if (marker == 0xE1 && (segPayloadEnd - segPayloadStart) >= 6)
            {
                const uint8_t* payload = bytes + segPayloadStart;
                const size_t payloadSize = segPayloadEnd - segPayloadStart;

                if (payloadSize >= 6 &&
                    payload[0] == 'E' && payload[1] == 'x' && payload[2] == 'i' && payload[3] == 'f' &&
                    payload[4] == 0x00 && payload[5] == 0x00)
                {
                    const uint8_t* tiff = payload + 6;
                    const size_t tiffSize = payloadSize - 6;
                    if (tiffSize < 8) return 1;

                    const bool littleEndian = (tiff[0] == 'I' && tiff[1] == 'I');
                    const bool bigEndian = (tiff[0] == 'M' && tiff[1] == 'M');
                    if (!littleEndian && !bigEndian) return 1;

                    const auto read16 = littleEndian ? ReadLE16 : ReadBE16;
                    const auto read32 = littleEndian ? ReadLE32 : ReadBE32;

                    const uint16_t magic = read16(tiff + 2);
                    if (magic != 42) return 1;

                    const uint32_t ifd0Offset = read32(tiff + 4);
                    if (ifd0Offset >= tiffSize || (tiffSize - ifd0Offset) < 2) return 1;

                    const uint8_t* ifd0 = tiff + ifd0Offset;
                    const uint16_t entryCount = read16(ifd0);
                    const size_t entriesBytes = static_cast<size_t>(entryCount) * 12u;
                    if ((tiffSize - ifd0Offset) < (2u + entriesBytes)) return 1;

                    for (uint16_t i = 0; i < entryCount; ++i)
                    {
                        const uint8_t* entry = ifd0 + 2u + static_cast<size_t>(i) * 12u;
                        const uint16_t tag = read16(entry + 0);
                        const uint16_t type = read16(entry + 2);
                        const uint32_t count = read32(entry + 4);

                        if (tag == 0x0112 && type == 3 && count == 1)
                        {
                            const uint16_t value = read16(entry + 8);
                            if (value >= 1 && value <= 8)
                            {
                                return static_cast<int>(value);
                            }
                            return 1;
                        }
                    }
                    return 1;
                }
            }

            pos = segPayloadEnd;
        }

        return 1;
    }

    static void ApplyExifOrientationRGBA8(DecodedImageRGBA8& img, int orientation)
    {
        if (orientation <= 1 || orientation > 8) return;
        if (img.Width == 0 || img.Height == 0 || img.Pixels.empty()) return;

        const uint32_t srcW = img.Width;
        const uint32_t srcH = img.Height;
        const uint8_t* src = img.Pixels.data();

        const bool swapWH = (orientation == 5 || orientation == 6 || orientation == 7 || orientation == 8);
        const uint32_t dstW = swapWH ? srcH : srcW;
        const uint32_t dstH = swapWH ? srcW : srcH;

        std::vector<uint8_t> dstPixels(static_cast<size_t>(dstW) * static_cast<size_t>(dstH) * 4u);

        auto srcIndex = [srcW](uint32_t x, uint32_t y) -> size_t {
            return (static_cast<size_t>(y) * static_cast<size_t>(srcW) + static_cast<size_t>(x)) * 4u;
        };
        auto dstIndex = [dstW](uint32_t x, uint32_t y) -> size_t {
            return (static_cast<size_t>(y) * static_cast<size_t>(dstW) + static_cast<size_t>(x)) * 4u;
        };

        for (uint32_t y = 0; y < dstH; ++y)
        {
            for (uint32_t x = 0; x < dstW; ++x)
            {
                uint32_t sx = 0, sy = 0;
                switch (orientation)
                {
                    case 2: sx = (srcW - 1u) - x; sy = y; break;
                    case 3: sx = (srcW - 1u) - x; sy = (srcH - 1u) - y; break;
                    case 4: sx = x; sy = (srcH - 1u) - y; break;
                    case 5: sx = y; sy = x; break;
                    case 6: sx = y; sy = (srcH - 1u) - x; break;
                    case 7: sx = (srcW - 1u) - y; sy = (srcH - 1u) - x; break;
                    case 8: sx = (srcW - 1u) - y; sy = x; break;
                    default: sx = x; sy = y; break;
                }

                const size_t si = srcIndex(sx, sy);
                const size_t di = dstIndex(x, y);
                dstPixels[di + 0] = src[si + 0];
                dstPixels[di + 1] = src[si + 1];
                dstPixels[di + 2] = src[si + 2];
                dstPixels[di + 3] = src[si + 3];
            }
        }

        img.Width = dstW;
        img.Height = dstH;
        img.Pixels = std::move(dstPixels);
    }

    void FlipVerticalRGBA8(DecodedImageRGBA8& img)
    {
        if (img.Width == 0 || img.Height == 0 || img.Pixels.empty()) return;

        const uint32_t w = img.Width;
        const uint32_t h = img.Height;
        const size_t rowBytes = static_cast<size_t>(w) * 4u;

        std::vector<uint8_t> tempRow(rowBytes);
        for (uint32_t y = 0; y < h / 2u; ++y)
        {
            uint8_t* rowA = img.Pixels.data() + static_cast<size_t>(y) * rowBytes;
            uint8_t* rowB = img.Pixels.data() + static_cast<size_t>(h - 1u - y) * rowBytes;
            std::memcpy(tempRow.data(), rowA, rowBytes);
            std::memcpy(rowA, rowB, rowBytes);
            std::memcpy(rowB, tempRow.data(), rowBytes);
        }
    }

    Result<DecodedImageRGBA8> DecodeToRGBA8(const std::string& path, bool flipVerticallyOnLoad)
    {
        static std::mutex s_StbMutex;
        std::lock_guard<std::mutex> lock(s_StbMutex);

        int exifOrientation = 1;
        std::vector<uint8_t> fileBytes;
        if (IsJpegPath(path))
        {
            std::ifstream in(path, std::ios::in | std::ios::binary);
            if (in.is_open())
            {
                std::vector<uint8_t> header;
                header.resize(static_cast<std::vector<uint8_t>::size_type>(64u) * 1024u);
                in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
                const std::streamsize got = in.gcount();
                if (got > 0)
                {
                    exifOrientation = ReadExifOrientationFromJpegBytes(header.data(), static_cast<size_t>(got));
                }
            }
        }

        {
            std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate);
            if (!in.is_open())
            {
                return Result<DecodedImageRGBA8>(ErrorCode::ResourceNotFound, "Failed to open image: " + path);
            }

            const std::streamoff size = in.tellg();
            if (size <= 0)
            {
                return Result<DecodedImageRGBA8>(ErrorCode::ResourceNotFound, "Image file is empty: " + path);
            }

            fileBytes.resize(static_cast<size_t>(size));
            in.seekg(0, std::ios::beg);
            in.read(reinterpret_cast<char*>(fileBytes.data()), static_cast<std::streamsize>(fileBytes.size()));
            if (!in)
            {
                return Result<DecodedImageRGBA8>(ErrorCode::FileCorrupted, "Failed to read image bytes: " + path);
            }
        }

        int w = 0, h = 0, channels = 0;
        stbi_uc* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(fileBytes.data()), static_cast<int>(fileBytes.size()), &w, &h, &channels, 4);
        if (!data || w <= 0 || h <= 0)
        {
            if (data) stbi_image_free(data);
            std::string message = "Failed to decode image: " + path;
            return Result<DecodedImageRGBA8>(ErrorCode::ResourceNotFound, message);
        }

        DecodedImageRGBA8 img;
        img.Width = static_cast<uint32_t>(w);
        img.Height = static_cast<uint32_t>(h);
        img.Pixels.resize(static_cast<size_t>(img.Width) * static_cast<size_t>(img.Height) * 4u);
        std::memcpy(img.Pixels.data(), data, img.Pixels.size());
        stbi_image_free(data);

        ApplyExifOrientationRGBA8(img, exifOrientation);
        if (flipVerticallyOnLoad)
        {
            FlipVerticalRGBA8(img);
        }
        return img;
    }

    Result<DecodedImageRGBA8> DecodeToRGBA8FromMemory(const uint8_t* bytes, size_t byteCount, const std::string& debugName, bool flipVerticallyOnLoad)
    {
        if (!bytes || byteCount == 0)
        {
            return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "DecodeToRGBA8FromMemory: empty input: " + debugName);
        }

        static std::mutex s_StbMutex;
        std::lock_guard<std::mutex> lock(s_StbMutex);

        const int exifOrientation = LooksLikeJpeg(bytes, byteCount)
            ? ReadExifOrientationFromJpegBytes(bytes, byteCount)
            : 1;

        int w = 0, h = 0, channels = 0;
        stbi_uc* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes), static_cast<int>(byteCount), &w, &h, &channels, 4);
        if (!data || w <= 0 || h <= 0)
        {
            if (data) stbi_image_free(data);
            std::string message = "Failed to decode image from memory: " + debugName;
            return Result<DecodedImageRGBA8>(ErrorCode::ResourceNotFound, message);
        }

        DecodedImageRGBA8 img;
        img.Width = static_cast<uint32_t>(w);
        img.Height = static_cast<uint32_t>(h);
        img.Pixels.resize(static_cast<size_t>(img.Width) * static_cast<size_t>(img.Height) * 4u);
        std::memcpy(img.Pixels.data(), data, img.Pixels.size());
        stbi_image_free(data);

        ApplyExifOrientationRGBA8(img, exifOrientation);
        if (flipVerticallyOnLoad)
        {
            FlipVerticalRGBA8(img);
        }
        return img;
    }

    struct PpmTokenReader
    {
        explicit PpmTokenReader(std::istream& in) : m_In(in) {}

        bool ReadToken(std::string& out)
        {
            out.clear();
            while (true)
            {
                int c = m_In.get();
                if (!m_In) return false;
                if (std::isspace(static_cast<unsigned char>(c)) != 0) continue;
                if (c == '#') { std::string dummy; std::getline(m_In, dummy); continue; }
                out.push_back(static_cast<char>(c));
                break;
            }
            while (true)
            {
                const int c = m_In.peek();
                if (!m_In) break;
                if (std::isspace(static_cast<unsigned char>(c)) != 0 || c == '#') break;
                out.push_back(static_cast<char>(m_In.get()));
            }
            return true;
        }

        std::istream& m_In;
    };

    Result<DecodedImageRGBA8> TryDecodePpmP3ToRGBA8(const std::string& path)
    {
        std::ifstream in(path, std::ios::in);
        if (!in.is_open())
        {
            return Result<DecodedImageRGBA8>(ErrorCode::ResourceNotFound, "Failed to open PPM file: " + path);
        }

        PpmTokenReader reader(in);
        std::string token;

        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (empty file): " + path);
        if (token != "P3") return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "PPM is not ASCII P3: " + path);

        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (missing width): " + path);
        const int width = std::stoi(token);
        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (missing height): " + path);
        const int height = std::stoi(token);
        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (missing max value): " + path);
        const int maxValue = std::stoi(token);

        if (width <= 0 || height <= 0) return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "PPM invalid dimensions: " + path);
        if (maxValue <= 0 || maxValue > 255) return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "PPM maxValue must be 1..255: " + path);

        DecodedImageRGBA8 img;
        img.Width = static_cast<uint32_t>(width);
        img.Height = static_cast<uint32_t>(height);
        img.Pixels.resize(static_cast<size_t>(img.Width) * static_cast<size_t>(img.Height) * 4u);

        const auto scale = [maxValue](int v) -> uint8_t {
            if (v < 0) v = 0;
            if (v > maxValue) v = maxValue;
            const float f = static_cast<float>(v) / static_cast<float>(maxValue);
            const int out = static_cast<int>(std::lround(static_cast<double>(f) * 255.0));
            return static_cast<uint8_t>(out < 0 ? 0 : (out > 255 ? 255 : out));
        };

        for (uint32_t i = 0; i < img.Width * img.Height; ++i)
        {
            std::string rTok, gTok, bTok;
            if (!reader.ReadToken(rTok) || !reader.ReadToken(gTok) || !reader.ReadToken(bTok))
            {
                return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (not enough pixel data): " + path);
            }
            const size_t base = static_cast<size_t>(i) * 4u;
            img.Pixels[base + 0] = scale(std::stoi(rTok));
            img.Pixels[base + 1] = scale(std::stoi(gTok));
            img.Pixels[base + 2] = scale(std::stoi(bTok));
            img.Pixels[base + 3] = 255;
        }

        return img;
    }

    Result<DecodedImageRGBA8> TryDecodePpmP3ToRGBA8FromMemory(const uint8_t* bytes, const size_t byteCount, const std::string& debugName)
    {
        if (!bytes || byteCount == 0)
        {
            return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "PPM parse failed (empty input): " + debugName);
        }

        std::string text(reinterpret_cast<const char*>(bytes), byteCount);
        std::istringstream in(text);
        PpmTokenReader reader(in);

        std::string token;
        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (empty stream): " + debugName);
        if (token != "P3") return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "PPM is not ASCII P3: " + debugName);

        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (missing width): " + debugName);
        const int width = std::stoi(token);
        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (missing height): " + debugName);
        const int height = std::stoi(token);
        if (!reader.ReadToken(token)) return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (missing max value): " + debugName);
        const int maxValue = std::stoi(token);

        if (width <= 0 || height <= 0) return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "PPM invalid dimensions: " + debugName);
        if (maxValue <= 0 || maxValue > 255) return Result<DecodedImageRGBA8>(ErrorCode::InvalidArgument, "PPM maxValue must be 1..255: " + debugName);

        DecodedImageRGBA8 img;
        img.Width = static_cast<uint32_t>(width);
        img.Height = static_cast<uint32_t>(height);
        img.Pixels.resize(static_cast<size_t>(img.Width) * static_cast<size_t>(img.Height) * 4u);

        const auto scale = [maxValue](int v) -> uint8_t {
            if (v < 0) v = 0;
            if (v > maxValue) v = maxValue;
            const float f = static_cast<float>(v) / static_cast<float>(maxValue);
            const int out = static_cast<int>(std::lround(static_cast<double>(f) * 255.0));
            return static_cast<uint8_t>(out < 0 ? 0 : (out > 255 ? 255 : out));
        };

        for (uint32_t i = 0; i < img.Width * img.Height; ++i)
        {
            std::string rTok, gTok, bTok;
            if (!reader.ReadToken(rTok) || !reader.ReadToken(gTok) || !reader.ReadToken(bTok))
            {
                return Result<DecodedImageRGBA8>(ErrorCode::FileAccessDenied, "PPM parse failed (not enough pixel data): " + debugName);
            }
            const size_t base = static_cast<size_t>(i) * 4u;
            img.Pixels[base + 0] = scale(std::stoi(rTok));
            img.Pixels[base + 1] = scale(std::stoi(gTok));
            img.Pixels[base + 2] = scale(std::stoi(bTok));
            img.Pixels[base + 3] = 255;
        }

        return img;
    }
}
