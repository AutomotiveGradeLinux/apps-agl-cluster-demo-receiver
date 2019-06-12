/*
 * Copyright (C) 2018 Konsulko Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <iostream>
#include <cstring>
#include <gst/gst.h>
#include "surface.hpp"

#undef DEBUG

int main(int argc, char *argv[])
{
	int port = 0;
	std::string token;
	std::string role = "receiver";

	try {
		port = std::stol(argv[1]);
		token = argv[2];
	} catch (const std::invalid_argument& e) {
		std::cerr << "Invalid argument" << std::endl;
		exit(1);
	} catch (const std::out_of_range& e) {
		std::cerr << "Port out of range" << std::endl;
		exit(1);
	}

	// NOTES:
	// (1) For reference, the pipeline used is based on the gst-launch-1.0 command in the ad hoc unit
        //     file in the previous hand-rolled CES demo:
	//
	//     udpsrc port=5005 ! application/x-rtp,media=video,encoding-name=H264 ! queue ! rtph264depay ! h264parse config-interval=1 disable-passthrough=true ! decodebin ! vaapisink
	//
	// (2) waylandsink is a bit broken, as it needs a RGB format, but its caps include non-RGB formats
	//     This results in crashes when videoconvert doesn't end up in the pipeline, so care must be taken
	//     if it is used, e.g. for testing:
	//
	//     videotestsrc pattern=smpte ! video/x-raw,format=BGRx,width=384,height=368 ! waylandsink
	//

	std::string pipeline_str = \
		"rtpbin name=rtpbin udpsrc caps=\"application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=JPEG,payload=26\" port=5005 !    rtpbin.recv_rtp_sink_0 rtpbin. ! rtpjpegdepay ! jpegdec ! ";
	GstPlugin *plugin = gst_registry_find_plugin(gst_registry_get (), "vaapisink");
	if(plugin) {
		pipeline_str += "vaapisink";
		gst_object_unref(plugin);
	} else {
		pipeline_str += "waylandsink";
	}
#ifdef DEBUG
	std::cout << "Using pipeline: " << pipeline_str << std::endl;
#endif

        // Initialize GStreamer
#ifdef DEBUG
	int gargc = 2;
	char **gargv = (char**) malloc(2 * sizeof(char*));
	gargv[0] = strdup(argv[0]);
	gargv[1] = strdup("--gst-debug-level=2");
	gst_init(&gargc, &gargv);
#else
        gst_init(NULL, NULL);
#endif

	// Create our IVI surface handler
	SurfaceHandler handler(port, token, role);

	GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), NULL);
	if(!pipeline) {
		std::cerr << "gstreamer pipeline construction failed!" << std::endl;
		exit(1);
	}

	// Start pipeline
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	std::cout << "gstreamer pipeline running" << std::endl;

	// Wait until error or EOS
	GstBus *bus = gst_element_get_bus(pipeline);
	GstMessage *msg = gst_bus_timed_pop_filtered(bus,
						     GST_CLOCK_TIME_NONE,
						     (GstMessageType) (GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

	// Free resources
	if(msg != NULL)
		gst_message_unref(msg);
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);

	return 0;
}
