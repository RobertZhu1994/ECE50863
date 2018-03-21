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
MotionDetector motiondetector[64];
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

atomic<int> mycount(0);

std::vector<AlprRegionOfInterest> detectmotion( Alpr* alpr, cv::Mat frame, std::string region, bool writeJson, int proc_id, int f_seq)
//bool detectandshow( Alpr* alpr, cv::Mat frame, std::string region, bool writeJson)
{
    timespec startTime;
    timespec endTime;

    mycount++;

    std::vector<AlprRegionOfInterest> regionsOfInterest;

    int ratio = 720 / 180;

    /*
    if (do_motiondetection)
    {
        cout << "motion detection" << endl;
        cv::Rect rectan = motiondetector.MotionDetect(&frame);
        if (rectan.width>0) regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
    }
    else regionsOfInterest.push_back(AlprRegionOfInterest(0, 0, frame.cols, frame.rows));
    */

    /* Motion detection phase */
    cv::Rect rectan = motiondetector[proc_id].MotionDetect(&frame);
    if (rectan.width > 0) {
        //cout << "here" << endl;
        //regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
        regionsOfInterest.push_back(AlprRegionOfInterest(ratio*rectan.x, ratio*rectan.y, ratio*rectan.width, ratio*rectan.height));
    }

    //cout << rectan.x << " " << rectan.y << " " << rectan.width << " " << rectan.height << " " << endl;
    //std::cout << "Motion Detection: " << diffclock(startTime, endTime) << "ms." << std::endl;

    return regionsOfInterest;
}

bool recognizeplate( Alpr* alpr, cv::Mat frame_720, std::string region, bool writeJson, int proc_id, int f_seq, std::vector<AlprRegionOfInterest> regionsOfInterest){

    AlprResults results;

    /* Recognition phase */
    //getTimeMonotonic(&startTime);
    results = alpr->recognize(frame_720.data, frame_720.elemSize(), frame_720.cols, frame_720.rows, regionsOfInterest);

    /* print the ROI */
    cout << regionsOfInterest[0].x << " " << regionsOfInterest[0].y << " " << regionsOfInterest[0].width << " " << regionsOfInterest[0].height << " " << endl;

    //getTimeMonotonic(&endTime);
    //std::cout << "Total Plate Recognition: " << diffclock(startTime, endTime) << "ms." << std::endl;

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

void send_request(int db_seq, int total_fnum, zmq::socket_t &sender){

}

int main (int argc, char *argv[])
{
	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t receiver(context, ZMQ_PULL);
//	receiver.connect(CLIENT_PULL_FROM_ADDR);
	receiver.bind(FRAME_PULL_ADDR);

    zmq::socket_t rq_sender(context, ZMQ_PUSH);
    rq_sender.bind(REQUEST);

    EE("bound to %s. wait for workers to push ...", FRAME_PULL_ADDR);

	CallBackTimer stat_collector;
	RxManager mg;

    zmq::socket_t rq720_sender(context, ZMQ_PUSH);
    rq720_sender.bind(REQUEST720);

	//stat_collector.start(200 /*ms, check interval */, &getStatistics);

	/* consumption phase */
	//Frame f;
	int rc;
	seq_t wm_c, wm_f;

    /* teddyxu: alpr starts here */
    //std::vector<std::string> filenames;
    std::string configFile = "";
    bool outputJson = false;
    //int seektoms = 0;
    //bool detectRegion = false;
    std::string country;
    int topn;

    timespec startTime;
    timespec endTime;
    //bool debug_mode = false;

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
        //seektoms = seekToMsArg.getValue();
        outputJson = jsonSwitch.getValue();
        //debug_mode = debugSwitch.getValue();
        configFile = configFileArg.getValue();
        //detectRegion = detectRegionSwitch.getValue();
        templatePattern = templatePatternArg.getValue();
        topn = topNArg.getValue();
        measureProcessingTime = clockSwitch.getValue();
        do_motiondetection = motiondetect.getValue();
    }
    catch (TCLAP::ArgException &e)    // catch any exceptions
    {
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
        //return 1;
    }

    //Alpr alpr(country, configFile);
    //alpr.setTopN(topn);

    Alpr *alpr[64];

    for(int m = 0; m < 64; m++){
        alpr[m] = new Alpr(country, configFile);
        alpr[m]->setTopN(topn);
    }

    int height = 180;
    int width = 320;

    //Frame frame[72005];
    int framesize = height * width;

    int cont_flag = 0;
    //for(int iter = 0; iter < 3; iter++) {
    while(1){
        Frame f;
        Frame frame[72005];
        Frame ff[64];

        request_desc rd;
        zmq::message_t rq_desc(sizeof(request_desc));

        if (cont_flag == 1)
            break;

        EE("bound to %s. Enter your target database: (1-n)", REQUEST);
        cin >> rd.db_seq;

        EE("bound to %s. Enter start frame number: ", REQUEST);
        cin >> rd.start_fnum;

        EE("bound to %s. Enter total frame numbers: ", REQUEST);
        cin >> rd.total_fnum;

        /* Sending requests */
        memcpy(rq_desc.data(), &rd, sizeof(request_desc));
        rq_sender.send(rq_desc, ZMQ_PUSH);

        I("Request sent, db: %d, frames: %d", rd.db_seq, rd.total_fnum);

        I("Doing this again???????????????");

        /* Receiving frames */
        for(int iter = 0; iter < rd.total_fnum; iter++) {
        //while(1){
            data_desc desc;
            auto msg_ptr = recv_one_frame(receiver, &desc);

            if (!msg_ptr) {
                mg.DepositADesc(desc);
                I("got desc: %s", desc.to_string().c_str());
                if (desc.type == TYPE_CHUNK_EOF || desc.type == TYPE_RAW_FRAME_EOF) {
                    break;
                }
            } else {
                mg.DepositAFrame(desc, msg_ptr);
                //stat.inc_byte_counter((int) msg_ptr->size());
                //stat.inc_rec_counter(1);
            }
        }

        k2_measure("retrieve begins");
        /* Retrieving frames */
        for(int i = rd.start_fnum; i < rd.start_fnum + rd.total_fnum; i++) {
            mg.GetWatermarks(&wm_c, &wm_f);

            I("watermarks:  chunk %u frame %u", wm_c, wm_f);
            rc = mg.RetrieveAFrame(&f);

            frame[i] = f;

            //memcpy(frame[i], &f, sizeof(shared_ptr));
            if (rc == VS_ERR_EOF_CHUNKS)
                break;

            //xzl_bug_on(rc != 0);
        }
        k2_measure("retrieve ends");

        //Frame frame_720p[72000];
        int height_720 = 720;
        int width_720 = 1280;
        int framesize_720 = height_720 * width_720;

        omp_lock_t writelock;
        omp_init_lock(&writelock);

        /* Processing Frames*/
        k2_measure("alpr begins");

        //#pragma omp parallel
        #pragma omp parallel for schedule(dynamic, 100) num_threads(30)
        for (int j = rd.start_fnum; j < rd.start_fnum + rd.total_fnum; j++) {
            int id = omp_get_thread_num();
            cv::Mat frm;
            char *imagedata = NULL;

            //char *imagedata = (char *) malloc(sizeof(char) * framesize);
            imagedata = static_cast<char *>(frame[j].msg_p->data()), frame[j].msg_p->size();
            //imagedata[id] = (char *)(frame[id*1440+j].msg_p->data()), frame[id*1440+j].msg_p->size();

            frm.create(height, width, CV_8UC1);
            memcpy(frm.data, imagedata, framesize);

            //bool plate_found = detectandshow(&alpr, frm, "", outputJson, id);
            std::vector<AlprRegionOfInterest> regionsOfInterest = detectmotion(alpr[id], frm, "", outputJson, id, j);

            sleep(1);
            //omp_set_lock(&writelock);

            zmq::message_t rq_desc720(sizeof(request_desc));
            request_desc rd_one_frame;

            if (regionsOfInterest.size()>0){
                //zmq::message_t rq_desc720(sizeof(request_desc));
                //request_desc rd_one_frame;

                EE("bound to %s. ready to push 720 requests", REQUEST720);

                //Specify the frame wants to retrieve
                rd_one_frame.start_fnum = j;
                rd_one_frame.db_seq = HDD_RAW_720;
                rd_one_frame.total_fnum = 1;

                cv::Mat frm_720;
                Frame f720;

                omp_set_lock(&writelock);

                memcpy(rq_desc720.data(), &rd_one_frame, sizeof(request_desc));
                rq720_sender.send(rq_desc720, ZMQ_PUSH);

                I("Request sent, db: %d, frame: %d", rd_one_frame.db_seq, rd_one_frame.start_fnum);

                for(int dc = 0; dc < 1; dc++) {
                    I("start to get 720p frames");
                    data_desc desc720;
                    auto msg_ptr = recv_one_frame(receiver, &desc720);
                    //I("msg_ptr = %lu", msg_ptr);
                    //cout << desc.c_seq << endl;
                    if (!msg_ptr) {
                        mg.DepositADesc(desc720);
                        I("got desc: %s", desc720.to_string().c_str());
                    }
                    else {
                        I("got 720p, deposit a frame");
                        mg.DepositAFrame(desc720, msg_ptr);
                    }
                }
/*
                mg.GetWatermarks(&wm_c, &wm_f);
                I("watermarks:  chunk %u frame %u", wm_c, wm_f);
                mg.RetrieveAFrame(&f);
                //mg.RetrieveAFrame(&f);
*/
                for(int i = rd_one_frame.start_fnum; i < rd_one_frame.start_fnum + rd_one_frame.total_fnum; i++) {
                    mg.GetWatermarks(&wm_c, &wm_f);

                    I("watermarks:  chunk %u frame %u", wm_c, wm_f);
                    rc = mg.RetrieveAFrame(&f);

                    ff[id] = f;

                    //memcpy(frame[i], &f, sizeof(shared_ptr));
                    if (rc == VS_ERR_EOF_CHUNKS)
                        break;

                    //xzl_bug_on(rc != 0);
                }

                char *imagedata_720 = NULL;
                //imagedata_720 = static_cast<char *>(ff[id].msg_p->data()), ff[id].msg_p->size();
                imagedata_720 = (char*)(ff[id].msg_p->data()), ff[id].msg_p->size();

                frm_720.create(height_720, width_720, CV_8UC1);
                memcpy(frm_720.data, imagedata_720, framesize_720);

                omp_unset_lock(&writelock);

                getTimeMonotonic(&startTime);
                bool plate_found = recognizeplate(alpr[id], frm_720, "", outputJson, id, j, regionsOfInterest);
                if (!plate_found && !outputJson) {
                    std::cout << "No license plates found." << std::endl;
                }
                getTimeMonotonic(&endTime);
                std::cout << "Plate Recognition: " << diffclock(startTime, endTime) << "ms." << std::endl;

            }
            //omp_unset_lock(&writelock);
        }
        omp_destroy_lock(&writelock);
        k2_measure("alpr ends");
        k2_measure_flush();
        cout << "count = " << mycount << endl;
    }
    //}
	/* XXX stop the stat collector */
	stat_collector.stop();

	return 0;
}
