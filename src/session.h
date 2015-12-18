/*
 * session.h
 *
 *  Created on: Nov 20, 2015
 *      Author: schurchill
 */

#ifndef SESSION_H_
#define SESSION_H_

#include "command_graph.h"

namespace dew {

using ::boost::asio::io_service;
using ::boost::asio::ip::tcp;
using ::boost::asio::serial_port;
using ::boost::chrono::steady_clock;
using ::boost::chrono::time_point;
using ::boost::chrono::milliseconds;
using ::boost::bind;
using ::boost::system::error_code;

using ::std::string;
using ::std::to_string;
using ::std::vector;
using ::std::deque;
using ::std::list;
using ::std::set;
using ::std::map;
using ::std::pair;
using ::std::make_pair;

using ::std::move;

using ::std::unique_ptr;
using ::std::shared_ptr;
using ::std::weak_ptr;
using ::std::enable_shared_from_this;


class dispatcher : public enable_shared_from_this<dispatcher> {
public:
	dispatcher(shared_ptr<io_service> const& io_in);
	dispatcher(shared_ptr<io_service> const& io_in, string log_in);
	~dispatcher() {}

private:
	context_struct_lite context_;
	string logdir_;

	list<ssp> serial_reading;
	list<ssp> serial_writing;
	list<nsp> network;

	map<string,set<nsp> > subscriptions = {
			{"raw_waveforms",{}},
			{"ascii_waveforms",{}}
	};

	bool local_logging_enabled = false;
	bool added_static_leaves = false;


/* Method type: creation and destruction of sessions */
public:
	nsp make_ns (tcp::endpoint&);
	nsp make_ns (tcp::socket&);
	void remove_ns (nsp);

	ssp make_r_ss(string, unsigned short);
	ssp make_rw_ss(string);
	ssp make_rwt_ss(string);
	ssp make_wt_ss(string);
	ssp make_w_ss(string);
private:
	ssp make_ss (string, unsigned short);
	ssp make_ss (string);

/* Method type: network communications */
public:
	void execute_network_command(sentence, nsp);
	nodep execute_tree( sentence, nodep);
	void delivery(shared_ptr<string>);
	string get_command_tree_from_root();

private:
	void forward(shared_ptr<string>);
	void forward_handler(const error_code&,size_t, bBuffp, nsp);

	void subscribe(nsp, string);
	void unsubscribe(nsp, string);

	void ports_for_zabbix(nsp);
	string command_tree_from(nodep);


/* Method type: command tree building */
public:
	void build_command_tree();
private:
	void purge_dynamic_leaves();
	void add_dynamic_leaves();
	void add_static_leaves();


/* Method type: basic information */
public:
	string get_logdir() { return logdir_; }
	shared_ptr<const ss> get_ss_from_name(string name);
	shared_ptr<const ns> get_ns_from_name(string name);

/* Member type: command tree from root */
private:
	map<string,nodep> help_nodes = {
			{string("help"), std::make_shared<node>(
					std::function<void(nsp)>(&help_help))},
			{string("get"), std::make_shared<node>(
					std::function<void(nsp)>(&help_get))},
			{string("subscribe"), std::make_shared<node>(
					std::function<void(nsp)>(&help_subscribe))},
			{string("unsubscribe"), std::make_shared<node>(
					std::function<void(nsp)>(&help_unsubscribe))}
	};

	map<string,nodep> get_nodes = {
			{string("help"),std::make_shared<node>(
					std::function<void(nsp)>(&get_help))},
			{string("rx"), std::make_shared<node>(
					std::function<void(nsp)>(&get_help_rx))},
			{string("tx"), std::make_shared<node>(
					std::function<void(nsp)>(&get_help_tx))},
			{string("messages_received_tot"), std::make_shared<node>(
					std::function<void(nsp)>(&get_help_messages_received_tot))},
			{string("messages_lost_tot"), std::make_shared<node>(
					std::function<void(nsp)>(&get_help_messages_lost_tot))},
			{string("ports_for_zabbix"), std::make_shared<node>()}
	};

	map<string,nodep> subscribe_nodes = {
			{string("help"), std::make_shared<node>(
					std::function<void(nsp)>(&subscribe_help))},
			{string("to"), std::make_shared<node>(
					std::function<void(nsp)>(&subscribe_help))}
	};

	map<string,nodep> unsubscribe_nodes = {
			{string("help"),std::make_shared<node>(
					std::function<void(nsp)>(&unsubscribe_help))},
			{string("from"), std::make_shared<node>(
					std::function<void(nsp)>(&unsubscribe_help))}
	};

	map<string,nodep> root_nodes = {
			{string("help"), std::make_shared<node>(help_nodes,
					std::function<void(nsp)>(&help))},
			{string("get"), std::make_shared<node>(get_nodes,
					std::function<void(nsp)>(&get_help))},
			{string("subscribe"), std::make_shared<node>(subscribe_nodes,
					std::function<void(nsp)>(&subscribe_help))},
			{string("unsubscribe"), std::make_shared<node>(unsubscribe_nodes,
					std::function<void(nsp)>(&unsubscribe_help))}
	};

	node root;
};


} // dew namespace

#endif /* SESSION_H_ */
