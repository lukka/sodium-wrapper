// nonce.h -- Sodium nonce wrapper
//
// ISC License
// 
// Copyright (C) 2018 Farid Hajji <farid@hajji.name>
// 
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
// 
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#pragma once

#include "common.h"
#include <sodium.h>

#ifndef NDEBUG
#include <iostream>
#endif // ! NDEBUG

namespace sodium {

// Typical values for number of bytes of Nonces (from <sodium.h>):
static constexpr std::size_t NONCESIZE_SECRETBOX = crypto_secretbox_NONCEBYTES;
static constexpr std::size_t NONCESIZE_AEAD      = crypto_aead_chacha20poly1305_NPUBBYTES;
static constexpr std::size_t NONCESIZE_PK        = crypto_box_NONCEBYTES;
static constexpr std::size_t NONCESIZE_CHACHA20  = crypto_stream_chacha20_NONCEBYTES;
static constexpr std::size_t NONCESIZE_XCHACHA20 = crypto_stream_xchacha20_NONCEBYTES;
static constexpr std::size_t NONCESIZE_SALSA20   = crypto_stream_salsa20_NONCEBYTES;
static constexpr std::size_t NONCESIZE_XSALSA20  = crypto_stream_NONCEBYTES;
 
template <std::size_t N=NONCESIZE_SECRETBOX>
class nonce
{
  /**
   * The class sodium::nonce<N> represents a nonce with N bytes,
   * or N*8 bits length.  A nonce is a big integer number that is
   * supposed to be used only once, thus the name: Number-used-only-ONCE.
   *
   * Nonces SHOULD be generated randomly, and MUST NOT be reused
   * ever again with the same key. They are NOT necessarily secret
   * and can even be sent over an insecure channel. Therefore, nonces
   * are kept in regular, non-protected (bytes) memory, unlike
   * sodium::Key objects whose data are allocated in protected (key_t)
   * memory.
   *
   * This template is parameterized with the number of bytes of the
   * nonce.
   **/

 public:

  /**
   * Construct a nonce of size N bytes.
   * 
   * If bool is true (the default), initialize the nonce,
   * i.e. fill it with random data generated by libsodium's
   * randombytes_buf().
   *
   * If bool is false, the nonce remains default-initialized to the
   * default value of bytes, i.e. to zero bytes.
   **/
 
  nonce(bool init=true) : noncedata_(N) {
    if (init)
      randombytes_buf(noncedata_.data(), noncedata_.size());
  }

  // there's nothing special about copy operations: allow them.
  nonce(const nonce &)            = default;
  nonce& operator=(const nonce &) = default;

  // to increase efficiency, enable moving constructor:
  nonce(nonce &&other) :
	  noncedata_{ std::move(other.noncedata_) }
  {}

 /**
  * Various libsodium functions used either directly on in
  * the wrappers need access to the bytes stored in the nonce.
  *
  * data() gives const access to those bytes of which
  * size() bytes (N) are stored in the key.
  *
  * We don't provide mutable access to the bytes by design.
  * The only functions that change those bytes are here:
  *   nonce(true), increment(), operator+=().
  *
  * !!!! IMPORTANT INVARIANT -- CHECK MANUALLY !!!!
  *
  * Note that we return N instead of noncedata_.size() in size()
  * so that size() can be declared constexpr and used in
  * static_assert() in callers.  We must make sure that the
  * invariant N == noncedata_.size() always holds when modifying
  * this class.
  **/

  const            byte        *data() const { return noncedata_.data(); }
  static constexpr std::size_t  size()       { return N; }

  /**
   * Expose noncedata_ as const bytes for sodium::tohex().
   **/

  const bytes as_bytes() const { return noncedata_; }
 
  /**
   * Increment the nonce by 1 in constant time.
   * 
   * The byte pattern stored in nonce is considered to be
   * in little endian format.
   **/
 
  void increment() {
    sodium_increment(noncedata_.data(), noncedata_.size());
  }

  nonce& operator++ () {
	  increment();
	  return *this;
  }

  nonce operator++ (int) {
	  nonce result{ *this };
	  increment();
	  return result;
  }

  /**
   * Testing if a nonce contains only zero bits in constant time
   * (for a given length).
   **/
 
  bool is_zero() {
    return sodium_is_zero(noncedata_.data(), noncedata_.size()) == 1;
  }

  /**
   * Compute (*this + b) mod (2 ^ (8*N)) in constant time
   * and store the result back in *this.
   *
   * The byte patterns stored in the nonce(s) is considered to be
   * in little endian format.
   **/
  nonce& operator+= (const nonce &b) {
    sodium_add(noncedata_.data(), b.noncedata_.data(), noncedata_.size());
    return *this;
  }
 
 private:
  bytes noncedata_; // the bytes of the nonce are stored in normal memory
};

/**
 * Compare two nonces in constant time.
 **/
template <std::size_t N>
  int compare(const nonce<N> &a, const nonce<N> &b)
{
#ifndef NDEBUG
  std::cerr << "DEBUG: nonce::compare called." << std::endl;
#endif // ! NDEBUG

  return sodium_compare(a.data(), b.data(), a.size());
}

template<std::size_t N>
  bool operator==(const nonce<N> &a, const nonce<N> &b)
{
  return compare<N>(a, b) == 0;
}

template<std::size_t N>
  bool operator!=(const nonce<N> &a, const nonce<N> &b)
{
  return compare<N>(a, b) != 0;
}

template<std::size_t N>
  bool operator<(const nonce<N> &a, const nonce<N> &b)
{
  return compare<N>(a, b) < 0;
}

template<std::size_t N>
  bool operator>(const nonce<N> &a, const nonce<N> &b)
{
  return compare<N>(a, b) > 0;
}

template<std::size_t N>
  bool operator <=(const nonce<N> &a, const nonce<N> &b)
{
  return compare<N>(a, b) <= 0;
}

template<std::size_t N>
  bool operator >=(const nonce<N> &a, const nonce<N> &b)
{
  return compare<N>(a, b) >= 0;
}

} // namespace sodium