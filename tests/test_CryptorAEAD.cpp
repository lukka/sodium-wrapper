// test_CryptorAEAD.cpp -- Test Sodium::CryptorAEAD
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

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Sodium::CryptorAEAD Test
#include <boost/test/included/unit_test.hpp>

#include <sodium.h>
#include "cryptoraead.h"
#include "key.h"
#include "nonce.h"
#include <string>

using data_t = Sodium::data_t;

bool
test_of_correctness(const std::string &header,
		    const std::string &plaintext,
		    std::size_t &ciphertext_size,
		    bool falsify_header = false,
		    bool falsify_ciphertext = false)
{
  Sodium::CryptorAEAD                   sc {};
  Sodium::Key                           key(Sodium::Key::KEYSIZE_AEAD);
  Sodium::Nonce<Sodium::NONCESIZE_AEAD> nonce {};

  data_t plainblob    {plaintext.cbegin(), plaintext.cend()};
  data_t headerblob   {header.cbegin(), header.cend()};

  data_t ciphertext = sc.encrypt(headerblob, plainblob, key, nonce);

  if (falsify_ciphertext && ciphertext.size() != 0)
    ++ciphertext[0];

  ciphertext_size = ciphertext.size();
  
  data_t decrypted;

  // falsify the header AFTER encryption!
  if (falsify_header && headerblob.size() != 0)
    ++headerblob[0];
  
  try {
    decrypted  = sc.decrypt(headerblob, ciphertext, key, nonce);
  }
  catch (std::exception &e) {
    return false; // decryption failed;
  }

  key.noaccess();

  return plainblob == decrypted;
}

struct SodiumFixture {
  SodiumFixture()  {
    BOOST_REQUIRE(sodium_init() != -1);
    BOOST_TEST_MESSAGE("SodiumFixture(): sodium_init() successful.");
  }
  ~SodiumFixture() {
    BOOST_TEST_MESSAGE("~SodiumFixture(): teardown -- no-op.");
  }
};

/**
 * The previous fixture is RAII called _for each_ test case
 * individually; i.e. sodium_init() is initialized multiple times.
 *
 * ------ FIXME: THIS ADVICE DUMPS CORE !!! ---------------------------
 * If you prefer to to this fixture only once for the whole test
 * suite, replace BOOST_FIXTURE_TEST_SUITE (...) by call call to
 * BOOST_AUTO_TEST_SUITE (sodium_test_suite,
 *                        * boost::unit_test::fixture<SodiumFixture>())
 * i.e. using decorators.
 * ------ FIXME: THIS ADVICE DUMPS CORE !!! ---------------------------
 * 
 * To see the output of the messages, invoke with --log_level=message.
 **/

BOOST_FIXTURE_TEST_SUITE ( sodium_test_suite, SodiumFixture );

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_full_plaintext_full_header )
{
  std::string header    {"the head"};
  std::string plaintext {"the quick brown fox jumps over the lazy dog"};
  std::size_t csize;

  BOOST_CHECK(test_of_correctness(header, plaintext, csize, false, false));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_full_plaintext_empty_header )
{
  std::string header    {};
  std::string plaintext {"the quick brown fox jumps over the lazy dog"};
  std::size_t csize;

  BOOST_CHECK(test_of_correctness(header, plaintext, csize, false, false));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_empty_plaintext_full_header )
{
  std::string header    {"the head"};
  std::string plaintext {};
  std::size_t csize;

  BOOST_CHECK(test_of_correctness(header, plaintext, csize, false, false));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_empty_plaintext_empty_header )
{
  std::string header    {};
  std::string plaintext {};
  std::size_t csize;

  BOOST_CHECK(test_of_correctness(header, plaintext, csize, false, false));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_empty_plaintext_falsify_header )
{
  std::string header    {"the head"};
  std::string plaintext {};
  std::size_t csize;

  BOOST_CHECK(! test_of_correctness(header, plaintext, csize, true, false));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_full_plaintext_falsify_header )
{
  std::string header    {"the head"};
  std::string plaintext {"the quick brown fox jumps over the lazy dog"};
  std::size_t csize;

  BOOST_CHECK(! test_of_correctness(header, plaintext, csize, true, false));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_falsify_plaintext_empty_header )
{
  std::string header    {};
  std::string plaintext {"the quick brown fox jumps over the lazy dog"};
  std::size_t csize;

  BOOST_CHECK(! test_of_correctness(header, plaintext, csize, false, true));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_falsify_plaintext_full_header )
{
  std::string header    {"the head"};
  std::string plaintext {"the quick brown fox jumps over the lazy dog"};
  std::size_t csize;

  BOOST_CHECK(! test_of_correctness(header, plaintext, csize, false, true));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_falsify_plaintext_falsify_header )
{
  std::string header    {"the head"};
  std::string plaintext {"the quick brown fox jumps over the lazy dog"};
  std::size_t csize;

  BOOST_CHECK(! test_of_correctness(header, plaintext, csize, true, true));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);
}

BOOST_AUTO_TEST_CASE( sodium_cryptorAEAD_test_big_header )
{
  std::string header(Sodium::CryptorAEAD::MACSIZE * 200, 'A');
  std::string plaintext {"the quick brown fox jumps over the lazy dog"};
  std::size_t csize;

  // The following test shows that the header is NOT included in
  // the ciphertext. Only the plaintext and the MAC are included
  // in the ciphertext, no matter how big the header may be.
  // It is the responsability of the user to transmit the header
  // separately from the ciphertext, i.e. to tag it along.
  
  BOOST_CHECK_EQUAL(header.size(), Sodium::CryptorAEAD::MACSIZE * 200);
  BOOST_CHECK(test_of_correctness(header, plaintext, csize, false, false));
  BOOST_CHECK_EQUAL(csize, plaintext.size() + Sodium::CryptorAEAD::MACSIZE);

  // However, a modification of the header WILL be detected.
  // We modify only the 0-th byte right now, but a modification
  // SHOULD also be detected past MACSIZE bytes... (not tested)
  
  BOOST_CHECK(! test_of_correctness(header, plaintext, csize, true, false));
}

BOOST_AUTO_TEST_SUITE_END ();