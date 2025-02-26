/*
 * Copyright (c) 2021, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/MaybeOwned.h>
#include <AK/OwnPtr.h>
#include <AK/Stream.h>

namespace AK {

/// A stream wrapper class that allows you to read arbitrary amounts of bits
/// in big-endian order from another stream.
class BigEndianInputBitStream : public Stream {
public:
    explicit BigEndianInputBitStream(MaybeOwned<Stream> stream)
        : m_stream(move(stream))
    {
    }

    // ^Stream
    virtual ErrorOr<Bytes> read_some(Bytes bytes) override
    {
        if (m_current_byte.has_value() && is_aligned_to_byte_boundary()) {
            bytes[0] = m_current_byte.release_value();
            // FIXME: This accidentally slices off the first byte of the returned span.
            return m_stream->read_some(bytes.slice(1));
        }
        align_to_byte_boundary();
        return m_stream->read_some(bytes);
    }
    virtual ErrorOr<size_t> write_some(ReadonlyBytes bytes) override { return m_stream->write_some(bytes); }
    virtual bool is_eof() const override { return m_stream->is_eof() && !m_current_byte.has_value(); }
    virtual bool is_open() const override { return m_stream->is_open(); }
    virtual void close() override
    {
        m_stream->close();
        align_to_byte_boundary();
    }

    ErrorOr<bool> read_bit()
    {
        return read_bits<bool>(1);
    }
    /// Depending on the number of bits to read, the return type can be chosen appropriately.
    /// This avoids a bunch of static_cast<>'s for the user.
    // TODO: Support u128, u256 etc. as well: The concepts would be quite complex.
    template<Unsigned T = u64>
    ErrorOr<T> read_bits(size_t count)
    {
        if constexpr (IsSame<bool, T>) {
            VERIFY(count == 1);
        }
        T result = 0;

        size_t nread = 0;
        while (nread < count) {
            if (m_current_byte.has_value()) {
                if constexpr (!IsSame<bool, T> && !IsSame<u8, T>) {
                    // read as many bytes as possible directly
                    if (((count - nread) >= 8) && is_aligned_to_byte_boundary()) {
                        // shift existing data over
                        result <<= 8;
                        result |= m_current_byte.value();
                        nread += 8;
                        m_current_byte.clear();
                    } else {
                        auto const bit = (m_current_byte.value() >> (7 - m_bit_offset)) & 1;
                        result <<= 1;
                        result |= bit;
                        ++nread;
                        if (m_bit_offset++ == 7)
                            m_current_byte.clear();
                    }
                } else {
                    // Always take this branch for booleans or u8: there's no purpose in reading more than a single bit
                    auto const bit = (m_current_byte.value() >> (7 - m_bit_offset)) & 1;
                    if constexpr (IsSame<bool, T>)
                        result = bit;
                    else {
                        result <<= 1;
                        result |= bit;
                    }
                    ++nread;
                    if (m_bit_offset++ == 7)
                        m_current_byte.clear();
                }
            } else {
                m_current_byte = TRY(m_stream->read_value<u8>());
                m_bit_offset = 0;
            }
        }

        return result;
    }

    /// Discards any sub-byte stream positioning the input stream may be keeping track of.
    /// Non-bitwise reads will implicitly call this.
    void align_to_byte_boundary()
    {
        m_current_byte.clear();
        m_bit_offset = 0;
    }

    /// Whether we are (accidentally or intentionally) at a byte boundary right now.
    ALWAYS_INLINE bool is_aligned_to_byte_boundary() const { return m_bit_offset == 0; }

private:
    Optional<u8> m_current_byte;
    size_t m_bit_offset { 0 };
    MaybeOwned<Stream> m_stream;
};

/// A stream wrapper class that allows you to read arbitrary amounts of bits
/// in little-endian order from another stream.
class LittleEndianInputBitStream : public Stream {
public:
    explicit LittleEndianInputBitStream(MaybeOwned<Stream> stream)
        : m_stream(move(stream))
    {
    }

    // ^Stream
    virtual ErrorOr<Bytes> read_some(Bytes bytes) override
    {
        if (m_current_byte.has_value() && is_aligned_to_byte_boundary()) {
            bytes[0] = m_current_byte.release_value();
            // FIXME: This accidentally slices off the first byte of the returned span.
            return m_stream->read_some(bytes.slice(1));
        }
        align_to_byte_boundary();
        return m_stream->read_some(bytes);
    }
    virtual ErrorOr<size_t> write_some(ReadonlyBytes bytes) override { return m_stream->write_some(bytes); }
    virtual bool is_eof() const override { return m_stream->is_eof() && !m_current_byte.has_value(); }
    virtual bool is_open() const override { return m_stream->is_open(); }
    virtual void close() override
    {
        m_stream->close();
        align_to_byte_boundary();
    }

    ErrorOr<bool> read_bit()
    {
        return read_bits<bool>(1);
    }
    /// Depending on the number of bits to read, the return type can be chosen appropriately.
    /// This avoids a bunch of static_cast<>'s for the user.
    // TODO: Support u128, u256 etc. as well: The concepts would be quite complex.
    template<Unsigned T = u64>
    ErrorOr<T> read_bits(size_t count)
    {
        if constexpr (IsSame<bool, T>) {
            VERIFY(count == 1);
        }
        T result = 0;

        size_t nread = 0;
        while (nread < count) {
            if (m_current_byte.has_value()) {
                if constexpr (!IsSame<bool, T> && !IsSame<u8, T>) {
                    // read as many bytes as possible directly
                    if (((count - nread) >= 8) && is_aligned_to_byte_boundary()) {
                        // shift existing data over
                        result |= (m_current_byte.value() << nread);
                        nread += 8;
                        m_current_byte.clear();
                    } else {
                        auto const bit = (m_current_byte.value() >> m_bit_offset) & 1;
                        result |= (bit << nread);
                        ++nread;
                        if (m_bit_offset++ == 7)
                            m_current_byte.clear();
                    }
                } else {
                    // Always take this branch for booleans or u8: there's no purpose in reading more than a single bit
                    auto const bit = (m_current_byte.value() >> m_bit_offset) & 1;
                    if constexpr (IsSame<bool, T>)
                        result = bit;
                    else
                        result |= (bit << nread);
                    ++nread;
                    if (m_bit_offset++ == 7)
                        m_current_byte.clear();
                }
            } else {
                m_current_byte = TRY(m_stream->read_value<u8>());
                m_bit_offset = 0;
            }
        }

        return result;
    }

    /// Discards any sub-byte stream positioning the input stream may be keeping track of.
    /// Non-bitwise reads will implicitly call this.
    u8 align_to_byte_boundary()
    {
        u8 remaining_bits = m_current_byte.value_or(0) >> m_bit_offset;
        m_current_byte.clear();
        m_bit_offset = 0;
        return remaining_bits;
    }

    /// Whether we are (accidentally or intentionally) at a byte boundary right now.
    ALWAYS_INLINE bool is_aligned_to_byte_boundary() const { return m_bit_offset == 0; }

private:
    Optional<u8> m_current_byte;
    size_t m_bit_offset { 0 };
    MaybeOwned<Stream> m_stream;
};

/// A stream wrapper class that allows you to write arbitrary amounts of bits
/// in big-endian order to another stream.
class BigEndianOutputBitStream : public Stream {
public:
    explicit BigEndianOutputBitStream(MaybeOwned<Stream> stream)
        : m_stream(move(stream))
    {
    }

    virtual ErrorOr<Bytes> read_some(Bytes) override
    {
        return Error::from_errno(EBADF);
    }

    virtual ErrorOr<size_t> write_some(ReadonlyBytes bytes) override
    {
        VERIFY(m_bit_offset == 0);
        return m_stream->write_some(bytes);
    }

    template<Unsigned T>
    ErrorOr<void> write_bits(T value, size_t bit_count)
    {
        VERIFY(m_bit_offset <= 7);

        while (bit_count > 0) {
            u8 next_bit = (value >> (bit_count - 1)) & 1;
            bit_count--;

            m_current_byte <<= 1;
            m_current_byte |= next_bit;
            m_bit_offset++;

            if (m_bit_offset > 7) {
                TRY(m_stream->write_value(m_current_byte));
                m_bit_offset = 0;
                m_current_byte = 0;
            }
        }

        return {};
    }

    virtual bool is_eof() const override
    {
        return true;
    }

    virtual bool is_open() const override
    {
        return m_stream->is_open();
    }

    virtual void close() override
    {
    }

    size_t bit_offset() const
    {
        return m_bit_offset;
    }

    ErrorOr<void> align_to_byte_boundary()
    {
        if (m_bit_offset == 0)
            return {};

        TRY(write_bits(0u, 8 - m_bit_offset));
        VERIFY(m_bit_offset == 0);
        return {};
    }

private:
    MaybeOwned<Stream> m_stream;
    u8 m_current_byte { 0 };
    size_t m_bit_offset { 0 };
};

/// A stream wrapper class that allows you to write arbitrary amounts of bits
/// in little-endian order to another stream.
class LittleEndianOutputBitStream : public Stream {
public:
    explicit LittleEndianOutputBitStream(MaybeOwned<Stream> stream)
        : m_stream(move(stream))
    {
    }

    virtual ErrorOr<Bytes> read_some(Bytes) override
    {
        return Error::from_errno(EBADF);
    }

    virtual ErrorOr<size_t> write_some(ReadonlyBytes bytes) override
    {
        VERIFY(m_bit_offset == 0);
        return m_stream->write_some(bytes);
    }

    template<Unsigned T>
    ErrorOr<void> write_bits(T value, size_t bit_count)
    {
        VERIFY(m_bit_offset <= 7);

        size_t input_offset = 0;
        while (input_offset < bit_count) {
            u8 next_bit = (value >> input_offset) & 1;
            input_offset++;

            m_current_byte |= next_bit << m_bit_offset;
            m_bit_offset++;

            if (m_bit_offset > 7) {
                TRY(m_stream->write_value(m_current_byte));
                m_bit_offset = 0;
                m_current_byte = 0;
            }
        }

        return {};
    }

    virtual bool is_eof() const override
    {
        return true;
    }

    virtual bool is_open() const override
    {
        return m_stream->is_open();
    }

    virtual void close() override
    {
    }

    size_t bit_offset() const
    {
        return m_bit_offset;
    }

    ErrorOr<void> align_to_byte_boundary()
    {
        if (m_bit_offset == 0)
            return {};

        TRY(write_bits(0u, 8 - m_bit_offset));
        VERIFY(m_bit_offset == 0);
        return {};
    }

private:
    MaybeOwned<Stream> m_stream;
    u8 m_current_byte { 0 };
    size_t m_bit_offset { 0 };
};

}
