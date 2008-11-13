/*
 * SchemeShellModule.cc
 *
 * Simple scheme shell
 * Copyright (c) 2008 Linas Vepstas <linas@linas.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_GUILE

#include <opencog/util/Logger.h>
#include <opencog/util/platform.h>

#include "SchemeShellModule.h"

using namespace opencog;

DECLARE_MODULE(SchemeShellModule);

SchemeShellModule::SchemeShellModule(void)
{
}

void SchemeShellModule::init(void)
{
	shellout_register();
}

SchemeShellModule::~SchemeShellModule()
{
	shellout_unregister();
}

/**
 * Register this shell with the console.
 */
std::string SchemeShellModule::shellout(Request *req, std::list<std::string> args)
{
	SchemeShell *sh = new SchemeShell();

	SocketHolder *h = req->getSocketHolder();
	sh->set_holder(h);

printf("just created a new shell\n");

	return "Entering scheme shell; use ^D or a single . on a "
	       "line by itself to exit.";
}

#endif
/* ===================== END OF FILE ============================ */
