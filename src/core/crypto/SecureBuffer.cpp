#include "core/crypto/SecureBuffer.h"

#include <new> // std::bad_alloc

#include <sodium.h>

namespace pm::crypto {

SecureBuffer::SecureBuffer(std::size_t size) : size_(size)
{
    if (size_ == 0) {
        return;
    }
    // sodium_malloc adds guard pages, locks the memory, and arranges for it to
    // be zeroed on sodium_free.
    data_ = static_cast<std::uint8_t *>(sodium_malloc(size_));
    if (data_ == nullptr) {
        size_ = 0;
        throw std::bad_alloc();
    }
}

SecureBuffer::SecureBuffer(SecureBuffer &&other) noexcept
    : data_(other.data_), size_(other.size_)
{
    other.data_ = nullptr;
    other.size_ = 0;
}

SecureBuffer &SecureBuffer::operator=(SecureBuffer &&other) noexcept
{
    if (this != &other) {
        reset();
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

SecureBuffer::~SecureBuffer()
{
    reset();
}

void SecureBuffer::reset() noexcept
{
    if (data_ != nullptr) {
        sodium_free(data_); // zeroes the memory before releasing it
        data_ = nullptr;
    }
    size_ = 0;
}

void SecureBuffer::makeReadOnly() noexcept
{
    if (data_ != nullptr) {
        sodium_mprotect_readonly(data_);
    }
}

void SecureBuffer::makeReadWrite() noexcept
{
    if (data_ != nullptr) {
        sodium_mprotect_readwrite(data_);
    }
}

} // namespace pm::crypto
