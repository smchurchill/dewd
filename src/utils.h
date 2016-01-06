/*
 * utils.h
 *
 *  Created on: Nov 24, 2015
 *      Author: schurchill
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <iterator>

#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>
#include <string>
#include <sstream>

#include <cctype>
#include <ios>

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>


namespace dew {

/*-----------------------------------------------------------------------------
 * November 25, 2015
 *
 * wanted a nonnegative modulus operator
 */

inline int mod(int n, int m) {
	return (n%m < 0 ? n%m + m : n%m);
}


/*-----------------------------------------------------------------------------
 * November 25, 2015
 *
 * specialized fn from AJS
 */

using ::boost::asio::async_write;

using ::boost::asio::ip::address_v4;
using ::boost::asio::ip::tcp;

using ::std::stringstream;
using ::std::string;
using ::std::to_string;
using ::std::vector;

using ::std::iterator_traits;
using ::std::enable_if;
using ::std::is_same;
using ::std::copy;
using ::std::ostream_iterator;
using ::std::cout;
using ::std::endl;
using ::std::isgraph;

/*
template <typename T> using ivt = typename iterator_traits<T>::value_type;

template<typename Input>
typename enable_if<!is_same<ivt<Input>,u8>::value>::type
debug (Input first, Input last) {
    copy (first, last, ostream_iterator<ivt<Input>> (cout, " "));
    cout << '\n';
}

template<typename Input>
typename enable_if<is_same<ivt<Input>,u8>::value>::type
debug (Input first, Input last) {
    copy (first, last, ostream_iterator<ivt<Input>> (cout));
    cout << '\n';
}


void test_specialization (void)
{
	cout << "==Test specialization==\n";

	string sc = "hello world";

    std::vector<int> vi = { 3, 1, 2 };
    debug (vi.begin(), vi.end());

    std::vector<u8> vc (sc.begin(), sc.end());
    debug (vc.begin(), vc.end());

    std::deque<u8> dc (sc.begin(), sc.end());
    debug (dc.begin(), dc.end());
}
*/

/*-----------------------------------------------------------------------------
 * November 25, 2015
 *
 * Range adapter from AJS
 */




/* could just as well transform !is_print(c) to <ff><fe> notation*/
string filter_unprintable (u8 c) {
	stringstream ss, cs;
	ss << "<" << (int)c << ">";
	cs << c;
  return string ( isprint(c) ? cs.str() : ss.str() );
}

template<typename Range>
void debug (Range rng) {
	// typedef typename Range::first_type::value_type T;
	boost::transform (rng, ostream_iterator<string> (cout), filter_unprintable);
	cout << endl;
}

template<typename Range>
void debug (Range rng, std::iostream* ios) {
	// typedef typename Range::first_type::value_type T;
	boost::transform (rng, ostream_iterator<string> (*ios), filter_unprintable);
	*ios << endl;
}

template<typename Range>
void debug_noline (Range rng) {
	// typedef typename Range::first_type::value_type T;
	boost::transform (rng, ostream_iterator<string> (cout), filter_unprintable);
}

template<typename Range>
string debug_str (Range rng) {
	stringstream ss;
	// typedef typename Range::first_type::value_type T;
	boost::transform (rng, ostream_iterator<string> (ss), filter_unprintable);
	return ss.str();
}

void debug (string s) {
	debug(boost::make_iterator_range(s.begin(),s.end()));
}

template<typename Printable>
void dprint(Printable p) {
	cout << p << '\n';
}
 /*
void test_range (void)
{
	cout << "==Test range==\n";
	string sc = "hello world";
    std::deque<u8> dc (sc.begin(), sc.end());
    debug (std::make_pair (dc.begin(), dc.end()));
}
*/



/*-----------------------------------------------------------------------------
 * November 25, 2015
 *
 * We will use the generating polynomial x^8 + x^2 + x^1 + 1.  This
 * can also be written as 10000111 or 135 or 0x87, etc.
 *
 * This implementation taken from chromium.googlesource.com.
 * first google result for "crc8 c".
 */

template< typename Range >
u8 crc8( Range rng ) {

	unsigned crc = 0;
	int j;
	for( const auto & i : rng ) {
			crc ^= (i << 8);
			for( j=8 ; j ; --j) {
				if(crc & 0x8000)
					crc^= (0x1070 << 3);
				crc <<=1;
			}
	}

	return (u8)(crc >> 8);
}



/*-----------------------------------------------------------------------------
 * November 27, 2015
 *
 * Shuffling some code away from main()
 */

void graceful_exit(const boost::system::error_code& error, int signal_number)
{
	exit(0);
}



/* January 5, 2016
 *
 * Takes an ip4_addr:port_number string as input and returns an endpoint.
 */
using ::boost::asio::ip::tcp;
using ::boost::asio::ip::address_v4;
using ::std::string;

tcp::endpoint stoe (string str) {
	string ip;
	short port;
	auto pos = str.find(':');
	if(pos == string::npos) {
		ip = str;
		port = 2023;
	} else { // pos < npos...
		assert(pos < string::npos);
		ip = str.substr(0, pos);
		port = std::stoi(str.substr(pos+1, string::npos));
	}
	tcp::endpoint endpoint (address_v4::from_string(ip), port);
	return endpoint;
}

}; // namespace dew


#endif /* UTILS_H_ */
