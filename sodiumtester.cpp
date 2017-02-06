// sodiumtester.cpp -- Test functions for the test harness SodiumTester
//
// Copyright (C) 2017 Farid Hajji <farid@hajji.name>. All rights reserved.

#include "sodiumtester.h"

#include "sodiumnonce.h"
#include "sodiumkey.h"
#include "sodiumcrypter.h"
#include "sodiumauth.h"

#include <stdexcept>
#include <string>
#include <sstream>

/**
 * Construct the test harness by calling sodium_init() which initializes
 * the libsodium library.
 **/

SodiumTester::SodiumTester()
{
  // We need to initialize libsodium by calling sodium_init() at least
  // once before calling other functions of this library.
  // Calling sodium_init() multiple times doesn't hurt (it may happen
  // e.g. in custom allocators etc.).
  
  if (sodium_init() == -1)
    throw std::runtime_error {"sodium_init() failed"};
}

/**
 * Encrypt a plaintext string with a randomly generated key and nonce
 * and return the result as a string in hexadecimal representation.
 *
 * - We use Sodium::Key wrapper to create and store a random key
 * - We use Sodium::Nonce wrapper to create a store a random nonce
 * - We store the plaintext/ciphertext in a data_t, in unprotected memory
 * - We use our wrapper Sodium::Crypter to do the encryption
 * - We use our wrapper Sodium::Crypter to test-decrypt the result
 *   and verify that the decrypted text is the same as the plaintext.
 * - We use our wrapper Sodium::Crypter to convert the ciphertext into
 *   hexadecimal string, which we return.
 **/

std::string
SodiumTester::test0(const std::string &plaintext)
{  
  using data_t = Sodium::Crypter::data_t; // unprotected memory
  
  Sodium::Crypter sc {}; // encryptor, decryptor, hexifior.
  Sodium::Key     key(Sodium::Key::KEYSIZE_SECRETBOX); // create random key
  Sodium::Nonce<> nonce {};                            // create random nonce;
  
  // transfer plaintext into a binary blob
  data_t plainblob {plaintext.cbegin(), plaintext.cend()};
  
  // encrypt the plaintext (binary blob) using key/nonce:
  data_t ciphertext = sc.encrypt(plainblob,  key, nonce);

  // (test-) decrypt the ciphertext using same key/nonce:
  data_t decrypted  = sc.decrypt(ciphertext, key, nonce);

  // we're done with the key for now, disable memory access to it!
  // we could re-enable it later with readonly() or readwrite() though...
  key.noaccess();
  
  // test of correctness (sanity check): the ciphertext must be
  // equal to the plaintext.
  // 
  // Note that Sodium::Crypter::decrypt() will also have performed
  // a check and thrown a std::runtime_error, should the decryption
  // fail. It can detect corruption of the ciphertext, because
  // Sodium::Crypter::encrypt() encrypts both the plaintext and a MAC
  // that was generated out of the plaintext and of the key/nonce before.
  //
  // We're just double-checking here.

  if (plainblob != decrypted)
    throw std::runtime_error {"test0() message forged (own test)"};

  // finally, convert the bytes of the ciphertext into a hexadecimal
  // string that can be printed, and return that string.

  std::string encrypted_as_hex = sc.tohex(ciphertext);
  return encrypted_as_hex;

  // the key will self-destruct here when it goes out of scope.
}

/**
 * Compute the MAC of a plaintext string using a randomly created
 * key. Then:
 *   - verify the unchanged plaintext with the same key/mac
 *   - verify the changed plaintext with the same key/mac
 *   - verify the unchanged plaintext with a different key/mac
 **/

bool
SodiumTester::test1(const std::string &plaintext)
{
  using data_t = Sodium::Auth::data_t; // unprotected memory
  
  Sodium::Auth sa {}; // Secret Key Authenticator/Verifier
  Sodium::Key  key(Sodium::Key::KEYSIZE_AUTH); // Create a random key
  
  // transfer plaintext into a binary blob
  data_t plainblob {plaintext.cbegin(), plaintext.cend()};
  
  // compute the MAC:
  data_t mac { sa.auth(plainblob, key) };

  // verify the MAC with unchanged data
  if (! sa.verify(plainblob, mac, key))
    throw std::runtime_error {"SodiumTester::test1() identical MAC failed"};

  // 2. change plaintext, and re-verify MAC:
  if (plainblob.size() > 0 &&
      (plainblob[0] = static_cast<unsigned char>('!')) &&
      sa.verify(plainblob, mac, key))
    throw std::runtime_error {"SodiumTester::test1() different MAC verify"};

  // 3. restore plaintext, then change key and reverify MAC
  plainblob.assign(plaintext.cbegin(), plaintext.cend());
  key.readwrite();
  key.initialize();
  key.readonly();
  if (sa.verify(plainblob, mac, key))
    throw std::runtime_error {"SodiumTester::test1() different KEYS verify"};

  // not strictly necessary, because we're about to destroy key soon
  key.noaccess();

  return true;
}

/**
 * This function tests the key derivation algorithm of libsodium.
 *
 *   - derive a key from pw1, and encrypt plaintext with it.
 *   - derive a new key from pw2, store it in old key location.
 *   - attempt to decrypt encrypted text with it.
 *   - if decryption succeeded (didn't throw), check again manually
 *     that both plaintext and decrypted text are identical.
 *
 * In all cases the same random Nonce is (re-)used for encryption
 * and decryption, of course.
 **/

bool
SodiumTester::test2(const std::string &plaintext,
		    const std::string &pw1,
		    const std::string &pw2)
{
  using data_t = Sodium::Crypter::data_t; // unprotected memory
  using key_t  = Sodium::Key::key_t;      // protected memory
  
  Sodium::Crypter sc {}; // encryptor, decryptor, hexifior.
  Sodium::Key     key(Sodium::Key::KEYSIZE_SECRETBOX,
		      false); // uninitialized, read-write for now
  Sodium::Nonce<> nonce {};

  // random salt, needed by the key derivation function.
  // NOTE: can't move this into Sodium::Key::setpass(),
  // because we need the salt AND the password to be able
  // to deterministically recreate a key. If we generated
  // the salt in setpass() randomly, users would have no
  // way to recreate the key -- that would be throw-away
  // use-once keys.
  data_t salt(Sodium::Key::KEYSIZE_SALT);
  randombytes_buf(salt.data(), salt.size());

  // transfer plaintext into a binary blob
  data_t plainblob {plaintext.cbegin(), plaintext.cend()};

  // try the first key
  key.setpass(pw1, salt, Sodium::Key::strength_t::medium);
  
  // now encrypt with that key
  data_t ciphertext = sc.encrypt(plainblob, key, nonce);

  // try the second key
  key.setpass(pw2, salt, Sodium::Key::strength_t::medium);
  
  // now decrypt with that new key.
  // if the key/password was different, we will throw right here and now
  data_t decrypted = sc.decrypt(ciphertext, key, nonce);

  return (decrypted == plainblob);
}

/**
 * This function tests Sodium::Nonce(s).
 *
 *   - We create a Nonce<> 'a' with a random value.
 *   - We check that 'a' is indeed 24 bytes (Sodium::NONCESIZE_SECRETBOX) long
 *   - We display a hex representation of 'a'
 *   - We copy 'a' into 'a_copy' using the compiler-generated copy constructor
 *   - We test with operator != in constant time if they are different
 *     and throw if they are. We don't measure times here.
 *   - We increment 'a' 5 times, i.e. in pseudo-code: a = a+5.
 *     That is: we call a.increment() 5 times in a row, and we display
 *     each time the hex value of 'a'.  Notice how the FIRST byte changes,
 *     showing indeed that the Nonce bytes are interpreted indeed as an
 *     integer in Little Endian format.
 *   - We test with operator > if 'a_copy' is greater than 'a' and throw
 *     if yes. Indeed, a_copy is the original Nonce value, and 'a' has been
 *     incremented 5 times already. So 'a_copy' shouldn't be greater than 'a'
 *     The test is in constant time... but we don't measure that here.
 *   - We create a new Nonce 'b', but uninitialized. When a nonce is
 *     uninitialized, its backend is default-initialized, i.e. all those
 *     unsigned char(s) of its std::vector are zeroes.
 *   - We check this by:
 *        + displaying a hex representation of 'b'
 *        + checking in constant time if 'b' is all-zeroes with b.is_zero()
 *          and throw if not.
 *   - We increment 'b' by calling b.increment() 5 times in a row. 'b' is
 *     therefore equivalent to '5'. We display hex representation of 'b'
 *   - We exercise operator += by adding 'b' to 'a_copy': i.e. by calling
 *       a_copy += b;
 *     Unless there was an overflow, we should get 'a_copy' == 'a', since
 *     'a' was also incremented 5 times in a row previously.  We check this
 *     with operator != and throw if not equal.
 *
 * We generate the output of the tests as a std::string incrementally
 * by writing the results in a std::ostringstream, and return the string
 * at the end to be displayed by the caller of this function.
 **/

std::string
SodiumTester::test3()
{
  std::ostringstream os; // to collect output
  os << "starting Nonce test... -------" << std::endl;

  Sodium::Nonce<> a {}; // a random nonce
  
  // Check at compile time that we got the default size of the Nonce:
  static_assert(a.size() == Sodium::NONCESIZE_SECRETBOX,
		"SodiumTester::test3() wrong nonce size");
  // if (a.size() != Sodium::NONCESIZE_SECRETBOX)
  //   throw std::runtime_error {"SodiumTester::test3() wrong nonce size"};

  os << "a+0: " << a.tohex() << std::endl;
  
  Sodium::Nonce<> a_copy {a};
  if (a != a_copy)
    throw std::runtime_error {"SodiumTester::test3() a != a_copy"};
  
  for (int i: {1,2,3,4,5}) {
    a.increment();
    os << "a+" << i << ": " << a.tohex() << std::endl;
  }

  if (a_copy > a)
    throw std::runtime_error {"SodiumTester::test3() a+5 > a"};
  
  Sodium::Nonce<> b(false); // uninitialized, zeroed?
  os << "b+0: " << b.tohex() << std::endl;
  if (! b.is_zero())
    throw std::runtime_error {"SodiumTester::test3() not initialized to zero"};

  for (int i: {1,2,3,4,5})
    b.increment();
  // b is now 5, display it!
  os << "b+5: " << b.tohex() << std::endl;

  a_copy += b; // increment original a by 5 (should be new a)
  if (a_copy != a)
    throw std::runtime_error {"SodiumTester::test3() a_copy + 5 != a+5"};

  os << "---------------- ending Nonce test..." << std::endl;
  return os.str();
}
