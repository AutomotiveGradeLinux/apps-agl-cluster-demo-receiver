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

#include <cstdio>
#include <cstdarg>
#include "surface.hpp"
#include "hmi-debug.h"

#define AREA_NORMAL_FULL "normal.full"

SurfaceHandler::SurfaceHandler(const int port, const std::string &token, const std::string &role)
{
	m_port = port;
	m_token = token;
	m_role = role;

	m_rid = getpid();

	// Setup HomeScreen/WindowManager API
	if (init_wm()) {
		HMI_ERROR("receiver:sh", "cannot setup windowmanager API");
		exit(1);
	}

	// Setup ilmController API
	m_ic = new ILMControl(notify_ivi_control_cb_static, this);
	if (!m_ic) {
		HMI_ERROR("receiver:sh", "cannot setup IVI layer management API");
		exit(1);
	}
}

int SurfaceHandler::init_wm(void)
{
	m_wm = new LibWindowmanager();
	if (m_wm->init(m_port, m_token.c_str())) {
		HMI_ERROR("receiver:sh", "cannot initialize windowmanager");
		return -1;
	}

	std::function< void(json_object*) > h_active =
		[](json_object* object) {
			HMI_DEBUG("receiver:sh", "Got Event_Active");
		};

	std::function< void(json_object*) > h_inactive =
		[](json_object* object) {
			HMI_DEBUG("receiver:sh", "Got Event_Inactive");
		};

	std::function< void(json_object*) > h_visible =
		[](json_object* object) {
			HMI_DEBUG("receiver:sh", "Got Event_Visible");
		};

	std::function< void(json_object*) > h_invisible =
		[](json_object* object) {
			HMI_DEBUG("receiver:sh", "Got Event_Invisible");
		};

	std::function< void(json_object*) > h_syncdraw =
		[this](json_object* object) {
			HMI_DEBUG("receiver:sh", "Got Event_SyncDraw");
			this->m_wm->endDraw(this->m_role.c_str());
		};

	std::function< void(json_object*) > h_flushdraw =
		[](json_object* object) {
			HMI_DEBUG("receiver:sh", "Got Event_FlushDraw");
		};

	m_wm->set_event_handler(LibWindowmanager::Event_Active, h_active);
	m_wm->set_event_handler(LibWindowmanager::Event_Inactive, h_inactive);
	m_wm->set_event_handler(LibWindowmanager::Event_Visible, h_visible);
	m_wm->set_event_handler(LibWindowmanager::Event_Invisible, h_invisible);
	m_wm->set_event_handler(LibWindowmanager::Event_SyncDraw, h_syncdraw);
	m_wm->set_event_handler(LibWindowmanager::Event_FlushDraw, h_flushdraw);

	return 0;
}

void SurfaceHandler::notify_ivi_control_cb(ilmObjectType object,
					   t_ilm_uint id,
					   t_ilm_bool created)
{
	if (object == ILM_SURFACE) {
		struct ilmSurfaceProperties surf_props;

		ilm_getPropertiesOfSurface(id, &surf_props);
		pid_t surf_pid = surf_props.creatorPid;

		if (!created) {
			HMI_DEBUG("receiver:sh", "ivi surface (id=%d, pid=%d) destroyed.", id, surf_pid);
			unregister_surfpid(surf_pid);
			m_surfaces.erase(surf_pid);
			return;
		}

		HMI_DEBUG("receiver:sh", "ivi surface (id=%d, pid=%d) is created.", id, surf_pid);

		register_surfpid(surf_pid);
		if (m_rid && surf_pid == find_surfpid_by_rid(m_rid)) {
			setup_surface(id);
		}
		m_surfaces[surf_pid] = id;
	} else if (object == ILM_LAYER) {
		if (created)
			HMI_DEBUG("receiver:sh", "ivi layer: %d created.", id);
		else
			HMI_DEBUG("receiver:sh", "ivi layer: %d destroyed.", id);
	}

}

void SurfaceHandler::notify_ivi_control_cb_static (ilmObjectType object,
						   t_ilm_uint id,
						   t_ilm_bool created,
						   void *user_data)
{
	SurfaceHandler *handler = static_cast<SurfaceHandler*>(user_data);
	handler->notify_ivi_control_cb(object, id, created);
}

void SurfaceHandler::register_surfpid (pid_t surf_pid)
{
	if (surf_pid == m_rid) {
		if (!std::count(m_pid_v.begin(), m_pid_v.end(), surf_pid)) {
			HMI_DEBUG("receiver:sh", "surface creator(pid=%d) registered", surf_pid);
			m_pid_v.push_back(surf_pid);
			HMI_DEBUG("receiver:sh", "m_pid_v.count(%d) = %d", surf_pid,
				  std::count(m_pid_v.begin(), m_pid_v.end(), surf_pid));
		}
	}
}

void SurfaceHandler::unregister_surfpid (pid_t surf_pid)
{
	auto itr = m_pid_v.begin();
	while (itr != m_pid_v.end()) {
		if (*itr == surf_pid) {
			m_pid_v.erase(itr++);
		} else {
			++itr;
		}
	}
}

pid_t SurfaceHandler::find_surfpid_by_rid (pid_t rid)
{
	HMI_DEBUG("receiver:sh", "find surfpid by rid(%d)", rid);
	if (std::count(m_pid_v.begin(), m_pid_v.end(), rid)) {
		HMI_DEBUG("receiver:sh", "found return(%d)", rid);
		return rid;
	}

	return -1;
}

void SurfaceHandler::setup_surface (int id)
{
	std::string sid = std::to_string(id);

	// This surface is mine, register pair app_name and ivi id.
	HMI_DEBUG("receiver:sh", "requestSurfaceXDG(%s,%d)", m_role.c_str(), id);
	m_wm->requestSurfaceXDG(m_role.c_str(), id);

	if (m_pending_create) {
		// Activate window if it has not been yet
		HMI_DEBUG("receiver:sh", "calling activateWindow on (%s,%d)", m_role.c_str(), id);
		m_pending_create = false;
		m_wm->activateWindow(m_role.c_str(), AREA_NORMAL_FULL);
	}
}
