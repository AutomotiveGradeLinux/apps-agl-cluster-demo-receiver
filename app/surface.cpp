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

#include <QGuiApplication>
#include <QtGui/QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include <QObject>
#include <QString>
#include <QScreen>
#include <QWindow>

#include "agl-shell-desktop-client-protocol.h"

static void
global_add(void *data, struct wl_registry *reg, uint32_t name,
		const char *interface, uint32_t version)
{
	struct agl_shell_desktop **shell =
		static_cast<struct agl_shell_desktop **>(data);

	if (strcmp(interface, agl_shell_desktop_interface.name) == 0) {
		*shell = static_cast<struct agl_shell_desktop *>(
			wl_registry_bind(reg, name, &agl_shell_desktop_interface, version)
		);
	}
}

static void
global_remove(void *data, struct wl_registry *reg, uint32_t id)
{
	(void) data;
	(void) reg;
	(void) id;
}

static const struct wl_registry_listener registry_listener = {
	global_add,
	global_remove,
};


static void
application_id_event(void *data, struct agl_shell_desktop *agl_shell_desktop,
		const char *app_id)
{

}

static void
application_id_state(void *data, struct agl_shell_desktop *agl_shell_desktop,
		const char *app_id, const char *app_data,
		uint32_t app_state, uint32_t app_role)
{
	(void) app_data;
	(void) agl_shell_desktop;
	(void) app_id;
	(void) app_state;
	(void) app_role;
}

static const struct agl_shell_desktop_listener agl_shell_desk_listener = {
	application_id_event,
	application_id_state,
};

static struct agl_shell_desktop *
register_agl_shell_desktop(QPlatformNativeInterface *native)
{
	struct wl_display *wl;
	struct wl_registry *registry;
	struct agl_shell_desktop *shell = nullptr;

	wl = static_cast<struct wl_display *>(native->nativeResourceForIntegration("display"));
	registry = wl_display_get_registry(wl);

	wl_registry_add_listener(registry, &registry_listener, &shell);
	// Roundtrip to get all globals advertised by the compositor
	wl_display_roundtrip(wl);
	wl_registry_destroy(registry);

	return shell;
}

SurfaceHandler::SurfaceHandler(const int port, const std::string &token, const std::string &role)
{
	m_port = port;
	m_token = token;
	m_role = role;

	if (init_agl_shell()) {
		exit(1);
	}

	agl_shell_desktop_add_listener(shell, &agl_shell_desk_listener, this);
}

int
SurfaceHandler::init_agl_shell(void)
{
	QPlatformNativeInterface *native = qApp->platformNativeInterface();
	shell = register_agl_shell_desktop(native);
	if (!shell) {
		return -1;
	}

	std::shared_ptr<struct agl_shell_desktop>
		agl_shell{shell, agl_shell_desktop_destroy};

	aglShell = new Shell(agl_shell, nullptr);

	return 0;
}

void
SurfaceHandler::set_bounding_box(int x, int y, int bx, int by, int width, int height)
{
	QString app_id = QString::fromUtf8(m_role.c_str(), -1);
	aglShell->set_window_props(app_id, AGL_SHELL_DESKTOP_APP_ROLE_POPUP,
				   x, y, bx, by, width, height);
}
