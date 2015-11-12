/* The twnn server
 *
 *
 */

#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

/* For std::make_shared and std::enable_shared_from_this */
#include <memory>

/* For std::move */
#include <utility>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>

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
			port_(io_service), name_(location), timer_(io_service) {
		filename_ = DATAPATH + name_.substr(name_.find_last_of("/\\"));
		port_.open(location);
		name_.resize(11,' ');
		start();
	}

	/* For now, this function begins the read/write loop with a call to
	 * port_.async_read_some with handler handle_read.  It makes a new vector,
	 * where the promise to free the memory is honored at the end of handle_read.
	 *
	 * Later, it will take care of making sure that buffer_, port_, and file_ are
	 * sane.  Other specifications are tbd.
	 */
	void start() {
    timer_.expires_from_now(boost::posix_time::milliseconds(TIMEOUT));

		std::vector<char>* buffer_ = new std::vector<char>;
		buffer_->assign (BUFFER_LENGTH,0);
		boost::asio::mutable_buffers_1 Buffer_ = boost::asio::buffer(*buffer_);
		boost::asio::async_read(port_,Buffer_,
				boost::bind(&serial_read_session::handle_read, this, _1, _2, buffer_));

    timer_.async_wait(
    		boost::bind(&serial_read_session::handle_timeout, this, _1));

	}

	void stop_read() {
		port_.cancel();
	}

private:
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
		this->start();

		if (length > 0) {
			file_ = fopen(filename_.c_str(), "a");
			fwrite(buffer_->data(), sizeof(char), length, file_);
			fclose(file_);
		}

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

  void handle_timeout(boost::system::error_code ec) {
    if (timer_.expires_from_now() < boost::posix_time::seconds(0)) {
      timer_.expires_from_now(boost::posix_time::milliseconds(TIMEOUT));
      timer_.async_wait(bind(&serial_read_session::handle_timeout, this, _1));
      port_.cancel();
    } else {
      timer_.async_wait(
          boost::bind(&serial_read_session::handle_timeout, this, _1));
    }

  }




	boost::asio::serial_port port_;
	std::string name_;
	std::string filename_;
	FILE* file_;

	/* Timer specific members */
  boost::asio::deadline_timer timer_;

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
	}

	void start() {
		socket_.async_read_some(boost::asio::buffer(buffer_, BUFFER_LENGTH),
				boost::bind(&network_session::handle_read, this, _1));
	}

	/* haven't decided what we want to do with what we've read yet */
	void handle_read(boost::system::error_code ec) {

	}

private:

	char buffer_[BUFFER_LENGTH];
	boost::asio::ip::tcp::socket socket_;
};

/*-----------------------------------------------------------------------------
 * November 10, 2015
 * class network_server
 *
 * The server is initialized with a port to bind to and an io_service to assign
 * work to.  It sits on an acceptor and a socket, waiting for someone to try to
 * connect.  When a connection is accepted it is assigned to socket_, and a new
 * shared_ptr to socket_ is created and passed to a new network_session which
 * will communicate through socket_. The server then continues to sit on socket_
 * and acceptor_ and waits for a new connection.
 *
 */

class network_server {
public:
	network_server(boost::asio::io_service& io_service, int port) :
			acceptor_(io_service,
					boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)), socket_(
					io_service) {
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
 */


int main(int argc, char* argv[]) {
	try {

		/*
		 * Can use argv[1] to see if we have permission to modify the given path.
		 * That would keep us from unsuccessfully logging things.
		 */
		if (argc < 3) {
			std::cerr << "No serial port names supplied\n";
			return 1;
		}

		boost::asio::io_service io_service;

		/*
		 * Instantiate the serial port sessions
		 */

		std::vector<serial_read_session*> vec;
		for (int i = 2; i < argc; ++i) {
			serial_read_session* s = new serial_read_session(io_service, argv[i], argv[1]);
			vec.push_back(s);
		}

		/*
		 * io_service.run() will run the io_service until there are no jobs or han-
		 * dlers left to be invoked.  Because the handlers as written invoke new
		 * work, run() should never terminate.
		 *
		 */
		io_service.run();

	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
