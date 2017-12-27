//
// Created by xzl on 12/23/17.
//

#include <zmq.hpp>
#include <iostream>
#include <boost/program_options.hpp>

#include "config.h"
#include "mydecoder.h"

#include "log.h"
#include "measure.h"
#include "msgfmt.h"

using namespace std;
using namespace vstreamer;

namespace po = boost::program_options;

#define H264_FILE "/shared/videos/video-200p-10.mp4"
//#define H264_FILE "/tmp/output.mkv"
//#define H264_FILE "/shared/videos/serenity.mp4"
#define MPG1_FILE "/shared/videos/hst_1.mpg"

struct serv_config {
	bool use_hw; /* using hw decoder? */
	int 	pull_from_port; /* where we fetch chuncks */
	int 	push_to_port; 	/* where to push frames */

	serv_config () : use_hw(true), pull_from_port(5556), push_to_port(5557) { }
};

struct serv_config the_config;

void parse_options(int ac, char *av[], serv_config* config)
{
	po::variables_map vm;
	xzl_assert(config);

	try {

		po::options_description desc("Allowed options");
		desc.add_options()
				("help,h", "produce help message")
				("hw-decode,w", "using hw decoder")
//		("bundles,b", po::value<unsigned long>(), "bundles per wm interval")
//		("target_tput,t", po::value<unsigned long>(), "target throughput (krec/s)")
//		("record_size,z", po::value<unsigned long>(), "record (string_range) size (bytes)")
//		("cores,c", po::value<unsigned long>(), "# cores for worker threads")
//		("source_tasks,s", po::value<unsigned int>(), "# for source tasks")
//		("input_file,i", po::value<vector<string>>(), "input file path") /* must be vector */
				;

		po::store(po::parse_command_line(ac, av, desc), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << desc << "\n";
			exit(1);
		}

		if (vm.count("hw-decode")) {
			config->use_hw = true;
		}

//		if (vm.count("record_size")) {
//			config->record_size = vm["record_size"].as<unsigned long>();
//		}
//
//		/* determine cores first */
//		if (vm.count("cores")) {
//			config->cores = vm["cores"].as<unsigned long>();
//		} else { /* default value for # cores (save one for source) */
//			config->cores = std::thread::hardware_concurrency() - 1;
//		}
//
//		if (vm.count("source_tasks")) {
//			config->source_tasks = vm["source_tasks"].as<unsigned int>();
//		} else { /* default value for # cores (save one for source) */
//			config->source_tasks = config->cores;
//		}
//
//		if (vm.count("bundles")) {
//			config->bundles_per_epoch = vm["bundles"].as<unsigned long>();
//		} else {
//			config->bundles_per_epoch = 4 * config->cores;
//		}
//
//		if (vm.count("records")) {
//			config->records_per_epoch = vm["records"].as<unsigned long>();
//		} else {
//			config->records_per_epoch =
//					config->bundles_per_epoch * CONFIG_DEFAULT_BUNDLE_SIZE;
//		}
//
//		if (vm.count("input_file")) {
//			config->input_file = vm["input_file"].as<vector<string>>()[0];
//		}
	}
	catch(exception& e) {
		cerr << "error: " << e.what() << "\n";
		abort();
	}
	catch(...) {
		cerr << "Exception of unknown type!\n";
		abort();
	}
}

/* one time ops */
void init_decoder(void)
{
	av_register_all();
	avcodec_register_all();
}

#undef _GLIBCXX_DEBUG /* does not get along with program options lib */

int main(int ac, char * av[])
{
	parse_options(ac, av, &the_config);

	init_decoder();

	zmq::context_t context (1 /* # of io threads */);

	// for incoming encoded chunks
	zmq::socket_t  recver(context, ZMQ_PULL);
//	recver.bind(SERVER_PULL_ADDR);
	recver.connect(WORKER_PULL_FROM_ADDR);
	EE("connected to source (chunks)");

	// for outgoing decoded frames
	zmq::socket_t  sender(context, ZMQ_PUSH);
//	sender.bind(SERVER_PUSH_ADDR);
	sender.connect(WORKER_PUSH_TO_ADDR);
	EE("connected to sink (frames)");

	// for feedback
	zmq::socket_t fb_sender(context, ZMQ_PUSH);
	fb_sender.connect(WORKER_PUSHFB_TO_ADDR);
	EE("connected to fb");

//	I("bound to sockets...");
	int fb_cnt = 0;

//	for (int k = 0; k < 20; k++) {
	while (1) {
		/* test with local file */
		if (!the_config.use_hw) {
			chunk_desc desc;
			//		const char * fname = "/tmp/data.file";
			//		recv_one_chunk_tofile(recver, &desc, fname);
			//		decode_one_file_sw(fname, sender, desc);
			decode_one_file_sw(H264_FILE, sender, desc);
		} else {
			chunk_desc desc; /* XXX fill it XXX */
#if 1
//			char fname[] = "/tmp/XXXXXX"; /* template */
			char fname[] = "/shared/tmp/XXXXXX"; /* template */
			auto ret = mkstemp(fname);
			xzl_bug_on(ret == -1);
			recv_one_chunk_tofile(recver, &desc, fname);
			decode_one_file_hw(fname, sender, desc);

			ret = unlink(fname); /* clean up tmp file */
			xzl_bug_on(ret != 0);
#endif

			/* send some fb back */
			feedback fb(desc.id, fb_cnt++);
			send_one_fb(fb, fb_sender);

			I("sent one fb");
		}
		k2_measure_flush();
	}

	return 0;
}

