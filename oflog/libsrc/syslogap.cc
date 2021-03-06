// Module:  Log4CPLUS
// File:    syslogappender.cxx
// Created: 6/2001
// Author:  Tad E. Smith
//
// Copyright 2001-2009 Tad E. Smith
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dcmtk/oflog/syslogap.h"
#if defined(DCMTK_LOG4CPLUS_HAVE_SYSLOG_H) && !defined(_WIN32)

#include "dcmtk/oflog/streams.h"
#include "dcmtk/oflog/helpers/loglog.h"
#include "dcmtk/oflog/spi/logevent.h"

#include <syslog.h>

using namespace std;
using namespace dcmtk::log4cplus;
using namespace dcmtk::log4cplus::helpers;



///////////////////////////////////////////////////////////////////////////////
// dcmtk::log4cplus::SysLogAppender ctors and dtor
///////////////////////////////////////////////////////////////////////////////

SysLogAppender::SysLogAppender(const tstring& id)
: ident(id)
{
    ::openlog(DCMTK_LOG4CPLUS_TSTRING_TO_STRING (ident).c_str(), 0, 0);
}


SysLogAppender::SysLogAppender(const Properties properties, tstring&)
: Appender(properties)
{
    ident = properties.getProperty( DCMTK_LOG4CPLUS_TEXT("ident") );
    ::openlog(DCMTK_LOG4CPLUS_TSTRING_TO_STRING (ident).c_str(), 0, 0);
}


SysLogAppender::~SysLogAppender()
{
    destructorImpl();
}



///////////////////////////////////////////////////////////////////////////////
// dcmtk::log4cplus::SysLogAppender public methods
///////////////////////////////////////////////////////////////////////////////

void
SysLogAppender::close()
{
    getLogLog().debug(DCMTK_LOG4CPLUS_TEXT("Entering SysLogAppender::close()..."));
    DCMTK_LOG4CPLUS_BEGIN_SYNCHRONIZE_ON_MUTEX( access_mutex )
        ::closelog();
        closed = true;
    DCMTK_LOG4CPLUS_END_SYNCHRONIZE_ON_MUTEX;
}



///////////////////////////////////////////////////////////////////////////////
// dcmtk::log4cplus::SysLogAppender protected methods
///////////////////////////////////////////////////////////////////////////////

int
SysLogAppender::getSysLogLevel(const LogLevel& ll) const
{
    if(ll < DEBUG_LOG_LEVEL) {
        return -1;
    }
    else if(ll < INFO_LOG_LEVEL) {
        return LOG_DEBUG;
    }
    else if(ll < WARN_LOG_LEVEL) {
        return LOG_INFO;
    }
    else if(ll < ERROR_LOG_LEVEL) {
        return LOG_WARNING;
    }
    else if(ll < FATAL_LOG_LEVEL) {
        return LOG_ERR;
    }
    else if(ll == FATAL_LOG_LEVEL) {
        return LOG_CRIT;
    }

    return LOG_ALERT;  // ll > FATAL_LOG_LEVEL
}


// This method does not need to be locked since it is called by
// doAppend() which performs the locking
void
SysLogAppender::append(const spi::InternalLoggingEvent& event)
{
    int level = getSysLogLevel(event.getLogLevel());
    if(level != -1) {
        tostringstream buf;
        layout->formatAndAppend(buf, event);
        ::syslog(level, "%s", DCMTK_LOG4CPLUS_TSTRING_TO_STRING(buf.str()).c_str());
    }
}

#endif // defined(DCMTK_LOG4CPLUS_HAVE_SYSLOG_H)

