/*
 * serial_session.hpp
 *
 *  Created on: Nov 18, 2015
 *      Author: schurchill
 */

#ifndef SESSION_HPP_
#define SESSION_HPP_


#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <cstdio>
#include <algorithm>
#include <functional>

#include <utility>
#include <memory>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <boost/chrono.hpp>
#include <boost/chrono/time_point.hpp>

#include <boost/program_options.hpp>
#include <boost/range/iterator_range.hpp>

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_multiset_of.hpp>


#include "structs.h"
#include "types.h"
#include "utils.h"
#include "serial_session.h"
#include "network_session.h"
#include "network_help.h"
#include "command_graph.h"

#include "session.h"

namespace dew {

using ::boost::asio::io_service;
using ::boost::chrono::steady_clock;
using ::boost::make_iterator_range;
using ::boost::bind;

using ::std::endl;

using ::std::string;
using ::std::to_string;
using ::std::stringstream;
using ::std::vector;
using ::std::deque;
using ::std::map;
using ::std::set;
using ::std::make_pair;
using ::std::pair;

using ::std::find;
using ::std::swap;

using ::std::cout;
using ::std::count;

using ::std::unique_ptr;
using ::std::shared_ptr;
using ::std::make_shared;
using ::std::move;

using ::std::enable_shared_from_this;

/* December 16, 2015 :: constructors */

dispatcher::dispatcher(
		shared_ptr<io_service> const& io_in
) :
		dispatcher(io_in, "/dev/null")
{
}
dispatcher::dispatcher(
		shared_ptr<io_service> const& io_in,
		string log_in
) :
		dispatcher(io_in, log_in, {0,0,0,0})
{
}
dispatcher::dispatcher(
		shared_ptr<io_service> const& io_in,
		string log_in,
		write_test_struct wts_in
) :
		context_(io_in),
		logdir_(log_in),
		wts_(wts_in)
{
}

/*	December 14, 2015 :: Session creation.
 *
 * Under the new object management model, the dispatcher handles the creation
 * and destruction of all sessions.  The only sessions with lifetime less than
 * the run time of the program are the network sockets.
 *
 * This also allows us to include pseudo-default constructors in our session
 * classes that need only be passed a context_struct.
 *
 */

nsp dispatcher::make_ns (tcp::endpoint& ep_in) {
	auto pt = make_shared<ns>(context_struct(context_, shared_from_this()),ep_in);
	pt->start_accept();
	network.emplace_back(pt->get_ns());
	return pt->get_ns();
}

nsp dispatcher::make_ns (tcp::socket& sock_in) {
	auto pt = make_shared<ns>(context_struct(context_, shared_from_this()), sock_in);
	pt->start_read();
	network.emplace_back(pt->get_ns());
	return pt->get_ns();
}


void dispatcher::remove_ns (nsp to_remove) {
	to_remove->cancel_socket();

	//for(auto channel : channel_subscribers)
	//	unsub(channel.first, to_remove);

	network.remove(to_remove);
}

ssp dispatcher::make_r_ss(string device_name, unsigned short timeout) {
	auto pt = make_ss(device_name, timeout);
	serial_reading.emplace_back(pt->get_ss());
	pt->start_read();
	return pt->get_ss();
}

ssp dispatcher::make_rw_ss(string device_name) {
	auto pt = make_ss(device_name);
	serial_reading.emplace_back(pt->get_ss());
	serial_writing.emplace_back(pt->get_ss());
	pt->start_read();
	return pt->get_ss();
}

ssp dispatcher::make_rwt_ss(string device_name) {
	auto pt = make_sst(device_name);
	serial_reading.emplace_back(pt->get_ss());
	serial_writing.emplace_back(pt->get_ss());
	pt->start_read();
	pt->start_write();
	return pt->get_ss();
}

ssp dispatcher::make_wt_ss(string device_name) {
	auto pt = make_sst(device_name);
	serial_writing.emplace_back(pt->get_ss());
	pt->start_write();
	return pt->get_ss();
}

ssp dispatcher::make_w_ss(string device_name) {
	auto pt = make_ss(device_name);
	serial_writing.emplace_back(pt->get_ss());
	return pt->get_ss();
}

ssp dispatcher::make_ss(string device_name, unsigned short timeout) {
	auto pt = make_shared<ss>(context_struct(context_, shared_from_this()), device_name,
			milliseconds(timeout));
	return pt->get_ss();
}

ssp dispatcher::make_sst(string device_name) {
	auto pt = make_shared<ss>(context_struct(context_, shared_from_this()), device_name,
			wts_);
	return pt->get_ss();
}

ssp dispatcher::make_ss(string device_name) {
	auto pt = make_shared<ss>(context_struct(context_, shared_from_this()), device_name);
	return pt->get_ss();
}

/* December 15, 2015 :: network communications */

void dispatcher::execute_network_command( sentence command, nsp reference) {
	auto to_exec = walk_tree(command, root);
	(*to_exec)(reference);
}

nodep dispatcher::walk_tree( sentence command, nodep current) {
	if( command.empty() || current->is_leaf())
		return current;
	else {
		auto iter = current->get_child(command[0]);
		if(iter == current->get_end())
			return current;
		else {
			command.pop_front();
			current = iter->second;
			return walk_tree(command, current);
		}
	}
}

void dispatcher::delivery(shared_ptr<string> message) {
	auto self (shared_from_this());
	context_.service->post(bind(&dispatcher::forward,self,message));
}

void dispatcher::forward(shared_ptr<string> message) {

	auto fpm = make_shared<flopointpb::FloPointMessage>();
	bool parse_successful = fpm->ParseFromString(*message);

	if(parse_successful) {
		store_pbs(message);
		auto fpwf = make_shared<::flopointpb::FloPointMessage_Waveform>(fpm->waveform());

		for(auto subscriber : subscriptions["raw_waveforms"])
				subscriber->do_write(waveform_ts_bytes(fpwf));

		for(auto subscriber : subscriptions["ascii_waveforms"])
				subscriber->do_write(waveform_ts_ascii(fpwf));

		for(auto subscriber : subscriptions["protobuf_all"])
				subscriber->do_write(message);

		for(auto subscriber : subscriptions[fpm->name()])
				subscriber->do_write(message);

		if(local_logging_enabled){
			FILE * log = fopen((logdir_ + "dispatch.message.log").c_str(),"a");
			string s (to_string(steady_clock::now()) + ": Message received:\n");
			s += "\tName: " + fpm->name() + '\n';
			s += "\tWaveform: ";
			for(int i = 0; i < fpm->waveform().wheight().size(); ++i)
				s += to_string(fpm->waveform().wheight(i)) + '\n';
			std::fwrite(s.c_str(), sizeof(u8), s.length(), log);
			fclose(log);
		}
	} else {
		if(local_logging_enabled){
			FILE * log = fopen((logdir_ + "dispatch.failure.log").c_str(),"a");
			string s (to_string(steady_clock::now()) + ": Could not parse string.\n");
			std::fwrite(s.c_str(), sizeof(u8), s.length(), log);
			fclose(log);
		} else {
			string s (to_string(steady_clock::now()) + ": Could not parse string.\n");
			cout << s;
		}
	}
}

stringp dispatcher::waveform_ts_ascii(
		shared_ptr<::flopointpb::FloPointMessage_Waveform> fpm_p) {
	auto ascii_wf_str = make_shared<string>();
	for(auto wheight : fpm_p->wheight()) {
		ascii_wf_str->append("\t");
		ascii_wf_str->append(to_string(wheight));
	}
	ascii_wf_str->append("\n");
	return ascii_wf_str;
}


stringp dispatcher::waveform_ts_bytes(
		shared_ptr<::flopointpb::FloPointMessage_Waveform> fpm_p) {
	auto raw_wf_str = make_shared<string>();
	for(auto wheight : fpm_p->wheight()) {
		raw_wf_str->append("\t");
		raw_wf_str->append(to_string((wheight >> 24 ) & 0xFF));
		raw_wf_str->append(to_string((wheight >> 16 ) & 0xFF));
		raw_wf_str->append(to_string((wheight >> 8 ) & 0xFF));
		raw_wf_str->append(to_string(wheight & 0xFF));
	}
	raw_wf_str->append("\n");

	return raw_wf_str;
}

void dispatcher::subscribe(nsp sub, string channel) {
	auto sub_set = subscriptions.find(channel);
	if(sub_set->second.find(sub) == sub_set->second.end()) {
		sub_set->second.emplace(sub);
	} else
		if(0)
			sub->do_write(make_shared<string>("You are already subscribed to "+channel+"\n"));
}

void dispatcher::unsubscribe(nsp sub, string channel) {
	auto sub_set = subscriptions.find(channel);
	auto locator = sub_set->second.find(sub);
	if(locator != sub_set->second.end()) {
		sub_set->second.erase(locator);
		if(0)
			sub->do_write(make_shared<string>("Unsubscribed from "+channel+"\n"));
	} else
		if(0)
			sub->do_write(make_shared<string>("You are not subscribed to "+channel+"\n"));
}

void dispatcher::ports_for_zabbix(nsp in) {
	string json ("{\"data\":[");
	int not_first = 0;
	for(auto port : serial_reading) {
			if(not_first)
				json += ",";
			json += "{\"{#DEWDSP}\":\"";
			json += port->get_name();
			json += "\"}";
			++not_first;
	}
	json += "]}";
	in->do_write(make_shared<string>(json));
}

void dispatcher::stored_pbs(nsp in) {
	::flopointpb::FloPointMultiMessage fpmm;

	for(auto pbs : pbs_locations) {
		auto fpm = fpmm.add_messages();
		fpm->ParseFromString(*pbs);
	}

	in->do_write(make_shared<string>(fpmm.SerializeAsString()));
}

void dispatcher::stored_ascii_waveforms(nsp in) {
	auto to_send = make_shared<string>();
	::flopointpb::FloPointMessage fpm;

	for(auto pbs : pbs_locations) {
			fpm.ParseFromString(*pbs);
			auto fpwf = make_shared<::flopointpb::FloPointMessage_Waveform>(fpm.waveform());
			to_send->append(*waveform_ts_ascii(fpwf));
			fpm.Clear();
		}
	in->do_write(to_send);
}

int dispatcher::store_pbs(stringp str_in) {
	pbs_locations.emplace_back(str_in);
	while(pbs_locations.size()>max_size)
		pbs_locations.pop_front();

	return (max_size - pbs_locations.size());
}


/* December 16, 2015 :: command tree building */


void dispatcher::build_command_tree() {
	root->purge();
	make_branches();
	make_leaves();
	make_trunk();
	root->spawn(root_nodes);
}

void dispatcher::make_trunk() {
	auto self (shared_from_this());

	root_nodes.emplace("help", std::make_shared<node>(
			help_nodes,
			node_fn(bind(&dispatcher::help, self, _1))));
	root_nodes.emplace("get", std::make_shared<node>(
			get_nodes,
			node_fn(bind(&dispatcher::get_help, self, _1))));
	root_nodes.emplace("subscribe", std::make_shared<node>(
			subscribe_nodes,
			node_fn(bind(&dispatcher::subscribe_help, self, _1))));
	root_nodes.emplace("unsubscribe", std::make_shared<node>(
			unsubscribe_nodes,
			node_fn(bind(&dispatcher::unsubscribe_help, self, _1))));
}

void dispatcher::make_branches() {
	auto self (shared_from_this());
	help_nodes.clear();
	get_nodes.clear();
	subscribe_nodes.clear();
	unsubscribe_nodes.clear();

	help_nodes.emplace("help", std::make_shared<node>(
			node_fn( bind(&dispatcher::help_help, self, _1))));
	help_nodes.emplace("get", std::make_shared<node>(
			node_fn( bind(&dispatcher::help_get, self, _1))));
	help_nodes.emplace("subscribe", std::make_shared<node>(
			node_fn( bind(&dispatcher::help_subscribe, self, _1))));
	help_nodes.emplace("unsubscribe", std::make_shared<node>(
			node_fn( bind(&dispatcher::help_unsubscribe, self, _1))));

	get_nodes.emplace("help",std::make_shared<node>(
			node_fn( bind(&dispatcher::get_help, self, _1))));
	get_nodes.emplace("rx", std::make_shared<node>(
			node_fn( bind(&dispatcher::get_help_rx, self, _1))));
	get_nodes.emplace("tx", std::make_shared<node>(
			node_fn( bind(&dispatcher::get_help_tx, self, _1))));
	get_nodes.emplace("messages_received_tot", std::make_shared<node>(
			node_fn( bind(&dispatcher::get_help_messages_received_tot, self, _1))));
	get_nodes.emplace("messages_lost_tot", std::make_shared<node>(
			node_fn( bind(&dispatcher::get_help_messages_lost_tot, self, _1))));
	get_nodes.emplace("ports_for_zabbix", std::make_shared<node>(
			node_fn( bind(&dispatcher::ports_for_zabbix,self,_1))));
	get_nodes.emplace("stored_pbs", std::make_shared<node>(
			node_fn( bind(&dispatcher::stored_pbs,self,_1))));
	get_nodes.emplace("stored_ascii_waveforms", std::make_shared<node>(
			node_fn( bind(&dispatcher::stored_ascii_waveforms,self,_1))));

	subscribe_nodes.emplace("help", std::make_shared<node>(
			node_fn( bind(&dispatcher::subscribe_help, self, _1))));
	subscribe_nodes.emplace("to", std::make_shared<node>(
			node_fn( bind(&dispatcher::subscribe_help, self, _1))));

	unsubscribe_nodes.emplace("help", std::make_shared<node>(
			node_fn( bind(&dispatcher::unsubscribe_help, self, _1))));
	unsubscribe_nodes.emplace("from", std::make_shared<node>(
			node_fn( bind(&dispatcher::unsubscribe_help, self, _1))));


	for(auto channel : subscriptions) {
		subscribe_nodes["to"]->spawn(
				channel.first,
				make_shared<node>(
						node_fn( bind(&dispatcher::subscribe, self, _1, channel.first))));
		unsubscribe_nodes["from"]->spawn(
				channel.first,
				make_shared<node>(
						node_fn( bind(&dispatcher::unsubscribe, self, _1, channel.first))));
	}
}

void dispatcher::make_leaves() {
	for(auto port : serial_reading) {
		get_nodes["rx"]->spawn(
			port->get_name(),
			make_shared<node>(
					node_fn( bind(&ss::get_rx,port,_1))));
		get_nodes["messages_received_tot"]->spawn(
			port->get_name(),
			make_shared<node>(
					node_fn( bind(&ss::get_messages_received_tot,port,_1))));
		get_nodes["messages_lost_tot"]->spawn(
			port->get_name(),
			make_shared<node>(
					node_fn( bind(&ss::get_messages_lost_tot,port,_1))));
	}

	for(auto port : serial_writing) {
		get_nodes["tx"]->spawn(
			port->get_name(),
			make_shared<node>(
					node_fn( bind(&ss::get_tx,port,_1))));
	}

}

/*=============================================================================
 * December 16, 2015
 *
 * The following return help strings for network commands.
 */

void dispatcher::help(nsp in) {
	string to_write ("help called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::help_help(nsp in) {
	string to_write ("help_help called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::get_help(nsp in) {
	string to_write ("get_help called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::get_help_rx(nsp in) {
	string to_write ("get_help_rx(called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::get_help_tx(nsp in) {
	string to_write ("get_help_tx called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::get_help_messages_received_tot(nsp in) {
	string to_write ("get_help_messages_received_tot called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::get_help_messages_lost_tot(nsp in) {
	string to_write ("get_help_messages_lost_tot called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::get_help_ports_for_zabbix(nsp in) {
	string to_write ("get_help_ports_for_zabbix called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::help_get(nsp in) {
	get_help(in);
}

void dispatcher::subscribe_help(nsp in) {
	string to_write ("subscribe_help() called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::help_subscribe(nsp in) {
	subscribe_help(in);
}

void dispatcher::unsubscribe_help(nsp in) {
	string to_write ("unsubscribe_help called.\n");
	in->do_write(make_shared<string>(to_write));
}

void dispatcher::help_unsubscribe(nsp in) {
	unsubscribe_help(in);
}

} //namespace

#endif /* SESSION_HPP_ */
