/*
 * network_sessions.h
 *
 *  Created on: Nov 20, 2015
 *      Author: schurchill
 */

#ifndef NETWORK_SESSION_H_
#define NETWORK_SESSION_H_

namespace dew {

using ::boost::asio::io_service;
using ::boost::asio::ip::tcp;
using ::boost::chrono::steady_clock;
using ::boost::chrono::milliseconds;

using ::std::stringstream;
using ::std::string;
using ::std::vector;
using ::std::deque;
using ::std::map;
using ::std::pair;
using ::std::cout;
using ::std::size_t;
using ::std::move;

using ::std::enable_shared_from_this;

class network_session : public enable_shared_from_this<network_session> {
public:
	network_session(context_struct context_in);

	network_session(
			context_struct context_in,
			tcp::endpoint const& ep_in
	);

	network_session(
			context_struct context_in,
			tcp::socket& sock_in
	);

	~network_session() {};

	nsp get_ns();

	void start_accept() { if(acceptor_.is_open()) do_accept(); }
	void start_read() { if(socket_.is_open()) do_read(); }
	void do_write(stringp);

	void cancel_socket() { if(socket_.is_open()) socket_.cancel(); }

/* December 15, 2015
 *
 * These variables are named by whether they are initialized by the constructor
 */
private:
	context_struct context_;
	tcp::endpoint endpoint_;
	tcp::acceptor acceptor_;
	tcp::socket socket_;

	const long BUFFER_LENGTH = 8192;
	bBuff request = bBuff (BUFFER_LENGTH);

	void do_accept();
	void do_read();
	void handle_read(boost::system::error_code, size_t);
	void handle_write(boost::system::error_code, size_t, bBuffp);
	sentence buffer_to_sentence(int len);

public:
};

} // dew namespace

#endif /* NETWORK_SESSION_H_ */
