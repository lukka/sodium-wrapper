// keypairsign.h -- Public/Private Signing Key Pair Wrapper
//
// ISC License
// 
// Copyright (c) 2017 Farid Hajji <farid@hajji.name>
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

#ifndef _S_KEYPAIRSIGN_H_
#define _S_KEYPAIRSIGN_H_

#include <algorithm>

#include "key.h"

namespace Sodium {

class KeyPairSign
{
  /**
   * The class Sodium::KeyPairSign represents a pair of Public Key /
   * Private Key used for public key signatures in various public key
   * cryptography functions of the libsodium library.
   *
   * The public key is stored in unprotected (data_t) memory, while
   * the private key, being sensitive, is stored in protected (key_t) 
   * memory, i.e. in an internal Key object.
   *
   * A KeyPairSign provides non-mutable data()/size() access to the
   * bytes of the public/private keys in a uniform fashion via the
   * pubkey() and privkey() accessors.
   *
   * A key pair can be constructed randomly, or deterministically by
   * providing a seed. Furthermore, given a private key previously
   * generated by KeyPairSign or the underlying libsodium functions,
   * the corresponding public key and the seed can be derived and a
   * KeyPairSign constructed.
   **/

 public:
  // common constants for typical key and seed sizes
  static constexpr std::size_t KEYSIZE_PUBKEY    = Sodium::KEYSIZE_PUBKEY_SIGN;
  static constexpr std::size_t KEYSIZE_PRIVKEY   = Sodium::KEYSIZE_PRIVKEY_SIGN;
  static constexpr std::size_t KEYSIZE_SEEDBYTES = Sodium::KEYSIZE_SEEDBYTES_SIGN;

  /**
   * Generate a new (random) key pair of public/private signing keys.
   *
   * The created KeyPairSign contains a public key with KEYSIZE_PUBKEY bytes,
   * and a private key with KEYSIZE_PRIVKEY bytes. Both keys are related
   * and must be used together.
   *
   * Underlying libsodium function: crypto_sign_keypair().
   *
   * the private key is stored in an internal Key object in protected
   * key_t memory (readonly). It will be wiped clean when the KeyPairSign
   * goes out of scope or is destroyed.
   *
   * The public key is stored in an internal data_t object in
   * unprotected (readwrite) memory.
   **/
  
  KeyPairSign()
    : privkey_(false), pubkey_(KEYSIZE_PUBKEY, '\0') {
    if (crypto_sign_keypair(pubkey_.data(), privkey_.setdata()) == -1)
      throw std::runtime_error {"Sodium::KeyPairSign::KeyPairSign() crypto_sign_keypair() -1"};
    
    privkey_.readonly();
  }

  /**
   * Deterministically generate a key pair of public/private signing keys.
   *
   * The created KeyPairSign depends on a seed which must have
   * KEYSIZE_SEEDBYTES bytes. The same public/private keys will be
   * generated for the same seeds. Providing a seed of wrong size will
   * throw a std::runtime_error.
   *
   * Underlying libsodium function: crypto_sign_seed_keypair().
   *
   * Otherwise, see KeyPairSign().
   **/
  
  KeyPairSign(const data_t &seed)
    : privkey_(false), pubkey_(KEYSIZE_PUBKEY, '\0') {
    if (seed.size() != KEYSIZE_SEEDBYTES)
      throw std::runtime_error {"Sodium::KeyPairSign::KeyPairSign(seed) wrong seed size"};

    if (crypto_sign_seed_keypair(pubkey_.data(), privkey_.setdata(),
				 seed.data()) == -1)
      throw std::runtime_error {"Sodium::KeyPairSign::KeyPairSign(seed...) crypto_sign_seed_keypair() -1"};
    
    privkey_.readonly();
  }

  /**
   * Given a previously calculated private Key whose privkey_size
   * bytes are stored starting at privkey_data, derive the
   * corresponding public key pubkey, and construct with privkey_data
   * and pubkey a new KeyPairSign. privkey_data MUST point to
   * KEYSIZE_PRIVKEY bytes as shown by privkey_size, of course, or
   * this constructor will throw a std::runtime_error. The bytes at
   * privkey_data must be accessible or readable, or the program will
   * terminate.
   *
   * Underlying libsodium function: crypto_sign_ed25519_sk_to_pk()
   *
   * Note that the bytes at privkey_data MUST have been generated by
   * calculation, i.e.  by calls to KeyPairSign() constructors or
   * underlying libsodium functions. Undefined behavior results if
   * this is not the case.
   *
   * Otherwise, see KeyPairSign().
   **/
  
  KeyPairSign(const unsigned char *privkey_data,
	      const std::size_t privkey_size)
    : privkey_(false), pubkey_(KEYSIZE_PUBKEY, '\0') {
    if (privkey_size != KEYSIZE_PRIVKEY)
      throw std::runtime_error {"Sodium::KeyPairSign::KeyPairSign(privkey_data, privkey_size) wrong privkey_size"};
    std::copy(privkey_data, privkey_data+privkey_size,
	      privkey_.setdata());

    // reconstruct pubkey from privkey
    if (crypto_sign_ed25519_sk_to_pk(pubkey_.data(), privkey_.data()) == -1)
      throw std::runtime_error {"Sodium::KeyPairSign::KeyPairSign(privkey_data...) crypto_sign_ed25519_sk_to_pk -1"};

    privkey_.readonly();
  }

  /**
   * Return the seed corresponding to the private key stored in
   * this KeyPairSign.
   *
   * Underlying libsodium function: crypto_sign_ed25519_sk_to_seed()
   **/

  data_t seed() {
    data_t the_seed(KEYSIZE_SEEDBYTES);
    if (crypto_sign_ed25519_sk_to_seed(the_seed.data(),
				       privkey_.data()) == -1)
      throw std::runtime_error {"Sodium::KeyPairSign::seed() crypto_sign_ed25519_sk_to_seed() -1"};
    
    return the_seed;
  }
  
  /**
   * Give const access to the stored private key as a Key object.
   *
   * This can be used to access the bytes of the private key via a
   * non-mutable data()/size() interface like this:
   *   <SOME_KEYPAIR>.privkey().data(), <SOME_KEYPAIR>.privkey().size()
   **/

  const Key<KEYSIZE_PRIVKEY> privkey() const { return privkey_; }

  /**
   * Give const access to the stored public key as a data_t object.
   *
   * This can be used to access the bytes of the public key via a
   * non-mutable data()/size() interface like this:
   *  <SOME_KEYPAIR>.pubkey().data(), <SOME_KEYPAIR>.pubkey().size()
   **/

  const data_t pubkey() const { return pubkey_; }
  
 private:
  data_t               pubkey_;
  Key<KEYSIZE_PRIVKEY> privkey_;
};

} // namespace Sodium

extern bool operator== (const Sodium::KeyPairSign &kp1,
			const Sodium::KeyPairSign &kp2);
extern bool operator!= (const Sodium::KeyPairSign &kp1,
			const Sodium::KeyPairSign &kp2);

#endif // _S_KEYPAIRSIGN_H_
