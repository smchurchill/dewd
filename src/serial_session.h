/*
 * serial_session.h
 *
 *  Created on: Nov 20, 2015
 *      Author: schurchill
 */

#ifndef SERIAL_SESSION_H_
#define SERIAL_SESSION_H_

namespace dew {

using ::boost::asio::io_service;
using ::boost::chrono::steady_clock;

using ::boost::asio::basic_waitable_timer;
using ::boost::chrono::time_point;
using ::boost::chrono::milliseconds;

using ::boost::asio::serial_port;

using ::boost::bind;

using ::boost::system::error_code;

using ::std::string;
using ::std::vector;
using ::std::pair;
using ::std::deque;
using ::std::size_t;

/*=============================================================================
 * December 15, 2015 :: serial_session class
 *
 * Contains both read and write capabilities as of December 15, 2015.
 */
class serial_session : public enable_shared_from_this<serial_session> {
public:
	serial_session(context_struct context_in);

	serial_session(
			context_struct context_in,
			string device_in,
			milliseconds timeout_in
	);

	serial_session(
			context_struct context_in,
			string device_in
	);

	serial_session(
				context_struct context_in,
				string device_in,
				write_test_struct wts_in
		);

	shared_ptr<serial_session> get_ss();

	/* It is recommended to only invoke _timeout_read if the port is not writing
	 * to avoid potential loss or clobbering of data.
	 *
	 * It is also recommended that at most one type of reading/writing loop is
	 * started in a single serial session for similar reasons.
	 */
	void start_write();
	void start_read();



/* December 15, 2015 :: serial_session variables
 *
 * These variables are named by whether they are initialized by the constructor
 */

/* November 13, 2015 :: Timer specific members/methods
 *
 * Using:
 * 	boost::asio::basic_waitable_timer<boost::chrono::steady_clock>
 * 	boost::chrono::time_point<boost::chrono::steady_clock>
 *
 * over:
 * 	boost::asio::steady_timer
 * 	boost::asio::steady_timer::time_point
 *
 * because the latter does not work.  The typedefs of the asio library should
 * make them act the same, but they don't.  The latter doesn't even compile.
 *
 * May be related to the issue located at:
 *	www.boost.org/doc/libs/1_59_0/doc/html/boost_asio/overview/cpp2011/chrono.html
 */
private:
	context_struct context_;
	serial_port port_;
	int fd_;
	string name_;
	milliseconds timeout_;
  basic_waitable_timer<steady_clock> timer_;
  bool read_type_is_timeout_;
  write_test_struct wts_{0};

  bool write_type_is_test = false;
	const size_t MAX_FRAME_LENGTH = 4096;
	const size_t BUFFER_LENGTH = 16000;
	serial_icounter_struct ioctl_counters {0};
	message_counter_struct counts {0};
	time_point<steady_clock> front_last = steady_clock::now();
	time_point<steady_clock> dead = steady_clock::now();
	pBuff to_parse;

/* December 15, 2015 :: serial_session methods
 *
 * Method type: prework
 */
	void do_write();
	void do_write(bBuffp);
	void do_read();

/* Method type: postwork */
	void handle_write(const error_code&, size_t, bBuffp);
	void handle_read(const error_code&, size_t, bBuffp);

/* Method type: debug */
	void scope(bBuffp);

/* Method type: complicated information handling */
	void set_a_check();
	void check_the_deque();
	int scrub(pBuff::iterator);
	int pop_counters();
	bBuffp generate_message();

/* Method type: time handling */
	void set_read_timer();
	void handle_read_timeout(const error_code&);

/* Method type: basic information */
public:
	string get_tx();
	void get_tx(nsp in);
	string get_rx();
	void get_rx(nsp in);
	string get_messages_received_tot();
	void get_messages_received_tot(nsp in);
	string get_messages_sent_tot();
	string get_messages_lost_tot();
	void get_messages_lost_tot(nsp in);
	string get_frame_too_old();
	string get_frame_too_long();
	string get_bad_prefix();
	string get_bad_crc();
	string get_bytes_received();
	string get_msg_bytes_tot();
	string get_wrapper_bytes_tot();
	string get_garbage();
	string get_name();
	string get_type();
};

} // dew namespace

#endif /* SERIAL_SESSION_H_ */
