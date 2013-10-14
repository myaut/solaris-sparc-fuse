/*
	log.c (02.09.09)
	exFAT file system implementation library.

	Copyright (C) 2010-2013  Andrew Nayenko

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <exfat/exfat.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>

int exfat_errors;
int exfat_log_enabled = 1;

exfat_bug_handler_func exfat_bug_handler = abort;

static void exfat_logmsg(int priority, const char* prefix, const char* format, va_list ap) {
	va_list aq;

	if(exfat_log_enabled && !isatty(STDERR_FILENO)) {
		va_copy(aq, ap);

		fflush(stdout);
		fputs(prefix, stderr);

		vfprintf(stderr, format, aq);

		va_end(aq);

		fputc('\n', stderr);
	}

	vsyslog(priority, format, ap);
}

/*
 * This message means an internal bug in exFAT implementation.
 */
void exfat_bug(const char* format, ...)
{
	va_list ap;

	va_start(ap, format);
	exfat_logmsg(LOG_CRIT, "BUG: ", format, ap);
	va_end(ap);

	exfat_bug_handler();
}

/*
 * This message means an error in exFAT file system.
 */
void exfat_error(const char* format, ...)
{
	va_list ap;

	exfat_errors++;

	va_start(ap, format);
	exfat_logmsg(LOG_ERR, "ERROR: ", format, ap);
	va_end(ap);
}

/*
 * This message means that there is something unexpected in exFAT file system
 * that can be a potential problem.
 */
void exfat_warn(const char* format, ...)
{
	va_list ap;

	va_start(ap, format);
	exfat_logmsg(LOG_WARNING, "WARN: ", format, ap);
	va_end(ap);
}

/*
 * Just debug message. Disabled by default.
 */
void exfat_debug(const char* format, ...)
{
	va_list ap;

	va_start(ap, format);
	exfat_logmsg(LOG_DEBUG, "DEBUG: ", format, ap);
	va_end(ap);
}
