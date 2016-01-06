/*
 * network_sessions.hpp
 *
 *  Created on: Nov 20, 2015
 *      Author: schurchill
 */

#ifndef NETWORK_SESSION_HPP_
#define NETWORK_SESSION_HPP_

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdio>

#include <memory>
#include <utility>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <boost/chrono.hpp>
#include <boost/chrono/time_point.hpp>

#include <boost/function.hpp>

#include "structs.h"
#include "types.h"
#include "utils.h"

#include "network_session.h"
#include "network_help.h"
#include "session.h"


namespace dew {

using ::boost::asio::io_service;
using ::boost::asio::ip::tcp;
using ::boost::chrono::steady_clock;
using ::boost::chrono::milliseconds;
using ::boost::chrono::nanoseconds;
using ::boost::bind;
using ::boost::make_iterator_range;
using ::boost::system::error_code;

using ::std::istream;
using ::std::stringstream;
using ::std::string;
using ::std::vector;
using ::std::deque;
using ::std::map;
using ::std::pair;

using ::std::size_t;

using ::std::copy;
using ::std::copy_n;

using ::std::make_shared;
using ::std::shared_ptr;

using ::std::enable_shared_from_this;

/* December 16, 2015 :: constructors */

ns::network_session(context_struct context_in) :
		context_(context_in),
		endpoint_(),
		acceptor_(*context_.service),
		socket_(*context_.service)
{
}

ns::network_session(
		context_struct context_in,
		tcp::endpoint const& ep_in
) :
		context_(context_in),
		endpoint_(ep_in),
		acceptor_(*context_.service, ep_in),
		socket_(*context_.service)
{
}

ns::network_session(
		context_struct context_in,
		tcp::socket& sock_in
) :
		context_(context_in),
		acceptor_(*context_.service),
		socket_(move(sock_in))
{
}

shared_ptr<network_session> ns::get_ns() {
	return shared_from_this();
}

void ns::do_write(stringp message) {
	auto self (shared_from_this());
	if(socket_.is_open()) {
		auto messagep = make_shared<bBuff>(message->begin(),message->end());
		auto Message = boost::asio::buffer(*messagep);
		boost::asio::async_write(
					socket_, Message, bind(&ns::handle_write, self, _1, _2, messagep));
	}
}

void ns::do_connect() {
	auto self (shared_from_this());
	socket_.async_connect(endpoint_, bind(&ns::handle_connect, self, _1));
}

void ns::do_accept() {
	auto self (shared_from_this());
	acceptor_.async_accept(socket_,
			[this,self](error_code ec)
			{
				if(!ec) {
					context_.dispatch->make_ns(socket_);
				}
				start_accept();
			});
}

void ns::do_read() {
	auto self (shared_from_this());
	socket_.async_read_some(boost::asio::buffer(request),bind(
			&ns::handle_read,self,_1,_2));
}

void ns::handle_read(
		boost::system::error_code ec, size_t in_length) {
	if(ec){
		context_.dispatch->remove_ns(shared_from_this());
		return;
	} else {
	auto self (shared_from_this());

	/* Cut out edge cases */
	if (in_length >= BUFFER_LENGTH) {
		string exceeds ("Request exceeds length.\r\n");
		do_write(make_shared<string>(exceeds));
	} else {
		sentence command = buffer_to_sentence(in_length);
		context_.dispatch->execute_network_command(command, self);
	}
	do_read();
	}
}

void ns::handle_write(
		boost::system::error_code ec, size_t in_length, bBuffp buffer) {
	buffer.reset();
	return;
}

void ns::handle_connect(boost::system::error_code ec) {
	if(ec) {
		//oh no!
	}
	return;
}

sentence ns::buffer_to_sentence(int len) {
	stringstream ss;
	for(auto c : make_iterator_range(request.begin(),request.begin()+len))
	ss << c;
	sentence s;
	string in;
	while(ss >> in)
		s.push_back(in);

	return s;
}


} // dew namespace

#endif /* NETWORK_SESSION_HPP_ */
