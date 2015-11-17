/*
 * dewd_classes.hpp
 *
 *  Created on: Nov 17, 2015
 *      Author: schurchill
 */

#ifndef SRC_DEWD_CLASSES_HPP_
#define SRC_DEWD_CLASSES_HPP_

/* For std::make_shared and std::enable_shared_from_this */
#include <memory>

/* For std::move */
#include <utility>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/chrono.hpp>
#include <boost/chrono/time_point.hpp>
#include <boost/chrono/duration.hpp>

#define PORT_NUMBER 50001
#define TIMEOUT 100
#define BUFFER_LENGTH 2048

/*-----------------------------------------------------------------------------
 * November 10, 2015
 * class serial_read_session
 *  The read and write sessions of the twnn server are separated because we have
 * a timeout implemented on reads.  We would like to rapidly create a trusted
 * communications log for our commands rather than fill our buffer each time.
 * The hope is that the integrity of the communications will be preserved by
 * this method.
 *
 * On the other hand, we would like to ensure that our whole buffer is clear
 * when we write to avoid half commands getting through to a flopoint board.
 * The boost::asio::serial_port.cancel() function cancels all read and write
 * operations in progress when it is called, so our specifications do require
 * the use of two separate boost::asio::serial_port instances: one for reading
 * and one for writing.
 *
 */



class serial_read_session {
public:
	serial_read_session(boost::asio::io_service& io_service,
			std::string location, std::string DATAPATH) :
			port_(io_service), name_(location), timeout_(TIMEOUT), timer_(io_service)
	{
		filename_ = DATAPATH + name_.substr(name_.find_last_of("/\\"));
		port_.open(location);
		name_.resize(11,' ');
		start_ = boost::chrono::steady_clock::now();
		dead_ = start_ + timeout_;

		start_read();
		set_timer();
	}

	/* For now, this function begins the read/write loop with a call to
	 * port_.async_read_some with handler handle_read.  It makes a new vector,
	 * where the promise to free the memory is honored at the end of handle_read.
	 *
	 * Later, it will take care of making sure that buffer_, port_, and file_ are
	 * sane.  Other specifications are tbd.
	 */

private:
	void start_read() {
		std::vector<char>* buffer_ = new std::vector<char>;
		buffer_->assign (BUFFER_LENGTH,0);
		boost::asio::mutable_buffers_1 Buffer_ = boost::asio::buffer(*buffer_);
		boost::asio::async_read(port_,Buffer_,
				boost::bind(&serial_read_session::handle_read, this, _1, _2, buffer_));
	}

	/* This handler first gives more work to the io_service.  The io_service will
	 * terminate if it is ever not executing work or handlers, so passing more
	 * work to the service at the start of this handler will reduce the possible
	 * situations where that could happen.
	 *
	 * If $length > 0 then there are characters to write, and we write $length
	 * characters from buffer_ to file_.  Otherwise, $length=0 and there are no
	 * characters ready to write in buffer_.  We increase the count since last
	 * write.  If a nonzero number of characters are written then that count is
	 * set to 0.
	 *
	 *
	 */
	void handle_read(boost::system::error_code ec, size_t length,
		std::vector<char>* buffer_) {
		/* The first thing we do is give more work to the io_service.  Allocate a
		 * new buffer to pass to our next read operation and for our next handler
		 * to read from.  That is all done in start().
		 */
		this->start_read();

		if (length > 0) {
			file_ = fopen(filename_.c_str(), "a");
			fwrite(buffer_->data(), sizeof(char), length, file_);
			fclose(file_);
		}
		if(0)
		std::cout << "[" << name_ << "]: " << length << " characters written" << std::endl;

		delete buffer_;
	}

  /* This timeout handling is based on the blog post located at:
   *
   * blog.think-async.com/2010/04/timeouts-by-analogy.html
   *
   * The basic idea is that there are two pieces of async work in handled by a
   * serial_read_session: a read and a wait.
   *
   * The async_wait runs the handle_timout handler either when time expires or
   * when the wait operation is canceled.  Nothing will cancel the wait in this
   * program at the moment.
   *
   * The async_read reads characters from the port_ buffer into this session's
   * buffer_ container.  The async_read will exit and run read_handler when
   * either buffer_ is full or timer_ expires and invokes handle_timeout, which
   * cancels all current work operations in progress through port_.
   */
	void set_timer() {
	boost::chrono::time_point<boost::chrono::steady_clock> now_ =
			boost::chrono::steady_clock::now();

	while(dead_< now_)
		dead_ = dead_ + timeout_;
	if(0)
	std::cout << "[" << name_ << "]: deadline set to " << dead_ << '\n';
	timer_.expires_at(dead_);
	timer_.async_wait(
    	boost::bind(&serial_read_session::handle_timeout, this, _1));
}

  void handle_timeout(boost::system::error_code ec) {

  	if(!ec)
  		port_.cancel();

  	set_timer();
}




	boost::asio::serial_port port_;
	std::string name_;
	std::string filename_;
	FILE* file_;

	/* Timer specific members */
	boost::chrono::milliseconds timeout_;

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

  boost::asio::basic_waitable_timer<boost::chrono::steady_clock> timer_;
  boost::chrono::time_point<boost::chrono::steady_clock> start_;
  boost::chrono::time_point<boost::chrono::steady_clock> dead_;
};

/*-----------------------------------------------------------------------------
 * November 10, 2015
 * class serial_write_session
 *
 * We plan to write commands to the serial port that are dictated by commands
 * read from a network socket.
 *
 * Depends:
 * 	- network_session
 * 	- dispatch
 */

class serial_write_session {
public:
	serial_write_session();

	/*
	 */
	void start() {
	}

private:
	/*
	 *
	 */

};

/*-----------------------------------------------------------------------------
 * November 10, 2015
 * class network_session
 */

class network_session: public std::enable_shared_from_this<network_session> {
public:
	network_session(boost::asio::ip::tcp::socket sock) :
			socket_(std::move(sock)) {
		start_ = boost::chrono::steady_clock::now();
	}

	void start() {
		do_read();

		if(0){
		std::cout << "hello" << '\n';
		std::vector<char>* buffer_ = new std::vector<char>;
		buffer_->assign (BUFFER_LENGTH,0);
		boost::asio::mutable_buffers_1 Buffer_ = boost::asio::buffer(*buffer_);
		socket_.async_read_some(Buffer_,
				boost::bind(&network_session::handle_read, this, _1, _2, Buffer_));
		}
	}

private:

	/*
	 * November 16, 2015
	 * Simple echoing protocol modeled on the tcp echo server example in boost
	 * asio library.
	 */
	void do_read() {
		auto self(shared_from_this());
		socket_.async_read_some(boost::asio::buffer(data_),
				[this,self](boost::system::error_code ec, std::size_t length)
				{
					if(!ec)
						do_write(length);
					else
						std::cout << ec << '\n';
				});
	}

	void do_write(std::size_t length)
	{
		auto self(shared_from_this());
		std::cout << "[" << socket_.local_endpoint() << "] started at [ " <<
				start_ << "]"	<< '\n';
		for(std::size_t i = 0 ; i < length ; ++i)
			std::cout << data_[i];
		std::cout << '\n';
		boost::asio::async_write(socket_,boost::asio::buffer(data_,length),
				[this,self](boost::system::error_code ec, std::size_t)
				{
					do_read();
				});
	}









	void handle_read(boost::system::error_code ec, std::size_t length,
			boost::asio::mutable_buffers_1 Buffer_) {
		boost::asio::async_write(socket_, Buffer_,
				boost::bind(&network_session::handle_write, this, _1, _2));
	}

	void handle_write(boost::system::error_code ec, std::size_t length) {
		start();
	}

	std::vector<char> data_ = std::vector<char>(BUFFER_LENGTH,0);
	boost::asio::ip::tcp::socket socket_;

  boost::chrono::time_point<boost::chrono::steady_clock> start_;
};

/*-----------------------------------------------------------------------------
 * November 13, 2015
 * class network_acceptor
 *
 * The acceptor is initialized with a port to bind to and an io_service to ass-
 * -ign work to.  It sits on an acceptor and a socket, waiting for someone to
 * try to connect.  When a connection is accepted it is assigned to socket_,
 * and a new shared_ptr to socket_ is created and passed to a new network_sess-
 * -ion which will communicate through socket_. The server then continues to
 * sit on socket_ and acceptor_ and waits for a new connection.
 *
 * The intent is to have an acceptor listening on port 50001 over each of the
 * ip addresses on pang that are assigned to flopoint.  As of writing, those
 * are 192.168.16.X for X in [0:8].
 *
 */

class network_acceptor {
public:
	network_acceptor(boost::asio::io_service& io_service,
			boost::asio::ip::tcp::endpoint ep ) :
			acceptor_(io_service,ep,true),
					socket_(io_service) {
		do_accept();
	}

private:
	void do_accept() {
		acceptor_.async_accept(socket_, [this](boost::system::error_code ec)
		{
			if(!ec) {
				std::make_shared<network_session>(std::move(socket_))->start();
			}
			do_accept();
		});
	}

	boost::asio::ip::tcp::acceptor acceptor_;
	boost::asio::ip::tcp::socket socket_;
};


/*-----------------------------------------------------------------------------
 * November 10, 2015
 * class dispatch
 *
 * A single dispatch object should be instantiated in main, before any other
 * object is created.  Other class constructors will be overloaded to accept a
 * pointer to a dispatch object.  That dispatch object will be able to route
 * communications between our other objects.  The desired functionality is des-
 * cribed by the following example:
 *
 * 0. Server twnn is launched on pang, listening on port 50001.
 * 			One dispatch object dispatch_, one server object server_, and ~enough~
 * 			serial_read_session and serial_write_session objects srs_i and sws_i
 * 			are instantiated.
 * 1. Client zabbix connects to pang on port 50001.
 * 2. server_ instantiates a network_session net_j to talk to zabbix.
 * 3.	zabbix sends a command to pang, asking for the GUID of the flopoint board
 * 			connected to serial port i.
 * 4. net_j forwards the command to dispatch which forwards the command to sws_i
 * 5. sws_i asks its board for a GUID.
 * 6. srs_i read a GUID, and sends that to dispatch.
 * 7. dispatch_ heard about a GUID from srs_i.  net_j asked for the GUID of who
 * 			is attached to serial port i, so dispatch sends the GUID to net_j
 * 8. net_j forwards the GUID to zabbix.
 *
 * This level of translation is needed because the commands being issued to and
 * from the flopoint board are intended to be bjson, while the communication
 * across the network is intended to be json.  The different wrappers should be
 * simple, but necessary.
 *
 */



/*-----------------------------------------------------------------------------
 * November 10, 2015
 * twnn accepts as arguments:
 * argv[1] is the path to a logging folder i.e. /var/log/twnn
 * argv[i] for i in [2,argc-1] are device names i.e. /dev/ttyS5
 *
 * November 16, 2015
 * Beginning the switch over to boost program_options library to parse command
 * line arguments.
 */



#endif /* SRC_DEWD_CLASSES_HPP_ */