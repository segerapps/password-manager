#pragma once

#include <cstddef>
#include <cstdint>

namespace pm::crypto {

// RAII wrapper over libsodium's guarded secure memory (`sodium_malloc`):
//   - pages are mlock'd (kept out of the swap file) and fenced with guard pages
//   - memory is zeroed automatically on free (`sodium_free`)
//
// Use this for every plaintext secret that lives in memory: master-password
// material, the KEK, and the DEK. Non-copyable (a secret should have exactly
// one owner); movable.
class SecureBuffer
{
public:
    SecureBuffer() = default;
    explicit SecureBuffer(std::size_t size);
    ~SecureBuffer();

    SecureBuffer(const SecureBuffer &) = delete;
    SecureBuffer &operator=(const SecureBuffer &) = delete;

    SecureBuffer(SecureBuffer &&other) noexcept;
    SecureBuffer &operator=(SecureBuffer &&other) noexcept;

    std::uint8_t *data() noexcept { return data_; }
    const std::uint8_t *data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    // Optional hardening: flip the guarded region read-only / read-write
    // (no-ops on an empty buffer). Keys can be made read-only after derivation.
    void makeReadOnly() noexcept;
    void makeReadWrite() noexcept;

private:
    void reset() noexcept;

    std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace pm::crypto
