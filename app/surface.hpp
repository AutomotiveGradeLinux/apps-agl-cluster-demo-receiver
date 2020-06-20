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

#include "shell-desktop.h"

class SurfaceHandler
{
public:
	SurfaceHandler(const int port, const std::string &token, const std::string &role);
	void set_bounding_box(int x, int y, int bx, int by, int width, int height);

private:
	int m_port;
	std::string m_token;
	std::string m_role;

	int init_agl_shell(void);

	struct agl_shell_desktop *shell;
	Shell *aglShell;
};

#endif // SURFACE_HPP
