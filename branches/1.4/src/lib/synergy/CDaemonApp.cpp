/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2012 Nick Bolton
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file COPYING that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CDaemonApp.h"

#if SYSAPI_WIN32
#include "CArchMiscWindows.h"
#include "XArchWindows.h"
#include "CScreen.h"
#include "CMSWindowsScreen.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "CSocketMultiplexer.h"
#include "CEventQueue.h"

#include <string>
#include <iostream>
#include <sstream>

using namespace std;

CDaemonApp* CDaemonApp::instance = NULL;

int
mainLoopStatic()
{
	CDaemonApp::instance->mainLoop();
	return kExitSuccess;
}

int
unixMainLoopStatic(int, const char**)
{
	return mainLoopStatic();
}

int
winMainLoopStatic(int, const char**)
{
	return CArchMiscWindows::runDaemon(mainLoopStatic);
}

CDaemonApp::CDaemonApp()
{
	instance = this;
}

CDaemonApp::~CDaemonApp()
{
}

int
CDaemonApp::run(int argc, char** argv)
{
	try
	{
#if SYSAPI_WIN32
		// win32 instance needed for threading, etc.
		CArchMiscWindows::setInstanceWin32(GetModuleHandle(NULL));
#endif

		// creates ARCH singleton.
		CArch arch;

		// if no args, daemonize.
		if (argc == 1) {
#if SYSAPI_WIN32
			ARCH->daemonize("Synergy", winMainLoopStatic);
#else SYSAPI_UNIX
			ARCH->daemonize("Synergy", unixMainLoopStatic);
#endif
		}
		else {
			for (int i = 1; i < argc; ++i) {
				string arg(argv[i]);

#if SYSAPI_WIN32
				if (arg == "/install") {
					ARCH->installDaemon();
					continue;
				}
				else if (arg == "/uninstall") {
					ARCH->uninstallDaemon();
					continue;
				}
#endif
				stringstream ss;
				ss << "Unrecognized argument: " << arg;
				error(ss.str().c_str());
				return kExitArgs;
			}
		}

		return kExitSuccess;
	}
	catch (XArch& e) {
		error(e.what().c_str());
		return kExitFailed;
	}
	catch (std::exception& e) {
		error(e.what());
		return kExitFailed;
	}
	catch (...) {
		error("Unrecognized error.");
		return kExitFailed;
	}
}

void
CDaemonApp::mainLoop()
{
	DAEMON_RUNNING(true);

	CEventQueue eventQueue;

#if SYSAPI_WIN32
	// HACK: create a dummy screen, which can handle system events 
	// (such as a stop request from the service controller).
	CMSWindowsScreen::init(CArchMiscWindows::instanceWin32());
	CScreen dummyScreen(new CMSWindowsScreen(false, true, false));
#endif

	CEvent event;
	EVENTQUEUE->getEvent(event);
	while (event.getType() != CEvent::kQuit) {
		EVENTQUEUE->dispatchEvent(event);
		CEvent::deleteData(event);
		EVENTQUEUE->getEvent(event);
	}

	DAEMON_RUNNING(false);
}

void CDaemonApp::error(const char* message)
{
#if SYSAPI_WIN32
	MessageBox(NULL, message, "Synergy Service", MB_OK | MB_ICONERROR);
#elif SYSAPI_UNIX
	cerr << message << endl;
#endif
}
