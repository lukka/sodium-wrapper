// test_hasher_generic_keyless.cpp -- Test sodium::hasher_generic_keyless<>
//
// ISC License
//
// Copyright (c) 2018 Farid Hajji <farid@hajji.name>
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

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE sodium::hasher_generic_keyless Test
#include <boost/test/included/unit_test.hpp>

#include "hasher_generic_keyless.h"
#include <sodium.h>
#include <stdexcept>
#include <string>

using bytes = sodium::bytes;
using hasher_generic_keyless = sodium::hasher_generic_keyless<bytes>;

bool
test_hash_size(const std::string& plaintext, const std::size_t hashsize)
{
    hasher_generic_keyless hasher;

    bytes plainblob{ plaintext.cbegin(), plaintext.cend() };

    try {
        bytes outHash = hasher.hash(plainblob, hashsize);

        return outHash.size() == hashsize;
    } catch (std::exception& /* e */) {
        // test failed for some reason
        return false;
    }

    // NOTREACHED
    return false;
}

bool
test_keyless_hashing(const std::string& plaintext)
{
    hasher_generic_keyless hasher;

    bytes plainblob{ plaintext.cbegin(), plaintext.cend() };

    try {
        bytes outHash1 = hasher.hash(
          plainblob /* , hasher_generic_keyless::HASHSIZE */); // keyless
        bytes outHash2(hasher_generic_keyless::HASHSIZE);
        hasher.hash(plainblob, outHash2); // keyless

        return outHash1 == outHash2;
    } catch (std::exception& /* e */) {
        // test failed for some reason
        return false;
    }

    // NOTREACHED
    return false;
}

struct SodiumFixture
{
    SodiumFixture()
    {
        BOOST_REQUIRE(sodium_init() != -1);
        // BOOST_TEST_MESSAGE("SodiumFixture(): sodium_init() successful.");
    }
    ~SodiumFixture()
    {
        // BOOST_TEST_MESSAGE("~SodiumFixture(): teardown -- no-op.");
    }
};

BOOST_FIXTURE_TEST_SUITE(sodium_test_suite, SodiumFixture)

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_default_hash_size)
{
    std::string plaintext{ "the quick brown fox jumps over the lazy dog" };

    BOOST_CHECK(test_hash_size(plaintext, hasher_generic_keyless::HASHSIZE));
}

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_min_hash_size)
{
    std::string plaintext{ "the quick brown fox jumps over the lazy dog" };

    BOOST_CHECK(
      test_hash_size(plaintext, hasher_generic_keyless::HASHSIZE_MIN));
}

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_max_hash_size)
{
    std::string plaintext{ "the quick brown fox jumps over the lazy dog" };

    BOOST_CHECK(
      test_hash_size(plaintext, hasher_generic_keyless::HASHSIZE_MAX));
}

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_hash_size_too_small)
{
    std::string plaintext{ "the quick brown fox jumps over the lazy dog" };

    BOOST_CHECK(
      !test_hash_size(plaintext, hasher_generic_keyless::HASHSIZE_MIN - 1));
}

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_hash_size_too_big)
{
    std::string plaintext{ "the quick brown fox jumps over the lazy dog" };

    BOOST_CHECK(
      !test_hash_size(plaintext, hasher_generic_keyless::HASHSIZE_MAX + 1));
}

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_falsify_plaintext)
{
    hasher_generic_keyless hasher;

    std::string plaintext{ "the quick brown fox jumps over the lazy dog" };
    hasher_generic_keyless::bytes_type plainblob{ plaintext.cbegin(),
                                                  plaintext.cend() };
    hasher_generic_keyless::bytes_type falsified{ plainblob };
    ++falsified[0];

    bytes hash1 =
      hasher.hash(plainblob /* , hasher_generic_keyless::HASHSIZE */);
    bytes hash2 =
      hasher.hash(falsified /* , hasher_generic_keyless::HASHSIZE */);

    // unless there is a collision (very unlikely),
    // both hashes must be different for test to succeed
    BOOST_CHECK(hash1 != hash2);
}

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_keyless_full_plaintext)
{
    std::string plaintext{ "the quick brown fox jumps over the lazy dog" };

    BOOST_CHECK(test_keyless_hashing(plaintext));
}

BOOST_AUTO_TEST_CASE(sodium_hasher_generic_keyless_test_keyless_empty_plaintext)
{
    std::string plaintext{};

    BOOST_CHECK(test_keyless_hashing(plaintext));
}

BOOST_AUTO_TEST_SUITE_END()
