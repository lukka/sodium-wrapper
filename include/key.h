// key.h -- Sodium Key Wrapper
//
// Copyright (C) 2017 Farid Hajji <farid@hajji.name>. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _S_KEY_H_
#define _S_KEY_H_

#include <vector>
#include <string>
#include <algorithm>
#include "common.h"
#include "alloc.h"

namespace Sodium {

class Key
{
  /**
   * The class Sodium::Key represents a Key used in various functions
   * of the libsodium library.  Key material, being particulary sensitive,
   * is stored in "protected memory" using a special allocator.
   *
   * A Key can be
   *   - default-constructed using random data,
   *   - default-constructed but left uninitialized
   *   - derived from a password string and a (hopefully random) salt.
   *
   * A Key can be made read-only or non-accessible when no longer
   * needed.  In general, it is a good idea to be as restrictive as
   * possible with key material.
   *
   * When a Key goes out of scope, it auto-destructs by zeroing its
   * memory, and eventually releasing the virtual pages too.
   **/

 public:

  // Some common constants for typical key sizes from <sodium.h>
  static constexpr std::size_t KEYSIZE_SECRETBOX   = crypto_secretbox_KEYBYTES;
  static constexpr std::size_t KEYSIZE_AUTH        = crypto_auth_KEYBYTES;
  static constexpr std::size_t KEYSIZE_SALT        = crypto_pwhash_SALTBYTES;
  static constexpr std::size_t KEYSIZE_AEAD        = crypto_aead_chacha20poly1305_KEYBYTES;
  static constexpr std::size_t KEYSIZE_HASHKEY     = crypto_generichash_KEYBYTES;
  static constexpr std::size_t KEYSIZE_HASHKEY_MIN = crypto_generichash_KEYBYTES_MIN;
  static constexpr std::size_t KEYSIZE_HASHKEY_MAX = crypto_generichash_KEYBYTES_MAX;
  static constexpr std::size_t KEYSIZE_PUBKEY      = crypto_box_PUBLICKEYBYTES;
  static constexpr std::size_t KEYSIZE_PRIVKEY     = crypto_box_SECRETKEYBYTES;

  // for keypair(), size of optional data_t seed blob
  static constexpr std::size_t KEYSIZE_SEEDBYTES   = crypto_box_SEEDBYTES;
  
  /**
   * key_t is protected memory for bytes of key material (see: sodiumkey.h)
   *   * key_t memory will self-destruct/zero when out-of-scope / throws
   *   * key_t memory can be made readonly or temporarily non-accessible
   *   * key_t memory is stored in virtual pages protected by canary,
   *     guard pages, and access to those pages is granted with mprotect().
   **/
  
  using key_t      = std::vector<unsigned char, SodiumAlloc<unsigned char>>;
  
  // The strengh of the key derivation efforts for setpass()
  using strength_t = enum class Strength { low, medium, high };

  // The class KeyPair can access keydata bytes directly
  friend class KeyPair;
  
  /**
   * Construct a Key of size key_size.
   *
   * If bool is true, initialize the key, i.e. fill it with random data
   * generated by initialize(), and then make it readonly().
   *
   * If bool is false, leave the key uninitialized, i.e. in the state
   * as created by the special allocator for protected memory. Leave
   * the key in the readwrite() default for further setpass()...
   **/
  
  Key(std::size_t key_size, bool init=true) : keydata(key_size) {
    if (init) {
      initialize();
      readonly();
    }
    // CAREFUL: read/write uninitialized key
  }

  // Keys can be copied (for now), but be careful
  Key(const Key &other)             = default;
  Key& operator= (const Key &other) = default;

  /**
   * Various libsodium functions used either directly or in
   * the wrappers need access to the bytes stored in the key.
   *
   * data() gives const access to those bytes of which
   * size() bytes are stored in the key.
   *
   * We don't provide mutable access to the bytes by design
   * with this data()/size() interface.
   * 
   * The only functions that change those bytes are:
   *   initialize(), destroy(), setpass().
   *
   * Furthermore, the class KeyPair is a friend of Key, so it
   * can also modify the bytes of a Key.
   **/
  
  const unsigned char *data() const { return keydata.data(); }
  const std::size_t    size() const { return keydata.size(); }
  
  /**
   * Derive key material from the string password, and the salt
   * (where salt.size() == KEYSIZE_SALT) and store that key material
   * into this key object's protected readonly() memory.
   * 
   * The strength parameter determines how much effort is to be
   * put into the derivation of the key. It can be one of
   *    Sodium::Key::strength_t::{low,medium,high}.
   *
   * This function throws a std::runtime_error if the strength parameter
   * or the salt size don't make sense, or if the underlying libsodium
   * derivation function crypto_pwhash() runs out of memory.
   **/

  void setpass (const std::string &password,
		const data_t &salt,
		const strength_t strength = strength_t::high) {
    // check strength and set appropriate parameters
    unsigned long long strength_mem;
    unsigned long long strength_cpu;
    switch (strength) {
    case strength_t::low:
      strength_mem = crypto_pwhash_MEMLIMIT_INTERACTIVE;
      strength_cpu = crypto_pwhash_OPSLIMIT_INTERACTIVE;
      break;
    case strength_t::medium:
      strength_mem = crypto_pwhash_MEMLIMIT_MODERATE;
      strength_cpu = crypto_pwhash_OPSLIMIT_MODERATE;
      break;
    case strength_t::high:
      strength_mem = crypto_pwhash_MEMLIMIT_SENSITIVE;
      strength_cpu = crypto_pwhash_OPSLIMIT_SENSITIVE;
      break;
    default:
      throw std::runtime_error {"Sodium:::Key::setpass() wrong strength"};
    }

    // check salt length
    if (salt.size() != KEYSIZE_SALT)
      throw std::runtime_error {"Sodium::Key::setpass() wrong salt size"};

    // derive a key from the hash of the password, and store it!
    readwrite(); // temporarily unlock the key (if not already)
    if (crypto_pwhash (keydata.data(), keydata.size(),
		       password.data(), password.size(),
		       salt.data(),
		       strength_cpu,
		       strength_mem,
		       crypto_pwhash_ALG_DEFAULT) != 0)
      throw std::runtime_error {"Sodium::Key::setpass() crypto_pwhash()"};
    readonly(); // relock the key
  }

  /**
   * Initialize, i.e. fill with random data generated with libsodium's
   * function randombytes_buf() the number of bytes already allocated
   * to this Key upon construction.
   *
   * You normally don't need to call this function yourself, as it is
   * called by Key's constructor. It is provided as a public function
   * nonetheless, should you need to rescramble the key, keeping its
   * size (a rare case).
   *
   * This function will terminate the program if the Key is readonly()
   * or noaccess() on systems that enforce mprotect().
   **/
  
  void initialize() {
    randombytes_buf(keydata.data(), keydata.size());
  }

  /**
   * Destroy the bytes stored in protected memory of this key by
   * attempting to zeroing them.
   *
   * A Key that has been destroy()ed still holds size() zero-bytes in
   * protected memory, and can thus be reused, i.e. reset by calling
   * e.g. setpass().
   *
   * The key will be destroyed, even if it has been set readonly()
   * or noaccess() previously.
   * 
   * You normally don't need to explicitely zero a Key, because Keys
   * self-destruct (including zeroing their bytes) when they go out
   * of scope. This function is provided in case you need to immediately
   * erase a Key anyway (think: Panic Button).
   **/
  
  void destroy() {
    readwrite();
    sodium_memzero(keydata.data(), keydata.size());
  }

  /**
   * Mark this Key as non-accessible. All attempts to read or write
   * to this key will be caught by the CPU / operating system and
   * will result in abnormal program termination.
   * 
   * The protection mechanism works by mprotect()ing the virtual page
   * containing the key bytes accordingly.
   *
   * Note that the key bytes are still available, even when noaccess()
   * has been called. Restore access by calling readonly() or readwrite().
   **/
  
  void noaccess()  { keydata.get_allocator().noaccess(keydata.data()); }

  /**
   * Mark this Key as read-only. All attemps to write to this key will
   * be caught by the CPU / operating system and will result in abnormal
   * program termination.
   *
   * The protection mechanism works by mprotect()ing the virtual page
   * containing the key bytes accordingly.
   *
   * Note that the key bytes can be made writable by calling readwrite().
   **/
  
  void readonly()  { keydata.get_allocator().readonly(keydata.data()); }

  /**
   * Mark this Key as read/writable. Useful after it has been previously
   * marked readonly() or noaccess().
   **/

  void readwrite() { keydata.get_allocator().readwrite(keydata.data()); }

 private:
  key_t keydata; // the bytes of the key are stored in protected memory
};
 
} // namespace Sodium

extern bool operator== (const Sodium::Key &k1, const Sodium::Key &k2);
extern bool operator!= (const Sodium::Key &k1, const Sodium::Key &k2);

#endif // _S_KEY_H_
