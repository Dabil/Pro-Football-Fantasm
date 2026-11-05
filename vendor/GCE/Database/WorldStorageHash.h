#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
namespace WorldStorageHash
{
constexpr std::array<std::uint32_t, 64> Sha256K = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

inline std::uint32_t rotateRight(std::uint32_t value, int bits)
{
    return (value >> bits) | (value << (32 - bits));
}

class Sha256
{
  public:
    Sha256()
        : state_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U}
    {
    }

    void update(const std::uint8_t *data, std::size_t size)
    {
        for (std::size_t i = 0; i < size; ++i)
        {
            buffer_[bufferSize_++] = data[i];
            if (bufferSize_ == buffer_.size())
            {
                transform(buffer_.data());
                bitCount_ += 512;
                bufferSize_ = 0;
            }
        }
    }

    std::string finish()
    {
        bitCount_ += static_cast<std::uint64_t>(bufferSize_) * 8U;

        buffer_[bufferSize_++] = 0x80U;

        if (bufferSize_ > 56)
        {
            while (bufferSize_ < 64)
            {
                buffer_[bufferSize_++] = 0;
            }

            transform(buffer_.data());
            bufferSize_ = 0;
        }

        while (bufferSize_ < 56)
        {
            buffer_[bufferSize_++] = 0;
        }

        for (int i = 7; i >= 0; --i)
        {
            buffer_[bufferSize_++] = static_cast<std::uint8_t>((bitCount_ >> (i * 8)) & 0xffU);
        }

        transform(buffer_.data());

        static constexpr char Hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(64);

        for (std::uint32_t value : state_)
        {
            for (int shift = 28; shift >= 0; shift -= 4)
            {
                result.push_back(Hex[(value >> shift) & 0x0fU]);
            }
        }

        return result;
    }

  private:
    void transform(const std::uint8_t *block)
    {
        std::uint32_t words[64]{};

        for (int i = 0; i < 16; ++i)
        {
            words[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                       (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                       (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                       static_cast<std::uint32_t>(block[i * 4 + 3]);
        }

        for (int i = 16; i < 64; ++i)
        {
            const std::uint32_t s0 =
                rotateRight(words[i - 15], 7) ^ rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);

            const std::uint32_t s1 =
                rotateRight(words[i - 2], 17) ^ rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);

            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (int i = 0; i < 64; ++i)
        {
            const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);

            const std::uint32_t choose = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + sum1 + choose + Sha256K[i] + words[i];

            const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);

            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);

            const std::uint32_t temp2 = sum0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t bufferSize_ = 0;
    std::uint64_t bitCount_ = 0;
};

inline std::string bytes(const std::string &data)
{
    Sha256 h;
    h.update(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
    return h.finish();
}
inline std::string file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot open hashed input");
    Sha256 h;
    std::array<char, 65536> b{};
    while (in)
    {
        in.read(b.data(), b.size());
        h.update(reinterpret_cast<const std::uint8_t *>(b.data()), static_cast<std::size_t>(in.gcount()));
    }
    if (!in.eof())
        throw std::runtime_error("Cannot read hashed input");
    return h.finish();
}
} // namespace WorldStorageHash
