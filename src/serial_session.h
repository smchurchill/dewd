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

using ::std::string;
using ::std::vector;
using ::std::deque;
using ::std::size_t;

/*-----------------------------------------------------------------------------
 * November 20, 2015 :: base class
 */
class serial_session : public basic_session{
public:
	serial_session(
			io_service& io_in, string log_in, dispatcher* ref_in, string device_in) :
				basic_session(io_in, log_in, ref_in),
				name_(device_in),	port_(*io_ref, name_)
	{
	}

	string print() { return name_; }

protected:
	string name_;
	serial_port port_;
};

/*-----------------------------------------------------------------------------
 * November 20, 2015 :: _read_ class
 */
class serial_read_session :	public serial_session {
public:
	serial_read_session(
			io_service& io_in, string log_in, dispatcher* ref_in, string device_in) :
				serial_session(io_in, log_in, ref_in, device_in), timer_(*io_ref)
	{
	}

	virtual ~serial_read_session() {}

protected:
	/* November 18, 2015
	 * AJS says that pang has a 4k kernel buffer.  We want our buffer to be
	 * bigger than that, and we can be greedy with what "bigger" means.
	 */
	const long BUFFER_LENGTH = 2048;

	/* Timer specific members/methods */

	/* November 13, 2015
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
	 *
	 *
	 */
  basic_waitable_timer<steady_clock> timer_;
  time_point<steady_clock> dead_;

  /* November 18, 2015
	 * This value gives us a polling rate of 10Hz.  This is the only place where
	 * this polling rate is set.
	 */
	milliseconds timeout_ = milliseconds(100);

	/* November 20, 2015
	 * This timeout handling attempts to set a stopwatch that expires every 100ms
	 * so that the read/handle cycle of a serial_read_session operates at 10Hz.
	 * 10Hz timers are available in all serial_read_sessions, but are not
	 * necessary. The set_timer() function must be called by the inheriting class
	 * if the class wants to use the timer.
	 */
	void set_timer();
	void handle_timeout(boost::system::error_code ec);
	inline virtual void handle_timeout_extra() {}
};

/*-----------------------------------------------------------------------------
 * November 20, 2015 :: _read_parse_ class
 */
class serial_read_parse_session :	public serial_read_session {
public:
	serial_read_parse_session(
			io_service& io_in, string log_in, dispatcher* ref_in, string device_in) :
				serial_read_session(io_in, log_in, ref_in, device_in)
	{
		this->start();
	}

	void start();

private:
	void start_read();
	void handle_read(boost::system::error_code ec, size_t length,
			bBuff* buffer_);
	void check_the_deque();
	inline void handle_timeout_extra();

	time_point<steady_clock> front_last = steady_clock::now();

	const int MAX_FRAME_LENGTH = 2048;
	long int tenths_count = 0;

	pBuff to_parse;


};

/*-----------------------------------------------------------------------------
 * November 20, 2015 :: _read_log_ class
 */
class serial_read_log_session :	public serial_read_session {
public:
		serial_read_log_session(
			io_service& io_in, string log_in,	dispatcher* ref_in,	string device_in) :
				serial_read_session(io_in, log_in, ref_in, device_in)
	{
		filename_ = logdir_ + name_.substr(name_.find_last_of("/\\")+1);
		this->start();
	}

	void start();

	/* November 20, 2015
	 * This kicks off the reading work loop, and the loop returns here every time
	 * through.  This is the only place that the asyc_read work is added to the
	 * io_service.
	 *
	 * The new vector made in this method is freed when handle_read is called,
	 * once the async_read work breaks.
	 */
	void start_read();

	/* November 20, 2015
	 * The first thing we do is kick off another read work cycle by calling the
	 * start_read() method. If $length > 0 then there are characters to write, so
	 * we write $length characters from buffer_ to file_.  Otherwise, $length=0
	 * and there are no characters ready to write in buffer_.
	 */
	void handle_read(boost::system::error_code ec, size_t length,
		bBuff* buffer_);

private:
		string filename_;

};

/*-----------------------------------------------------------------------------
 * November 20, 2015 :: _write_ class
 */
class serial_write_session : public serial_session {
public:
	serial_write_session(
			io_service& io_in, string log_in,	dispatcher* ref_in,	string device_in) :
				serial_session(io_in, log_in, ref_in, device_in)
	{
	}
};

/*-----------------------------------------------------------------------------
 * November 24, 2015 :: _write_nonsense_ class
 */
class serial_write_nonsense_session : public serial_write_session {
public:
	serial_write_nonsense_session(
			io_service& io_in, string log_in, dispatcher* ref_in, string device_in) :
				serial_write_session(io_in, log_in, ref_in, device_in), timer_(*io_ref)
	{
		this->start();
	}

	void start();

private:
	void start_write();
	void handle_write(
			const boost::system::error_code& error, size_t bytes_transferred);
	bBuff* generate_nonsense();
	bBuff generate_some_sense(bBuff payload);

  time_point<steady_clock> dead_ = steady_clock::now();
  basic_waitable_timer<steady_clock> timer_;

  int internal_counter = 0;

};

} // dew namespace

#endif /* SERIAL_SESSION_H_ */
