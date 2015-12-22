/* The dewd server
 *
 *
 */

#define AJS_HACK

#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <cstdio>
#include <functional>
#include <memory>

#include <boost/version.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "structs.h"
#include "types.h"
#include "utils.h"

#include "network_session.hpp"
#include "network_help.h"
#include "command_graph.hpp"

#include "session.hpp"
#include "serial_session.hpp"



namespace
{
	const size_t SUCCESS = 0;
//	const size_t ERROR_IN_COMMAND_LINE = 1;
	const size_t ERROR_UNHANDLED_EXCEPTION = 2;
} //namespace

using namespace dew;
namespace po = boost::program_options;

using ::boost::asio::io_service;
using ::boost::asio::ip::tcp;
using ::boost::asio::ip::address_v4;

using ::boost::chrono::steady_clock;
using ::boost::chrono::time_point;
using ::boost::chrono::nanoseconds;

using ::std::string;
using ::std::vector;

using ::std::cout;
using ::std::cerr;
using ::std::to_string;
using ::std::endl;

using ::std::ifstream;
using ::std::shared_ptr;
using ::std::unique_ptr;




int main(int argc, char** argv) {
	static_assert(BOOST_VERSION >= 104900,
			"asio waitable timer only supported in boost versions at least 1.49");

	try {
		string logging_directory;
		vector<string> rdev;
		vector<string> rwdev;
		vector<string> rwtdev;
		vector<string> wtdev;
		vector<string> wdev;
		vector<string> addr;
		string conf;
		unsigned short timeout;
		write_test_struct wts;

		po::options_description modes("Mode options");
		modes.add_options()
			("sp-comm,s", "Start the dewd in serial port communication mode.  Need to"
					" specify serial ports to read and write to.  For all serial port"
					" options specified the input string must have no trailing '/' and"
					" the option allows multiple entries. Ex. /dev/ttyS0 /dev/ttyS1")
			("net-comm,n", "Start the dewd in network communication mode.  Need to"
					" specify ip-addresses to bind to.")
			("fp-em,m", "Start dewd as a flo-point emulator rather than a"
					" communications server.  Not currently supported.")
			;
		po::options_description ifaces("Interface options");
		ifaces.add_options()
				("poll-read,R", po::value<vector<string> >(&rdev)->multitoken(),
					"Serial ports open in polling-read mode, which reads buffers from your"
					" serial port at a default rate of 10Hz.  To write to a port opened in"
					" this mode, you must open the port again in some non-reading write mode.")
				("read-write,S", po::value<vector<string> >(&rwdev)->multitoken(),
					"Serial ports open in read-write mode, which reads at the standard asio"
					" async-read-some rate and writes full commands.")
				("read-write-test", po::value<vector<string> >(&rwtdev)->multitoken(),
					"Identical to read-write except test messages are written to the port.")
				("write-test,T", po::value<vector<string> >(&wtdev)->multitoken(),
					"Identical to read-write-test except the port is non-reading.")
				("write,W", po::value<vector<string> >(&wdev)->multitoken(),
					"Identical to read-write except the port is non-reading.")
				("network,I",po::value<vector<string> >(&addr)->multitoken(),
					"Specify local ip4 addresses to bind to. It is assumed that these ip"
					" addresses can be bound to by dewd." )
				("port-number,P",po::value<int>()->default_value(2023),
					"Give a port number to bind to.  Used with --net-comm flag.  It is"
					" assumed that dewd can bind to the given port.")
				;
		po::options_description mock("Mock data options.  Mock waveforms are"
				" generated by the function\n"
				"\tpeak/\n"
				"\t   / 1 + exp(c * i),\n"
				"where i ranges from -32 to 31.");
		mock.add_options()
				("min_c", po::value<double>(&wts.min_c)->default_value(0.1),
						"The minimum value of the c parameter used to generate mock waveforms.")
				("max_c", po::value<double>(&wts.max_c)->default_value(0.4),
						"The maximum value of the c parameter used to generate mock waveforms.")
				("sample_size", po::value<int>(&wts.sample_size)->default_value(100),
						"The number of mock waveforms generated before a sample repeats.  Set"
						" to 0 for waveforms generated by rand(), instead.")
				("peak", po::value<double>(&wts.peak)->default_value(65000),
						"The peak waveform value.  Note that this number is strictly greater"
						" than all values of a mock waveform.")
				;
		po::options_description general("General options");
		general.add_options()
				("help,h", "Print help messages.")
				("logging,l",
						po::value<string>(&logging_directory)->default_value("/tmp/"),
						"Specify the path to logging folder, where input string must have"
						" trailing '/'.  Default is /tmp/dewd/. Permissions are not"
						" checked before logging begins -- it is assumed that dewd can"
						" write to the given directory.")
				("config,c",po::value<string>(&conf)->default_value(
						"/usr/local/etc/dewd/dewd.conf"), "Specify a configuration file.")
				("timeout,t",po::value<unsigned short>(&timeout)->default_value(100),
						"Used with poll-read serial port interface to set polling rate."
						"Value is in milliseconds.")
				;

		po::options_description cmdline_options;
		cmdline_options.add(modes).add(ifaces).add(mock).add(general);


		po::variables_map vmap;
		try {
			po::store(po::parse_command_line(argc,argv,cmdline_options), vmap);
			ifstream ifs {vmap["config"].as<string>().c_str()};
			po::store(po::parse_config_file(ifs, cmdline_options),vmap);

			if(vmap.count("help")) {
				cout << "The dewd DewDrop daemon.\n" << cmdline_options << '\n';

				return SUCCESS;
			}

			po::notify(vmap);
		}
		catch(po::error& poe) {

			cerr << "Exception: " << poe.what() << ".\nExiting.\n";
			return ERROR_UNHANDLED_EXCEPTION;
		}

		const int port_number = vmap["port-number"].as<int>();


		auto service = make_shared<io_service>();
		auto dis = make_shared<dispatcher>(service, logging_directory, wts);

		vector<tcp::endpoint> ends;
		if (vmap.count("network"))
			set_endpoints(&ends,&addr,port_number);

		if(vmap.count("sp-comm")){
			for(auto it : rdev)
				dis->make_r_ss(it,timeout);
			for(auto it : rwdev)
				dis->make_rw_ss(it);
			for(auto it : rwtdev)
				dis->make_rwt_ss(it);
			for(auto it : wtdev)
				dis->make_wt_ss(it);
			for(auto it : wdev)
				dis->make_w_ss(it);
		}

		if(vmap.count("net-comm")) {
			for(auto it : ends)
				dis->make_ns(it);
		}

		dis->build_command_tree();

		/*
		 * Set signals to catch for graceful termination.
		 */

		boost::asio::signal_set signals(*service, SIGINT, SIGTERM);
		signals.async_wait(boost::bind(&graceful_exit,_1,_2));

		/*
		 * io_service::run() will run the io_service until there are no jobs or
		 * handlers left to be invoked.  Because the handlers as written invoke new
		 * work, run() should never terminate.
		 *
		 */

		service->run();


	} catch (std::exception& e) {
		cerr << e.what() << endl;
	}
	return 0;
}

