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
#include <QGuiApplication>
#include <QApplication>
#include <QtCore/QDebug>
#include <QtCore/QCommandLineParser>
#include <QtCore/QUrlQuery>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqml.h>
#include <QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtGui/QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <QTimer>

#define GST_USE_UNSTABLE_API

#include <QWidget>

#include <string>
#include <iostream>
#include <cstring>

#include <gst/gst.h>

#include <gst/video/videooverlay.h>
#include <gst/wayland/wayland.h>

#include "surface.hpp"

#define WINDOW_WIDTH_SIZE	640
#define WINDOW_HEIGHT_SIZE	720

#define WINDOW_WIDTH_POS_X	640
#define WINDOW_WIDTH_POS_Y	180

struct cluster_window_data {
	int x; int y;
	int width; int height;
};

struct cluster_receiver_data {
	QWidget *widget;
	QPlatformNativeInterface *native;

	GstElement *pipeline;
	GstWaylandVideo *wl_video;
	GstVideoOverlay *overlay;

	bool widget_has_buffer_mapped;
	struct cluster_window_data window_data;
};

static struct wl_surface *
getWlSurface(QPlatformNativeInterface *native, QWindow *window)
{
	void *surf = native->nativeResourceForWindow("surface", window);
	return static_cast<struct ::wl_surface *>(surf);
}


static void
error_cb(GstBus *bus, GstMessage *msg, gpointer user_data)
{
	struct cluster_receiver_data *d = static_cast<struct cluster_receiver_data *>(user_data);

	gchar *debug = NULL;
	GError *err = NULL;

	gst_message_parse_error(msg, &err, &debug);

	g_print("Error: %s\n", err->message);
	g_error_free(err);

	if (debug) {
		g_print("Debug details: %s\n", debug);
		g_free(debug);
	}

	gst_element_set_state(d->pipeline, GST_STATE_NULL);
}

static GstBusSyncReply
bus_sync_handler(GstBus *bus, GstMessage *message, gpointer user_data)
{
	struct cluster_receiver_data *d = static_cast<struct cluster_receiver_data *>(user_data);

	if (gst_is_wayland_display_handle_need_context_message(message)) {
		GstContext *context;
		struct wl_display *display_handle;

		display_handle =
			static_cast<struct wl_display *>(
				d->native->nativeResourceForIntegration("display")
			);

		context = gst_wayland_display_handle_context_new(display_handle);

		d->wl_video = GST_WAYLAND_VIDEO(GST_MESSAGE_SRC(message));
		gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);

		goto drop;
	} else if (gst_is_video_overlay_prepare_window_handle_message(message)) {
		struct wl_surface *window_handle;
		QWindow *win = d->widget->windowHandle();

		if (!d->widget_has_buffer_mapped) {
			d->widget->setVisible(true);
			d->widget_has_buffer_mapped = true;
		}

		/* GST_MESSAGE_SRC(message) will be the overlay object that we
		 * have to use. This may be waylandsink, but it may also be
		 * playbin. In the latter case, we must make sure to use
		 * playbin instead of waylandsink, because playbin resets the
		 * window handle and render_rectangle after restarting playback
		 * and the actual window size is lost */
		d->overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));
		window_handle = getWlSurface(d->native, win);

		g_print("setting window handle and size (%d x %d) w %d, h %d\n",
				d->window_data.x, d->window_data.y,
				d->window_data.width, d->window_data.height);

		gst_video_overlay_set_window_handle(d->overlay, (guintptr) window_handle);
		gst_video_overlay_set_render_rectangle(d->overlay,
				d->window_data.x, d->window_data.y,
				d->window_data.width, d->window_data.height);

		goto drop;
	}

	return GST_BUS_PASS;

drop:
	gst_message_unref(message);
	return GST_BUS_DROP;
}

int main(int argc, char *argv[])
{
	int port = 0;
	std::string token;
	std::string role = "receiver";
	QString my_role = "receiver";

	struct cluster_receiver_data receiver_data = {};
	struct cluster_window_data window_data = {};

	window_data.x = 0;
	window_data.y = 0;

	window_data.width = WINDOW_WIDTH_SIZE;
	window_data.height = WINDOW_HEIGHT_SIZE;

	receiver_data.window_data = window_data;

	int gargc = 2;
	char **gargv = (char**) malloc(2 * sizeof(char*));
	gargv[0] = strdup(argv[0]);
	gargv[1] = strdup("--gst-debug-level=2");

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
		"rtpbin name=rtpbin udpsrc caps=\"application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=JPEG,payload=26\" port=5005 ! rtpbin.recv_rtp_sink_0 rtpbin. ! rtpjpegdepay ! jpegdec ! ";
		//"rtpbin name=rtpbin udpsrc caps=\"application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=JPEG,payload=26\" port=5005 ! rtpbin.recv_rtp_sink_0 rtpbin. ! rtpjpegdepay ! jpegdec";
	GstPlugin *plugin = gst_registry_find_plugin(gst_registry_get(), "vaapisink");

	if (plugin) {
		std::cout << "Using va api sink" << std::endl;
		pipeline_str += "vaapisink";
		gst_object_unref(plugin);
	} else {
		std::cout << "Using wayland sink" << std::endl;
		pipeline_str += "waylandsink";
	}

	gst_init(&gargc, &gargv);

	std::cout << "Using pipeline: " << pipeline_str << std::endl;

	QApplication app(argc, argv);
	QPlatformNativeInterface *native = qApp->platformNativeInterface();
	receiver_data.native = native;

	// mark the application id -> my_role
	app.setDesktopFileName(my_role);

	SurfaceHandler handler(port, token, role);

	// we already have the app_id -> role this will set-up the x and y
	// position and a bounding box that will be used to clip out the
	// surface. Note, that in this example, the surface area is the same as
	// the bounding box
	handler.set_bounding_box(WINDOW_WIDTH_POS_X, WINDOW_WIDTH_POS_Y,
				 0, 0, WINDOW_WIDTH_SIZE, WINDOW_HEIGHT_SIZE);

	QWidget *widget = new QWidget;
	widget->resize(WINDOW_WIDTH_SIZE,
		       WINDOW_HEIGHT_SIZE);
	widget->setVisible(false);
	// this is necessary to trigger a buffer attach on the xdg surface;
	// waylandsink will use that surface to create a sub-surface and will
	// perform a commit on the parent one; without doing a show() here
	// QtWayland will not attach the buffer to the surface, and the parent
	// surface will not be 'committed'; most likely this require more
	// fiddling as to keep the buffer attached but not displayed or use
	// some kind of local alpha to display that cluster is showing
	widget->show();

	receiver_data.widget = widget;
	receiver_data.widget_has_buffer_mapped = true;

	GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), NULL);
	if (!pipeline) {
		std::cerr << "gstreamer pipeline construction failed!" << std::endl;
		exit(1);
	}
	receiver_data.pipeline = pipeline;

	GstBus *bus = gst_element_get_bus(pipeline);
	gst_bus_add_signal_watch(bus);

	g_signal_connect(bus, "message::error", G_CALLBACK(error_cb), &receiver_data);
	gst_bus_set_sync_handler(bus, bus_sync_handler, &receiver_data, NULL);
	gst_object_unref(bus);

	// Start the pipeline, giving Qt the time to create everything; not
	// waiting here will cause both Qt and Gstreamer to race, running as
	// different threads. So this gives Qt sufficient time to create the
	// Widget, the surface the buffer and attach it to the surface. The
	// drawback is that it will displayed until the remote side start
	// sending frames to decode.
	std::cout << "wainting for Qt to be ready..." << std::endl;
	QTimer::singleShot(500, [pipeline](){
		gst_element_set_state(pipeline, GST_STATE_PLAYING);
		std::cout << "gstreamer pipeline running" << std::endl;
	});

	// run the application
	int ret = app.exec();
	widget->hide();

	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
	return ret;
}
