//
// Created by xzl on 12/24/17.
//
// tester: pull frames from a

/*
 * Copyright (c) 2015 OpenALPR Technology, Inc.
 * Open source Automated License Plate Recognition [http://www.openalpr.com]
 *
 * This file is part of OpenALPR.
 *
 * OpenALPR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License
 * version 3 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef DEBUG
#define GLIBCXX_DEBUG 1 // debugging
#endif

#include <sstream> // std::ostringstream

#include <zmq.hpp>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>

#include "config.h"
#include "vs-types.h"
#include "mydecoder.h"
#include "log.h"
#include "rxtx.h"
#include "RxManager.h"
#include "measure.h"

#include "StatCollector.h"
#include "CallBackTimer.h"

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "tclap/CmdLine.h"
#include "openalpr/support/filesystem.h"
#include "openalpr/support/timing.h"
#include "openalpr/support/platform.h"
#include "video/videobuffer.h"
#include "openalpr/motiondetector.h"
#include "openalpr/alpr.h"

extern "C"{
#include <omp.h>
};

using namespace vs;
using namespace alpr;

const std::string MAIN_WINDOW_NAME = "ALPR main window";

const bool SAVE_LAST_VIDEO_STILL = false;
const std::string LAST_VIDEO_STILL_LOCATION = "/tmp/laststill.jpg";
const std::string WEBCAM_PREFIX = "/dev/video";
//MotionDetector motiondetector;
bool do_motiondetection = true;

/** Function Headers */
bool detectandshow(Alpr* alpr, cv::Mat frame, std::string region, bool writeJson);
bool is_supported_image(std::string image_file);

bool measureProcessingTime = true;
std::string templatePattern;

// This boolean is set to false when the user hits terminates (e.g., CTRL+C )
// so we can end infinite loops for things like video processing.
bool program_active = true;

/*
static StatCollector stat;

static void getStatistics(void) {
	Statstics s;
	if (!stat.ReportStatistics(&s))
		return;

	if (s.rec_persec < 1) // nothing?
		return;

	printf("%.2f MB/sec \t %.0f fps \n", s.byte_persec/1024/1024, s.rec_persec);
};
*/

/* pull the source to get info for all streams */
void getAllStreamInfo() {



}

bool detectandshow( Alpr* alpr, cv::Mat frame, std::string region, bool writeJson, int chunk_id)
{

    MotionDetector motiondetector[4];
    timespec startTime;
    //getTimeMonotonic(&startTime);

    std::vector<AlprRegionOfInterest> regionsOfInterest;
    /*
    if (do_motiondetection)
    {
        cout << "motion detection" << endl;
        cv::Rect rectan = motiondetector.MotionDetect(&frame);
        if (rectan.width>0) regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
    }
    else regionsOfInterest.push_back(AlprRegionOfInterest(0, 0, frame.cols, frame.rows));
    */
    //k2_measure("motion detection start");
    /* Motion detection phase */
    cv::Rect rectan = motiondetector[chunk_id].MotionDetect(&frame);
    //k2_measure("motion detection end");
    if (rectan.width>0) regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));

    timespec endTime;
    cout << "ROI size = " << regionsOfInterest.size() << endl;
    //getTimeMonotonic(&endTime);
    //k2_measure_flush();
    cout << rectan.x << " " <<  rectan.y << " " << rectan.width << " " << rectan.height << " " << endl;
    //std::cout << "Motion Detection: " << diffclock(startTime, endTime) << "ms." << std::endl;

    /* Recognition phase */
    getTimeMonotonic(&startTime);
    AlprResults results;
    if (regionsOfInterest.size()>0) results = alpr->recognize(frame.data, frame.elemSize(), frame.cols, frame.rows, regionsOfInterest);
    getTimeMonotonic(&endTime);
    //double totalProcessingTime = diffclock(startTime, endTime);

    std::cout << "Total Plate Recognition: " << diffclock(startTime, endTime) << "ms." << std::endl;

    /*
    if (measureProcessingTime) {
        std::cout << "Total Time to process image: " << totalProcessingTime << "ms." << std::endl;
    }
    */

    if (writeJson)
    {
        std::cout << alpr->toJson( results ) << std::endl;
    }
    else
    {
        for (int i = 0; i < results.plates.size(); i++)
        {
            std::cout << "plate" << i << ": " << results.plates[i].topNPlates.size() << " results";
            if (measureProcessingTime)
                std::cout << " -- Processing Time = " << results.plates[i].processing_time_ms << "ms.";
            std::cout << std::endl;

            if (results.plates[i].regionConfidence > 0)
                std::cout << "State ID: " << results.plates[i].region << " (" << results.plates[i].regionConfidence << "% confidence)" << std::endl;

            for (int k = 0; k < results.plates[i].topNPlates.size(); k++)
            {
                // Replace the multiline newline character with a dash
                std::string no_newline = results.plates[i].topNPlates[k].characters;
                std::replace(no_newline.begin(), no_newline.end(), '\n','-');

                std::cout << "    - " << no_newline << "\t confidence: " << results.plates[i].topNPlates[k].overall_confidence;
                if (templatePattern.size() > 0 || results.plates[i].regionConfidence > 0)
                    std::cout << "\t pattern_match: " << results.plates[i].topNPlates[k].matches_template;

                std::cout << std::endl;
            }
        }
    }

    return results.plates.size() > 0;
}

int main (int argc, char *argv[])
{
	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t receiver(context, ZMQ_PULL);
//	receiver.connect(CLIENT_PULL_FROM_ADDR);
	receiver.bind(FRAME_PULL_ADDR);

	EE("bound to %s. wait for workers to push ...", FRAME_PULL_ADDR);

	CallBackTimer stat_collector;
	RxManager mg;

	//stat_collector.start(200 /*ms, check interval */, &getStatistics);

//	size_t sz = 1;
	while (1) {
#if 0
		unsigned seq = recv_one_frame(receiver, &sz);
		if (sz == 0) {
			EE("got end frame. final seq = %u", seq);
		}
#endif

		data_desc desc;
		auto msg_ptr = recv_one_frame(receiver, &desc);

		if (!msg_ptr) {
			mg.DepositADesc(desc);
			I("got desc: %s", desc.to_string().c_str());
			if (desc.type == TYPE_CHUNK_EOF || desc.type == TYPE_RAW_FRAME_EOF) {
                cout << "teddy: end of chunk" << endl;
                break;
            }
		} else {
			mg.DepositAFrame(desc, msg_ptr);
			//stat.inc_byte_counter((int) msg_ptr->size());
			//stat.inc_rec_counter(1);

            //cout << "inside the first loop" << endl;
        }
	}

	cout << "after the first loop" << endl;
	/* consumption phase */
	Frame f;
	int rc;
	seq_t wm_c, wm_f;
	while (true) {

            /* teddyxu: alpr starts here */
            //std::vector<std::string> filenames;
            std::string configFile = "";
            bool outputJson = false;
            int seektoms = 0;
            bool detectRegion = false;
            std::string country;
            int topn;
            bool debug_mode = false;

            TCLAP::CmdLine cmd("OpenAlpr Command Line Utility", ' ', Alpr::getVersion());

            TCLAP::UnlabeledMultiArg<std::string> fileArg("image_file", "Image containing license plates", true, "",
                                                          "image_file_path");


            TCLAP::ValueArg<std::string> countryCodeArg("c", "country",
                                                        "Country code to identify (either us for USA or eu for Europe).  Default=us",
                                                        false, "us", "country_code");
            TCLAP::ValueArg<int> seekToMsArg("", "seek", "Seek to the specified millisecond in a video file. Default=0",
                                             false, 0, "integer_ms");
            TCLAP::ValueArg<std::string> configFileArg("", "config", "Path to the openalpr.conf file", false, "",
                                                       "config_file");
            TCLAP::ValueArg<std::string> templatePatternArg("p", "pattern",
                                                            "Attempt to match the plate number against a plate pattern (e.g., md for Maryland, ca for California)",
                                                            false, "", "pattern code");
            TCLAP::ValueArg<int> topNArg("n", "topn", "Max number of possible plate numbers to return.  Default=10",
                                         false, 10, "topN");

            TCLAP::SwitchArg jsonSwitch("j", "json", "Output recognition results in JSON format.  Default=off", cmd,
                                        false);
            TCLAP::SwitchArg debugSwitch("", "debug", "Enable debug output.  Default=off", cmd, false);
            TCLAP::SwitchArg detectRegionSwitch("d", "detect_region",
                                                "Attempt to detect the region of the plate image.  [Experimental]  Default=off",
                                                cmd, false);
            TCLAP::SwitchArg clockSwitch("", "clock",
                                         "Measure/print the total time to process image and all plates.  Default=off",
                                         cmd, false);
            TCLAP::SwitchArg motiondetect("", "motion", "Use motion detection on video file or stream.  Default=off",
                                          cmd, false);


            try {
                cmd.add(templatePatternArg);
                cmd.add(seekToMsArg);
                cmd.add(topNArg);
                cmd.add(configFileArg);
                cmd.add(fileArg);
                cmd.add(countryCodeArg);

                //filenames = fileArg.getValue();

                country = countryCodeArg.getValue();
                seektoms = seekToMsArg.getValue();
                outputJson = jsonSwitch.getValue();
                debug_mode = debugSwitch.getValue();
                configFile = configFileArg.getValue();
                detectRegion = detectRegionSwitch.getValue();
                templatePattern = templatePatternArg.getValue();
                topn = topNArg.getValue();
                measureProcessingTime = clockSwitch.getValue();
                do_motiondetection = motiondetect.getValue();
            }
            catch (TCLAP::ArgException &e)    // catch any exceptions
            {
                std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
                return 1;
            }

        int chunk[4] = {0};
        //timespec startTime;
        //getTimeMonotonic(&startTime);
        #pragma omp parallel num_threads(4)
        {
            int id = omp_get_thread_num();
            //mg.GetWatermarks(&wm_c, &wm_f);

            //I("proc %d ", id);
            while (chunk[id] < 75) {
                wm_c = 0;
                wm_f = id * 75 + chunk[id];

                //mg.GetWatermarks(&wm_c, &wm_f);
                I("watermarks:  chunk %u frame %u proc %d", wm_c, wm_f, id);
                rc = mg.RetrieveAFrame(&f);

                cv::Mat frame;
                Alpr alpr(country, configFile);
                alpr.setTopN(topn);

                int framesize = 320 * 180;
                char *imagedata = NULL;

                imagedata = (char *) malloc(sizeof(char) * framesize);

                imagedata = static_cast<char *>(f.msg_p->data()), f.msg_p->size();

                frame.create(180, 320, CV_8UC1);

                memcpy(frame.data, imagedata, framesize);

                //cout << frame << endl;
                I("frame %d", wm_f);
                bool plate_found = detectandshow(&alpr, frame, "", outputJson, id);

                if (!plate_found && !outputJson)
                    std::cout << "No license plates found." << std::endl;


                //if (rc == VS_ERR_EOF_CHUNKS)
                //break;
                xzl_bug_on(rc != 0);
                chunk[id] ++;
            }
        }
        //timespec endTime;
        //getTimeMonotonic(&endTime);
        //std::cout << "Total: " << diffclock(startTime, endTime) << "ms." << std::endl;
    }
	/* XXX stop the stat collector */
	stat_collector.stop();

	return 0;
}
