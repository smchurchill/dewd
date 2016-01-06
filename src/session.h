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
	dispatcher(shared_ptr<io_service> const&);
	dispatcher(shared_ptr<io_service> const&, string);
	dispatcher(shared_ptr<io_service> const&, string, write_test_struct);
	~dispatcher() {}

private:
	context_struct_lite context_;
	string logdir_;
	write_test_struct wts_;

	list<ssp> serial_reading;
	list<ssp> serial_writing;
	list<nsp> network;

	map<string,set<nsp> > subscriptions = {
			{"raw_waveforms",{}},
			{"ascii_waveforms",{}},
			{"protobuf_all",{}},
			{"0of09",{}},
			{"1of09",{}},
			{"2of09",{}},
			{"3of09",{}},
			{"4of09",{}},
			{"5of09",{}},
			{"6of09",{}},
			{"7of09",{}},
			{"8of09",{}},
			{"9of09",{}}
	};

	deque<stringp> pbs_locations;

	const int max_size = 10000;
	bool local_logging_enabled = false;


/* Method type: creation and destruction of sessions */
public:
	nsp make_ns (tcp::endpoint&);
	nsp make_fp_ns (tcp::endpoint&);
	nsp make_ns (tcp::socket&);
	void remove_ns (nsp);

	ssp make_r_ss(string, unsigned short);
	ssp make_rw_ss(string);
	ssp make_rwt_ss(string);
	ssp make_wt_ss(string);
	ssp make_w_ss(string);
private:
	ssp make_ss (string, unsigned short);
	ssp make_sst (string);
	ssp make_ss (string);

/* Method type: network communications */
public:
	void execute_network_command(sentence, nsp);
	nodep walk_tree( sentence, nodep);
	void delivery(stringp);
	string get_command_tree_from_root();

private:
	void forward(stringp);
	void forward_handler(const error_code&,size_t, bBuffp, nsp);

	stringp waveform_ts_ascii(shared_ptr<::flopointpb::FloPointMessage_Waveform>);
	stringp waveform_ts_bytes(shared_ptr<::flopointpb::FloPointMessage_Waveform>);

	void subscribe(nsp, string);
	void unsubscribe(nsp, string);

	void ports_for_zabbix(nsp);
	void stored_pbs(nsp);
	void stored_ascii_waveforms(nsp);

	int store_pbs(stringp);

	string command_tree_from(nodep);

/* Method type: command tree building */
public:
	void build_command_tree();
private:
	void make_trunk();
	void make_branches();
	void make_leaves();

/* Method type: basic information */
public:
	string get_logdir() { return logdir_; }
	void see_tree() {dprint(root->descendants(0));}

/* Member type: command tree from root */
private:
	void help(nsp);
	void help_help(nsp);
	void get_help(nsp);
	void get_help_rx(nsp);
	void get_help_tx(nsp);
	void get_help_messages_received_tot(nsp);
	void get_help_messages_lost_tot(nsp);
	void get_help_ports_for_zabbix(nsp);
	void help_get(nsp);
	void subscribe_help(nsp);
	void help_subscribe(nsp);
	void unsubscribe_help(nsp);
	void help_unsubscribe(nsp);

	map<string,nodep> help_nodes;
	map<string,nodep> get_nodes;
	map<string,nodep> subscribe_nodes;
	map<string,nodep> unsubscribe_nodes;
	map<string,nodep> root_nodes;

	nodep root = make_shared<node>();
};


} // dew namespace

#endif /* SESSION_H_ */
