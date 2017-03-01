// sodiumnonce.h -- Sodium Nonce Wrapper
//
// Copyright (C) 2017 Farid Hajji <farid@hajji.name>. All rights reserved.

#ifndef _SODIUMNONCE_H_
#define _SODIUMNONCE_H_

#include <cstddef>
#include <vector>

#include <sodium.h>

namespace Sodium {

// Typical values for number of bytes of Nonces (from <sodium.h>):
static constexpr std::size_t NONCESIZE_SECRETBOX = crypto_secretbox_NONCEBYTES;
static constexpr std::size_t NONCESIZE_AEAD      = crypto_aead_chacha20poly1305_NPUBBYTES;
 
template <unsigned int N=NONCESIZE_SECRETBOX>
class Nonce
{
  /**
   * The class Sodium::Nonce<N> represents a nonce with N bytes,
   * or N*8 bits length.  A nonce is a big integer number that is
   * supposed to be used only once, thus the name: Number-used-only-ONCE.
   *
   * Nonces SHOULD be generated randomly, and MUST NOT be reused
   * ever again with the same key. They are NOT necessarily secret
   * and can even be sent over an insecure channel. Therefore, Nonces
   * are kept in regular, non-protected (data_t) memory, unlike
   * Sodium::Key objects whose data are allocated in protected (key_t)
   * memory.
   *
   * This template is parameterized with the number of bytes of the
   * nonce.
   **/

 public:
  // XXX typedef unsigned char value_type;
 
  // data_t is unprotected memory (just plain bytes)
  using data_t = std::vector<unsigned char>;

  /**
   * Construct a Nonce of size N bytes.
   * 
   * If bool is true (the default), initialize the nonce,
   * i.e. fill it with random data generated by libsodium's
   * randombytes_buf().
   *
   * If bool is false, the nonce remains default-initialized to the
   * default value of data_t, i.e. to zero bytes.
   **/
 
  Nonce<N>(bool init=true) : noncedata(N) {
    if (init)
      randombytes_buf(noncedata.data(), noncedata.size());
  }

  // there's nothing special about copy operations: allow them.
  Nonce<N>(const Nonce<N> &)            = default;
  Nonce<N>& operator=(const Nonce<N> &) = default;

 /**
  * Various libsodium functions used either directly on in
  * the wrappers need access to the bytes stored in the nonce.
  *
  * data() gives const access to those bytes of which
  * size() bytes (N) are stored in the key.
  *
  * We don't provide mutable access to the btes by design.
  * The only functions that change those bytes are here:
  *   Nonce(true), increment(), operator+=().
  *
  * !!!! IMPORTANT INVARIANT -- CHECK MANUALLY !!!!
  *
  * Note that we return N instead of noncedata.size() in size()
  * so that size() can be declared constexpr and used in
  * static_assert() in callers.  We must make sure that the
  * invariant N == noncedata.size() always holds when modifying
  * this class.
  **/

            const unsigned char *data() const { return noncedata.data(); }
  constexpr const std::size_t    size() const { return N; }
 
  /**
   * Increment the Nonce by 1 in constant time.
   * 
   * The byte pattern stored in Nonce is considered to be
   * in little endian format.
   **/
 
  void increment() {
    sodium_increment(noncedata.data(), noncedata.size());
  }

  /**
   * Testing if a Nonce contains only zero bits in constant time
   * (for a given length).
   **/
 
  bool is_zero() {
    return sodium_is_zero(noncedata.data(), noncedata.size()) == 1;
  }

  /**
   * Compute (*this + b) mod (2 ^ (8*N)) in constant time
   * and store the result back in *this.
   *
   * The byte patterns stored in the Nonce(s) is considered to be
   * in little endian format.
   **/
  Nonce<N>& operator+= (const Nonce<N> &b) {
    sodium_add(noncedata.data(), b.noncedata.data(), noncedata.size());
    return *this;
  }

  /**
   * Return the bytes of the Nonce as hex digits.
   **/

  std::string tohex() {
    constexpr std::size_t hexbuf_size = N*2+1;
    std::vector<char> hexbuf(hexbuf_size);

    // convert [noncedata.cbegin(), noncedata.cend()) to hex:
    if (! sodium_bin2hex(hexbuf.data(), hexbuf_size,
			 noncedata.data(), N))
      throw std::runtime_error {"Sodium::Nonce<N>::tohex() overflowed"};
    
    // In C++17, we could construct a std::string with hexbuf_size chars,
    // and modify it directly through non-const data(). Unfortunately,
    // in C++11 and C++14, std::string's data() is const only, so we need
    // to copy the data over from std::vector<char> to std::string for now.

    // return hex output as a string:
    std::string outhex {hexbuf.cbegin(), hexbuf.cend()};
    return outhex;
  }
 
 private:
  data_t noncedata; // the bytes of the nonce are stored in normal memory
};

/**
 * Compare two Nonces in constant time.
 **/
template <unsigned int N>
  int compare(const Sodium::Nonce<N> &a, const Sodium::Nonce<N> &b)
{
  return sodium_compare(a.data(), b.data(), a.size());
}

template<unsigned int N>
  bool operator==(const Sodium::Nonce<N> &a, const Sodium::Nonce<N> &b)
{
  return compare<N>(a, b) == 0;
}

template<unsigned int N>
  bool operator!=(const Sodium::Nonce<N> &a, const Sodium::Nonce<N> &b)
{
  return compare<N>(a, b) != 0;
}

template<unsigned int N>
  bool operator<(const Sodium::Nonce<N> &a, const Sodium::Nonce<N> &b)
{
  return compare<N>(a, b) < 0;
}

template<unsigned int N>
  bool operator>(const Sodium::Nonce<N> &a, const Sodium::Nonce<N> &b)
{
  return compare<N>(a, b) > 0;
}

template<unsigned int N>
  bool operator <=(const Sodium::Nonce<N> &a, const Sodium::Nonce<N> &b)
{
  return compare<N>(a, b) <= 0;
}

template<unsigned int N>
  bool operator >=(const Sodium::Nonce<N> &a, const Sodium::Nonce<N> &b)
{
  return compare<N>(a, b) >= 0;
}

} // namespace Sodium

#endif // _SODIUMNONCE_H_