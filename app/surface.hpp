/*
 * Copyright (c) 2017 Panasonic Corporation
 * Copyright (c) 2018 Konsulko Group
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SURFACE_HPP
#define SURFACE_HPP

#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include <ilm/ilm_control.h> 
#include <libwindowmanager.h>

class ILMControl
{
public:
	ILMControl(notificationFunc callback, void *user_data) {
		ilm_init();
		ilm_registerNotification(callback, user_data);
	}

	~ILMControl(void) {
		ilm_unregisterNotification();
		ilm_destroy();
	}
};

class SurfaceHandler
{
public:
	SurfaceHandler(const int port, const std::string &token, const std::string &role);

	void notify_ivi_control_cb(ilmObjectType object,
				   t_ilm_uint id,
				   t_ilm_bool created);

	static void notify_ivi_control_cb_static (ilmObjectType object,
						  t_ilm_uint id,
						  t_ilm_bool created,
						  void *user_data);

private:
	int m_port;
	std::string m_token;

	std::string m_role;

	int m_rid = 0;

	LibWindowmanager *m_wm;
	ILMControl *m_ic;

	std::vector<pid_t> m_pid_v;

	std::map<int, int> m_surfaces;  // pair of <afm:rid, ivi:id>

	//bool m_pending_create = false;
	bool m_pending_create = true;

	// Private functions

	int init_wm(void);

	void register_surfpid(pid_t surf_pid);
	void unregister_surfpid(pid_t surf_pid);
	pid_t find_surfpid_by_rid(pid_t rid);
	void setup_surface(int id);
};

#endif // SURFACE_HPP
