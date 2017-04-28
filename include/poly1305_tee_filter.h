// poly1305_tee_filter.h -- Boost.Iostreams tee filter for Poly1305
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

#ifndef _S_POLY1305_FILTER_H_
#define _S_POLY1305_FILTER_H_

#if defined(_MSC_VER)
# pragma once
#endif

#include <boost/assert.hpp>
#include <boost/config.hpp>  // BOOST_DEDUCE_TYPENAME.
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/detail/adapter/device_adapter.hpp>
#include <boost/iostreams/detail/adapter/filter_adapter.hpp>
#include <boost/iostreams/detail/call_traits.hpp>
#include <boost/iostreams/detail/execute.hpp>
#include <boost/iostreams/detail/functional.hpp>  // call_close_all 
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/pipeline.hpp>
#include <boost/iostreams/traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/type_traits/is_same.hpp>

#include "key.h"

#include <stdexcept>     // std::runtime_error
#include <sodium.h>

#define NDEBUG
// #undef NDEBUG

#ifndef NDEBUG
#include <iostream>
#include <string>
#endif // ! NDEBUG

using namespace boost::iostreams;

namespace Sodium {

/**
 * poly1305_tee_filter<Device>
 * 
 * XXX: TODO -- Add documentation
 **/
  
template <typename Device>
class poly1305_tee_filter : public detail::filter_adapter<Device>
{
  /**
   * poly1305_tee_filter is like an boost::iostreams::tee_filter that
   * passes all input unmodified through first Sink, and at the same
   * times computes a Poly1305 checksum which it passes to the second
   * Sink when the input stream is about to be closed.
   * 
   * Use as a pipeable filter when both sinks have the same type
   * Device.
   **/

 public:

  typedef typename detail::param_type<Device>::type param_type;
  typedef typename char_type_of<Device>::type       char_type;
  struct category
    : dual_use_filter_tag,
    multichar_tag,
    closable_tag,
    flushable_tag,
    localizable_tag,
    optimally_buffered_tag
      { };

  BOOST_STATIC_ASSERT(is_device<Device>::value);
  BOOST_STATIC_ASSERT((
    boost::is_convertible< // Using mode_of causes failures on VC6-7.0.
                          BOOST_DEDUCED_TYPENAME boost::iostreams::category_of<Device>::type,
                          output
                         >::value
  ));

  static constexpr std::size_t KEYSIZE = Sodium::KEYSIZE_POLY1305;
  static constexpr std::size_t MACSIZE = crypto_onetimeauth_BYTES;
  
  using key_type = Key<KEYSIZE>;
  using mac_type = data2_t; // of MACSIZE elements...
  
  /**
   * Construct a poly1305_tee_filter which passes all data through
   * from input stream to the first sink, and computes at the same time
   * a Poly1305 MAC using the provided key. This MAC will be sent to
   * the second sink when the input stream is about to be closed.
   **/
  
  explicit poly1305_tee_filter(param_type dev, const key_type key) 
    : detail::filter_adapter<Device>(dev), key_ {key}
  {
    // initialize the Poly1305 state machine
    crypto_onetimeauth_init(&state_, key_.data());

#ifndef NDEBUG
    std::cerr << "Sodium::poly1305_tee_filter::poly1305_tee_filter() called"
	      << std::endl;
#endif // ! NDEBUG
  }

  template<typename Source>
  std::streamsize read(Source& src, char_type* s, std::streamsize n)
  {

    /**
     * NOTE: This function is not called for some reason! (XXX)
     * If yes, throw an std::runtime_exception (for now).
     * Need to better understand when read() is invoked!
     **/
    
    throw std::runtime_error {"Sodium::poly1305_tee_filter::read() called. FIXME!"};
    
    // Read (up to) n chars from src into buffer s
    // and update the Poly1305 state machine for this chunk
    // using the crypto_onetimeauth_*() streaming API update function:

    std::streamsize result = boost::iostreams::read(src, s, n);

#ifndef NDEBUG
    std::cerr << "WARNING !!! Sodium::poly1305_tee_filter::read() called "
	      << "[n=" << n << "] "
	      << '\n';
    if (result != -1) {
      std::string s_as_string {s, s+result};
      std::cerr << "  [s=" << s_as_string << "]"
		<< std::endl;
    }
#endif // ! NDEBUG

    // if (result != -1) {
    //   crypto_onetimeauth_update(&state_,
    // 				reinterpret_cast<unsigned char *>(s),
    // 				result);
    // 
    //  // Don't send anything to the second sink yet, since we're
    //   // not done yet computing the Poly1305 MAC.
    // 
    //   // std::streamsize result2 = iostreams::write(this->component(), s, result);
    //   // (void) result2; // Suppress 'unused variable' warning.
    //   // BOOST_ASSERT(result == result2);
    // }
    
    return result; // nr. of bytes read.
  }

  template<typename Sink>
  std::streamsize write(Sink& snk, const char_type* s, std::streamsize n)
  {
    // Write (up to) n chars from the buffer s into the first sink snk.
    // We pass the data unchanged:
    
    std::streamsize result = boost::iostreams::write(snk, s, n);

#ifndef NDEBUG
    std::string s_as_string {s, s+result};
    std::cerr << "Sodium::poly1305_tee_filter::write() called "
	      << "[n=" << n << "] "
	      << "[s=" << s_as_string << "] "
	      << "[result=" << result << "]"
	      << std::endl;
#endif // ! NDEBUG
    
    // Update the Poly1305 state with the chunk we've got:
    crypto_onetimeauth_update(&state_,
			      reinterpret_cast<const unsigned char *>(s),
			      result);

    // Don't write anything yet to the second sink, because we're not
    // done yet computing the Poly1305 MAC:
    
    // std::streamsize result2 = iostreams::write(this->component(), s, result);
    // (void) result2; // Suppress 'unused variable' warning.
    //  BOOST_ASSERT(result == result2);

    return result; // nr. of bytes (not really) written.
  }

  template<typename Next>
  void close(Next&, BOOST_IOS::openmode)
  {
    // before closing, send the computed Poly1305 MAC:
    char_type out[MACSIZE];
    crypto_onetimeauth_final(&state_,
			     reinterpret_cast<unsigned char *>(out));

    std::streamsize result =
      boost::iostreams::write(this->component(),
			      out,
			      MACSIZE);

#ifndef NDEBUG
    mac_type    out_as_mac_type_var {out, out+MACSIZE};
    std::string out_as_string       {tohex(out_as_mac_type_var)};
    std::cerr << "Sodium::poly1305_tee_filter::close() called "
	      << "[result=" << result << "], "
	      << "[MACSIZE=" << MACSIZE << "]" << '\n'
	      << "  [out=" << out_as_string << "]"
	      << std::endl;
#endif // ! NDEBUG
    
    BOOST_ASSERT(result = MACSIZE);
      
    // and now close the streams
    detail::close_all(this->component());

    // reset Poly1305 state so we can start afresh with new streams:
    crypto_onetimeauth_init(&state_, key_.data());
  }

  template<typename Sink>
  bool flush(Sink& snk)
  {
    bool r1 = boost::iostreams::flush(snk);
    bool r2 = boost::iostreams::flush(this->component()); // actually a NO-OP

#ifndef NDEBUG
    std::cerr << "Sodium::poly1305_tee_filter::flush() called "
	      << "[r1=" << r1 << ",r2=" << r2 << "]"
	      << std::endl;
#endif // ! NDEBUG
    
    return r1 && r2;
  }

 private:
  key_type key_;
  crypto_onetimeauth_state state_;
};

BOOST_IOSTREAMS_PIPABLE(poly1305_tee_filter, 1)

template<typename Sink>
poly1305_tee_filter<Sink> poly1305_tee(Sink& snk,
				       const typename poly1305_tee_filter<Sink>::key_type key) 
{ return poly1305_tee_filter<Sink>(snk, key); }

template<typename Sink>
poly1305_tee_filter<Sink> poly1305_tee(const Sink& snk,
				       const typename poly1305_tee_filter<Sink>::key_type key) 
{ return poly1305_tee_filter<Sink>(snk, key); }

/**
 * poly1305_tee_device<Device, Sink>
 * 
 * XXX: TODO -- add documentation
 **/

template<typename Device, typename Sink>
class poly1305_tee_device {
public:
  typedef typename detail::param_type<Device>::type  device_param;
  typedef typename detail::param_type<Sink>::type    sink_param;
  typedef typename detail::value_type<Device>::type  device_value;
  typedef typename detail::value_type<Sink>::type    sink_value;
  typedef typename char_type_of<Device>::type        char_type;
  typedef typename
    boost::mpl::if_<
      boost::is_convertible<
                            BOOST_DEDUCED_TYPENAME 
                            boost::iostreams::category_of<Device>::type, 
                            output
                           >,
      output,
      input
    >::type                                    mode;

  BOOST_STATIC_ASSERT(is_device<Device>::value);
  BOOST_STATIC_ASSERT(is_device<Sink>::value);
  BOOST_STATIC_ASSERT((
    boost::is_same<
                   char_type, 
                   BOOST_DEDUCED_TYPENAME char_type_of<Sink>::type
                  >::value
  ));
  BOOST_STATIC_ASSERT((
    boost::is_convertible<
                          BOOST_DEDUCED_TYPENAME boost::iostreams::category_of<Sink>::type, 
                          output
                         >::value
  ));
  struct category
    : mode,
    device_tag,
    closable_tag,
    flushable_tag,
    localizable_tag,
    optimally_buffered_tag
      { };


  static constexpr std::size_t KEYSIZE = Sodium::KEYSIZE_POLY1305;
  static constexpr std::size_t MACSIZE = crypto_onetimeauth_BYTES;
  
  using key_type = Key<KEYSIZE>;
  using mac_type = data2_t; // of size MACSIZE
  
  poly1305_tee_device(device_param device, sink_param sink,
		      const key_type key)
    : dev_(device), sink_(sink), key_ {key}
  {
    // initialize the Poly1305 state machine
    crypto_onetimeauth_init(&state_, key_.data());

#ifndef NDEBUG
    std::cerr << "Sodium::poly1305_tee_device::poly1305_tee_device() called"
	      << std::endl;
#endif // ! NDEBUG
  }
  
  std::streamsize read(char_type* s, std::streamsize n)
  {
    BOOST_STATIC_ASSERT((
      boost::is_convertible<
                            BOOST_DEDUCED_TYPENAME boost::iostreams::category_of<Device>::type, input
                           >::value
    ));

    /**
     * NOTE: This function is not called for some reason! (XXX)
     * If yes, throw an std::runtime_exception (for now).
     * Need to better understand when read() is invoked!
     **/

    throw std::runtime_error {"Sodium::poly1305_tee_device::read() called. FIXME!"};

    // Read (up to) n chars from dev_ into buffer s
    // and update the Poly1305 state machine for this chunk
    // using the crypto_onetimeauth_*() streaming API update function
    
    std::streamsize result1 = boost::iostreams::read(dev_, s, n);

#ifndef NDEBUG
    std::cerr << "WARNING !!! Sodium::poly1305_tee_device::read() called "
	      << "[n=" << n << "] "
	      << '\n';
    if (result1 != -1) {
      std::string s_as_string {s, s+result1};
      std::cerr << "  [s=" << s_as_string << "]"
		<< std::endl;
    }
#endif // ! NDEBUG
    
    // if (result1 != -1) {
    //   crypto_onetimeauth_update(&state_,
    // 				   reinterpret_cast<unsigned char *>(s),
    // 				   result1);
    //
    //   // Don't send anything to the sink_ yet, since we're
    //   // not done yet computing the Poly1305 MAC.
    //   // 
    //   // // std::streamsize result2 = iostreams::write(sink_, s, result1);
    //   // // (void) result1; // Suppress 'unused variable' warning.
    //   // // (void) result2;
    //   // BOOST_ASSERT(result1 == result2);
    // }
    
    return result1; // nr. of bytes read.
  }

  std::streamsize write(const char_type* s, std::streamsize n)
  {
    BOOST_STATIC_ASSERT((
      boost::is_convertible<
                            BOOST_DEDUCED_TYPENAME boost::iostreams::category_of<Device>::type, output
                           >::value
    ));

    // Write (up to) n chars from the buffer s into the first sink dev_.
    // We pass the data unchanged:
    
    std::streamsize result1 = boost::iostreams::write(dev_, s, n);

    BOOST_ASSERT(result1 == n); // sanity check: we didn't lose anything
    
#ifndef NDEBUG
    std::string s_as_string {s, s+result1};
    std::cerr << "Sodium::poly1305_tee_device::write() called "
	      << "[n=" << n << "] "
	      << "[s=" << s_as_string << "] "
	      << "[result1=" << result1 << "]"
	      << std::endl;
#endif // ! NDEBUG

    // Update the Poly1305 state with the chunk we've got:
    crypto_onetimeauth_update(&state_,
			      reinterpret_cast<const unsigned char *>(s),
			      result1);

    // Don't write anything yet to the second sink sink_, because we're not
    // done yet computing the Poly1305 MAC:
    
    // std::streamsize result2 = iostreams::write(sink_, s, n);
    // (void) result1; // Suppress 'unused variable' warning.
    // (void) result2;
    // BOOST_ASSERT(result1 == n && result2 == n);
    
    return n; // or result1: nr. of bytes (not really) written.
  }

  void close()
  {
    // before closing, send the computed Poly1305 MAC:
    char_type out[MACSIZE]; // XXX: actually, char_type of sink_, not of dev_
    crypto_onetimeauth_final(&state_,
			     reinterpret_cast<unsigned char *>(out));

    std::streamsize result =
      boost::iostreams::write(sink_,
			      out,
			      MACSIZE);

#ifndef NDEBUG
    mac_type    out_as_mac_type_var {out, out+MACSIZE};
    std::string out_as_string       {tohex(out_as_mac_type_var)};
    std::cerr << "Sodium::poly1305_tee_device::close() called "
	      << "[result=" << result << "] "
	      << "[MACSIZE=" << MACSIZE << "]" << '\n'
	      << "  [out=" << out_as_string << "]"
	      << std::endl;
#endif // ! NDEBUG

    BOOST_ASSERT(result = MACSIZE); // sanity check: we didn't lose anything

    // And now, close the streams
    detail::execute_all( detail::call_close_all(dev_),
			 detail::call_close_all(sink_) );

    // reset Poly1305 state so we can start afresh with new streams:
    crypto_onetimeauth_init(&state_, key_.data());
  }
  
  bool flush()
  {
    bool r1 = boost::iostreams::flush(dev_);
    bool r2 = boost::iostreams::flush(sink_);

#ifndef NDEBUG
    std::cerr << "Sodium::poly1305_tee_device::flush() called "
	      << "[r1=" << r1 << ",r2=" << r2 << "]"
	      << std::endl;
#endif // ! NDEBUG
    
    return r1 && r2;
  }
  
  template<typename Locale>
  void imbue(const Locale& loc)
  {
    boost::iostreams::imbue(dev_, loc);
    boost::iostreams::imbue(sink_, loc);
  }

  std::streamsize optimal_buffer_size() const 
  {
    return (std::max) ( boost::iostreams::optimal_buffer_size(dev_), 
			boost::iostreams::optimal_buffer_size(sink_) );
  }
  
private:
    device_value             dev_;
    sink_value               sink_;
    key_type                 key_;
    crypto_onetimeauth_state state_;
};

template<typename Device, typename Sink>
poly1305_tee_device<Device, Sink> poly1305_tee(Device& dev, Sink& sink,
					       const typename poly1305_tee_device<Device, Sink>::key_type key)
{ return poly1305_tee_device<Device, Sink>(dev, sink, key); }

template<typename Device, typename Sink>
poly1305_tee_device<Device, Sink> poly1305_tee(const Device& dev, Sink& sink,
						 const typename poly1305_tee_device<Device, Sink>::key_type key) 
{ return poly1305_tee_device<Device, Sink>(dev, sink, key); }

template<typename Device, typename Sink>
  poly1305_tee_device<Device, Sink> poly1305_tee(Device& dev, const Sink& sink,
						 const typename poly1305_tee_device<Device, Sink>::key_type key) 
{ return poly1305_tee_device<Device, Sink>(dev, sink, key); }

template<typename Device, typename Sink>
  poly1305_tee_device<Device, Sink> poly1305_tee(const Device& dev, const Sink& sink,
						 const typename poly1305_tee_device<Device, Sink>::key_type key) 
{ return poly1305_tee_device<Device, Sink>(dev, sink, key); }
 
} // namespace Sodium

#endif // _S_POLY1305_FILTER_H_
