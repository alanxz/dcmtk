// Module:  Log4CPLUS
// File:    fileappender.cxx
// Created: 6/2001
// Author:  Tad E. Smith
//
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

#include "dcmtk/oflog/fileap.h"
#include "dcmtk/oflog/layout.h"
#include "dcmtk/oflog/streams.h"
#include "dcmtk/oflog/helpers/loglog.h"
#include "dcmtk/oflog/helpers/strhelp.h"
#include "dcmtk/oflog/helpers/timehelp.h"
#include "dcmtk/oflog/spi/logevent.h"
//#include <algorithm>

//#include <cstdio>
#define INCLUDE_CSTDIO
//#include <cerrno>
#define INCLUDE_CERRNO
//#include <cstdlib>
#define INCLUDE_CSTDLIB

#include "dcmtk/ofstd/ofstdinc.h"

using namespace dcmtk::log4cplus::helpers;

namespace dcmtk
{

namespace log4cplus
{


const long MINIMUM_ROLLING_LOG_SIZE = 200*1024L;


///////////////////////////////////////////////////////////////////////////////
// File LOCAL definitions
///////////////////////////////////////////////////////////////////////////////

namespace
{

static
int
file_rename (tstring const & src, tstring const & target)
{
    return rename (DCMTK_LOG4CPLUS_TSTRING_TO_STRING (src).c_str (),
        DCMTK_LOG4CPLUS_TSTRING_TO_STRING (target).c_str ()) == 0 ? 0 : -1;
}


static
int
file_remove (tstring const & src)
{
    return remove (DCMTK_LOG4CPLUS_TSTRING_TO_STRING (src).c_str ()) == 0
        ? 0 : -1;
}


static
void
loglog_renaming_result (LogLog & loglog, tstring const & src,
    tstring const & target, int ret)
{
    if (ret == 0)
    {
        loglog.debug (
            DCMTK_LOG4CPLUS_TEXT("Renamed file ")
            + src
            + DCMTK_LOG4CPLUS_TEXT(" to ")
            + target);
    }
    else if (ret == -1 && errno != ENOENT)
    {
        loglog.error (
            DCMTK_LOG4CPLUS_TEXT("Failed to rename file from ")
            + target
            + DCMTK_LOG4CPLUS_TEXT(" to ")
            + target);
    }
}


static
void
loglog_opening_result (LogLog & loglog,
    tostream const & os, tstring const & filename)
{
    if (! os)
    {
        loglog.error (
            DCMTK_LOG4CPLUS_TEXT("Failed to open file ")
            + filename);
    }
}


static
void
rolloverFiles(const tstring& filename, unsigned int maxBackupIndex)
{
    SharedObjectPtr<LogLog> loglog
        = LogLog::getLogLog();

    // Delete the oldest file
    tostringstream buffer;
    buffer << filename << DCMTK_LOG4CPLUS_TEXT(".") << maxBackupIndex;
    OFSTRINGSTREAM_GETOFSTRING(buffer, buffer_str)
    int ret = file_remove (buffer_str);

    tostringstream source_oss;
    tostringstream target_oss;

    // Map {(maxBackupIndex - 1), ..., 2, 1} to {maxBackupIndex, ..., 3, 2}
    for (int i = maxBackupIndex - 1; i >= 1; --i)
    {
        source_oss.str(DCMTK_LOG4CPLUS_TEXT(""));
        target_oss.str(DCMTK_LOG4CPLUS_TEXT(""));

        source_oss << filename << DCMTK_LOG4CPLUS_TEXT(".") << i;
        target_oss << filename << DCMTK_LOG4CPLUS_TEXT(".") << (i+1);

        OFSTRINGSTREAM_GETOFSTRING(source_oss, source)
        OFSTRINGSTREAM_GETOFSTRING(target_oss, target)

#if defined (_WIN32)
        // Try to remove the target first. It seems it is not
        // possible to rename over existing file.
        ret = file_remove (target);
#endif

        ret = file_rename (source, target);
        loglog_renaming_result (*loglog, source, target, ret);
    }
} // end rolloverFiles()

}


///////////////////////////////////////////////////////////////////////////////
// FileAppender ctors and dtor
///////////////////////////////////////////////////////////////////////////////

FileAppender::FileAppender(const tstring& filename_,
    DCMTK_LOG4CPLUS_OPEN_MODE_TYPE mode, bool immediateFlush_)
    : immediateFlush(immediateFlush_)
    , reopenDelay(1)
{
    init(filename_, mode);
}


FileAppender::FileAppender(const Properties& properties,
                           tstring&,
                           DCMTK_LOG4CPLUS_OPEN_MODE_TYPE mode)
    : Appender(properties)
    , immediateFlush(true)
    , reopenDelay(1)
{
    bool append_ = (mode == STD_NAMESPACE ios::app);
    tstring filename_ = properties.getProperty( DCMTK_LOG4CPLUS_TEXT("File") );
    if (filename_.empty())
    {
        getErrorHandler()->error( DCMTK_LOG4CPLUS_TEXT("Invalid filename") );
        return;
    }
    if(properties.exists( DCMTK_LOG4CPLUS_TEXT("ImmediateFlush") )) {
        tstring tmp = properties.getProperty( DCMTK_LOG4CPLUS_TEXT("ImmediateFlush") );
        immediateFlush = (toLower(tmp) == DCMTK_LOG4CPLUS_TEXT("true"));
    }
    if(properties.exists( DCMTK_LOG4CPLUS_TEXT("Append") )) {
        tstring tmp = properties.getProperty( DCMTK_LOG4CPLUS_TEXT("Append") );
        append_ = (toLower(tmp) == DCMTK_LOG4CPLUS_TEXT("true"));
    }
    if(properties.exists( DCMTK_LOG4CPLUS_TEXT("ReopenDelay") )) {
        tstring tmp = properties.getProperty( DCMTK_LOG4CPLUS_TEXT("ReopenDelay") );
        reopenDelay = atoi(DCMTK_LOG4CPLUS_TSTRING_TO_STRING(tmp).c_str());
    }

    init(filename_, (append_ ? STD_NAMESPACE ios::app : STD_NAMESPACE ios::trunc));
}



void
FileAppender::init(const tstring& filename_,
                   DCMTK_LOG4CPLUS_OPEN_MODE_TYPE mode)
{
    this->filename = filename_;
    open(mode);

    if(!out.good()) {
        getErrorHandler()->error(  DCMTK_LOG4CPLUS_TEXT("Unable to open file: ")
                                 + filename);
        return;
    }
    getLogLog().debug(DCMTK_LOG4CPLUS_TEXT("Just opened file: ") + filename);
}



FileAppender::~FileAppender()
{
    destructorImpl();
}



///////////////////////////////////////////////////////////////////////////////
// FileAppender public methods
///////////////////////////////////////////////////////////////////////////////

void
FileAppender::close()
{
    DCMTK_LOG4CPLUS_BEGIN_SYNCHRONIZE_ON_MUTEX( access_mutex )
        out.close();
        closed = true;
    DCMTK_LOG4CPLUS_END_SYNCHRONIZE_ON_MUTEX;
}



///////////////////////////////////////////////////////////////////////////////
// FileAppender protected methods
///////////////////////////////////////////////////////////////////////////////

// This method does not need to be locked since it is called by
// doAppend() which performs the locking
void
FileAppender::append(const spi::InternalLoggingEvent& event)
{
    if(!out.good()) {
        if(!reopen()) {
            getErrorHandler()->error(  DCMTK_LOG4CPLUS_TEXT("file is not open: ")
                                     + filename);
            return;
        }
        // Resets the error handler to make it
        // ready to handle a future append error.
        else
            getErrorHandler()->reset();
    }

    layout->formatAndAppend(out, event);
    if(immediateFlush) {
        out.flush();
    }
}

void
FileAppender::open(STD_NAMESPACE ios::openmode mode)
{
    out.open(DCMTK_LOG4CPLUS_TSTRING_TO_STRING(filename).c_str(), mode);
}

bool
FileAppender::reopen()
{
    // When append never failed and the file re-open attempt must
    // be delayed, set the time when reopen should take place.
    if (reopen_time == Time () && reopenDelay != 0)
        reopen_time = Time::gettimeofday()
			+ Time(reopenDelay);
    else
	{
        // Otherwise, check for end of the delay (or absence of delay) to re-open the file.
        if (reopen_time <= Time::gettimeofday()
			|| reopenDelay == 0)
		{
            // Close the current file
            out.close();
            out.clear(); // reset flags since the C++ standard specified that all the
                         // flags should remain unchanged on a close

            // Re-open the file.
            open(STD_NAMESPACE ios::app);

            // Reset last fail time.
            reopen_time = Time ();

            // Succeed if no errors are found.
            if(out.good())
                return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
// RollingFileAppender ctors and dtor
///////////////////////////////////////////////////////////////////////////////

RollingFileAppender::RollingFileAppender(const tstring& filename_,
    long maxFileSize_, int maxBackupIndex_, bool immediateFlush_)
    : FileAppender(filename_, STD_NAMESPACE ios::app, immediateFlush_)
{
    init(maxFileSize_, maxBackupIndex_);
}


RollingFileAppender::RollingFileAppender(const Properties& properties, tstring& error)
    : FileAppender(properties, error, STD_NAMESPACE ios::app)
{
    int maxFileSize_ = 10*1024*1024;
    int maxBackupIndex_ = 1;
    if(properties.exists( DCMTK_LOG4CPLUS_TEXT("MaxFileSize") )) {
        tstring tmp = properties.getProperty( DCMTK_LOG4CPLUS_TEXT("MaxFileSize") );
        tmp = toUpper(tmp);
        maxFileSize_ = atoi(DCMTK_LOG4CPLUS_TSTRING_TO_STRING(tmp).c_str());
        if(tmp.find( DCMTK_LOG4CPLUS_TEXT("MB") ) == (tmp.length() - 2)) {
            maxFileSize_ *= (1024 * 1024); // convert to megabytes
        }
        if(tmp.find( DCMTK_LOG4CPLUS_TEXT("KB") ) == (tmp.length() - 2)) {
            maxFileSize_ *= 1024; // convert to kilobytes
        }
    }

    if(properties.exists( DCMTK_LOG4CPLUS_TEXT("MaxBackupIndex") )) {
        tstring tmp = properties.getProperty(DCMTK_LOG4CPLUS_TEXT("MaxBackupIndex"));
        maxBackupIndex_ = atoi(DCMTK_LOG4CPLUS_TSTRING_TO_STRING(tmp).c_str());
    }

    init(maxFileSize_, maxBackupIndex_);
}


void
RollingFileAppender::init(long maxFileSize_, int maxBackupIndex_)
{
    if (maxFileSize_ < MINIMUM_ROLLING_LOG_SIZE)
    {
        tostringstream oss;
        oss << DCMTK_LOG4CPLUS_TEXT ("RollingFileAppender: MaxFileSize property")
            DCMTK_LOG4CPLUS_TEXT (" value is too small. Resetting to ")
            << MINIMUM_ROLLING_LOG_SIZE << ".";
        OFSTRINGSTREAM_GETSTR(oss, str)
        getLogLog().warn(str);
        OFSTRINGSTREAM_FREESTR(str)
        maxFileSize_ = MINIMUM_ROLLING_LOG_SIZE;
    }
    this->maxFileSize = maxFileSize_;
    if (maxBackupIndex_ < 1)
        maxBackupIndex_ = 1;
    this->maxBackupIndex = maxBackupIndex_;
}


RollingFileAppender::~RollingFileAppender()
{
    destructorImpl();
}


///////////////////////////////////////////////////////////////////////////////
// RollingFileAppender protected methods
///////////////////////////////////////////////////////////////////////////////

// This method does not need to be locked since it is called by
// doAppend() which performs the locking
void
RollingFileAppender::append(const spi::InternalLoggingEvent& event)
{
    FileAppender::append(event);

    if(out.tellp() > maxFileSize) {
        rollover();
    }
}


void
RollingFileAppender::rollover()
{
    LogLog & loglog = getLogLog();

    // Close the current file
    out.close();
    out.clear(); // reset flags since the C++ standard specified that all the
                 // flags should remain unchanged on a close

    // If maxBackups <= 0, then there is no file renaming to be done.
    if (maxBackupIndex > 0)
    {
        rolloverFiles(filename, maxBackupIndex);

        // Rename fileName to fileName.1
        tstring target = filename + DCMTK_LOG4CPLUS_TEXT(".1");

        int ret;

#if defined (_WIN32)
        // Try to remove the target first. It seems it is not
        // possible to rename over existing file.
        ret = file_remove (target);
#endif

        loglog.debug (
            DCMTK_LOG4CPLUS_TEXT("Renaming file ")
            + filename
            + DCMTK_LOG4CPLUS_TEXT(" to ")
            + target);
        ret = file_rename (filename, target);
        loglog_renaming_result (loglog, filename, target, ret);
    }
    else
    {
        loglog.debug (filename + DCMTK_LOG4CPLUS_TEXT(" has no backups specified"));
    }

    // Open it up again in truncation mode
    open(STD_NAMESPACE ios::out | STD_NAMESPACE ios::trunc);
    loglog_opening_result (loglog, out, filename);
}


///////////////////////////////////////////////////////////////////////////////
// DailyRollingFileAppender ctors and dtor
///////////////////////////////////////////////////////////////////////////////

DailyRollingFileAppender::DailyRollingFileAppender(
    const tstring& filename_, DailyRollingFileSchedule schedule_,
    bool immediateFlush_, int maxBackupIndex_)
    : FileAppender(filename_, STD_NAMESPACE ios::app, immediateFlush_)
    , maxBackupIndex(maxBackupIndex_)
{
    init(schedule_);
}



DailyRollingFileAppender::DailyRollingFileAppender(
    const Properties& properties, tstring& error)
    : FileAppender(properties, error, STD_NAMESPACE ios::app)
    , maxBackupIndex(10)
{
    DailyRollingFileSchedule theSchedule = DAILY;
    tstring scheduleStr = properties.getProperty(DCMTK_LOG4CPLUS_TEXT("Schedule"));
    scheduleStr = toUpper(scheduleStr);

    if(scheduleStr == DCMTK_LOG4CPLUS_TEXT("MONTHLY"))
        theSchedule = MONTHLY;
    else if(scheduleStr == DCMTK_LOG4CPLUS_TEXT("WEEKLY"))
        theSchedule = WEEKLY;
    else if(scheduleStr == DCMTK_LOG4CPLUS_TEXT("DAILY"))
        theSchedule = DAILY;
    else if(scheduleStr == DCMTK_LOG4CPLUS_TEXT("TWICE_DAILY"))
        theSchedule = TWICE_DAILY;
    else if(scheduleStr == DCMTK_LOG4CPLUS_TEXT("HOURLY"))
        theSchedule = HOURLY;
    else if(scheduleStr == DCMTK_LOG4CPLUS_TEXT("MINUTELY"))
        theSchedule = MINUTELY;
    else {
        getLogLog().warn(  DCMTK_LOG4CPLUS_TEXT("DailyRollingFileAppender::ctor()- \"Schedule\" not valid: ")
                         + properties.getProperty(DCMTK_LOG4CPLUS_TEXT("Schedule")));
        theSchedule = DAILY;
    }

    if(properties.exists( DCMTK_LOG4CPLUS_TEXT("MaxBackupIndex") )) {
        tstring tmp = properties.getProperty(DCMTK_LOG4CPLUS_TEXT("MaxBackupIndex"));
        maxBackupIndex = atoi(DCMTK_LOG4CPLUS_TSTRING_TO_STRING(tmp).c_str());
    }

    init(theSchedule);
}



void
DailyRollingFileAppender::init(DailyRollingFileSchedule schedule_)
{
    this->schedule = schedule_;

    Time now = Time::gettimeofday();
    now.usec(0);
    struct tm time;
    now.localtime(&time);

    time.tm_sec = 0;
    switch (schedule)
    {
    case MONTHLY:
        time.tm_mday = 1;
        time.tm_hour = 0;
        time.tm_min = 0;
        break;

    case WEEKLY:
        time.tm_mday -= (time.tm_wday % 7);
        time.tm_hour = 0;
        time.tm_min = 0;
        break;

    case DAILY:
        time.tm_hour = 0;
        time.tm_min = 0;
        break;

    case TWICE_DAILY:
        if(time.tm_hour >= 12) {
            time.tm_hour = 12;
        }
        else {
            time.tm_hour = 0;
        }
        time.tm_min = 0;
        break;

    case HOURLY:
        time.tm_min = 0;
        break;

    case MINUTELY:
        break;
    };
    now.setTime(&time);

    scheduledFilename = getFilename(now);
    nextRolloverTime = calculateNextRolloverTime(now);
}



DailyRollingFileAppender::~DailyRollingFileAppender()
{
    destructorImpl();
}




///////////////////////////////////////////////////////////////////////////////
// DailyRollingFileAppender public methods
///////////////////////////////////////////////////////////////////////////////

void
DailyRollingFileAppender::close()
{
    rollover();
    FileAppender::close();
}



///////////////////////////////////////////////////////////////////////////////
// DailyRollingFileAppender protected methods
///////////////////////////////////////////////////////////////////////////////

// This method does not need to be locked since it is called by
// doAppend() which performs the locking
void
DailyRollingFileAppender::append(const spi::InternalLoggingEvent& event)
{
    if(event.getTimestamp() >= nextRolloverTime) {
        rollover();
    }

    FileAppender::append(event);
}



void
DailyRollingFileAppender::rollover()
{
    // Close the current file
    out.close();
    out.clear(); // reset flags since the C++ standard specified that all the
                 // flags should remain unchanged on a close

    // If we've already rolled over this time period, we'll make sure that we
    // don't overwrite any of those previous files.
    // E.g. if "log.2009-11-07.1" already exists we rename it
    // to "log.2009-11-07.2", etc.
    rolloverFiles(scheduledFilename, maxBackupIndex);

    // Do not overwriet the newest file either, e.g. if "log.2009-11-07"
    // already exists rename it to "log.2009-11-07.1"
    tostringstream backup_target_oss;
    backup_target_oss << scheduledFilename << DCMTK_LOG4CPLUS_TEXT(".") << 1;
    OFSTRINGSTREAM_GETOFSTRING(backup_target_oss, backupTarget)

    LogLog & loglog = getLogLog();
    int ret;

#if defined (_WIN32)
    // Try to remove the target first. It seems it is not
    // possible to rename over existing file, e.g. "log.2009-11-07.1".
    ret = file_remove (backupTarget);
#endif

    // Rename e.g. "log.2009-11-07" to "log.2009-11-07.1".
    ret = file_rename (scheduledFilename, backupTarget);
    loglog_renaming_result (loglog, scheduledFilename, backupTarget, ret);

#if defined (_WIN32)
    // Try to remove the target first. It seems it is not
    // possible to rename over existing file, e.g. "log.2009-11-07".
    ret = file_remove (scheduledFilename);
#endif

    // Rename filename to scheduledFilename,
    // e.g. rename "log" to "log.2009-11-07".
    loglog.debug(
        DCMTK_LOG4CPLUS_TEXT("Renaming file ")
        + filename
        + DCMTK_LOG4CPLUS_TEXT(" to ")
        + scheduledFilename);
    ret = file_rename (filename, scheduledFilename);
    loglog_renaming_result (loglog, filename, scheduledFilename, ret);

    // Open a new file, e.g. "log".
    open(STD_NAMESPACE ios::out | STD_NAMESPACE ios::trunc);
    loglog_opening_result (loglog, out, filename);

    // Calculate the next rollover time
    Time now = Time::gettimeofday();
    if (now >= nextRolloverTime)
    {
        scheduledFilename = getFilename(now);
        nextRolloverTime = calculateNextRolloverTime(now);
    }
}



Time
DailyRollingFileAppender::calculateNextRolloverTime(const Time& t) const
{
    switch(schedule)
    {
    case MONTHLY:
    {
        struct tm nextMonthTime;
        t.localtime(&nextMonthTime);
        nextMonthTime.tm_mon += 1;
        nextMonthTime.tm_isdst = 0;

        Time ret;
        if(ret.setTime(&nextMonthTime) == -1) {
            getLogLog().error(
                DCMTK_LOG4CPLUS_TEXT("DailyRollingFileAppender::calculateNextRolloverTime()-")
                DCMTK_LOG4CPLUS_TEXT(" setTime() returned error"));
            // Set next rollover to 31 days in future.
            ret = (t + Time(2678400));
        }

        return ret;
    }

    case WEEKLY:
        return (t + Time(7 * 24 * 60 * 60));

    default:
        getLogLog ().error (
            DCMTK_LOG4CPLUS_TEXT ("DailyRollingFileAppender::calculateNextRolloverTime()-")
            DCMTK_LOG4CPLUS_TEXT (" invalid schedule value"));
        // Fall through.

    case DAILY:
        return (t + Time(24 * 60 * 60));

    case TWICE_DAILY:
        return (t + Time(12 * 60 * 60));

    case HOURLY:
        return (t + Time(60 * 60));

    case MINUTELY:
        return (t + Time(60));
    };
}



tstring
DailyRollingFileAppender::getFilename(const Time& t) const
{
    tchar const * pattern = 0;
    switch (schedule)
    {
    case MONTHLY:
        pattern = DCMTK_LOG4CPLUS_TEXT("%Y-%m");
        break;

    case WEEKLY:
        pattern = DCMTK_LOG4CPLUS_TEXT("%Y-%W");
        break;

    default:
        getLogLog ().error (
            DCMTK_LOG4CPLUS_TEXT ("DailyRollingFileAppender::getFilename()-")
            DCMTK_LOG4CPLUS_TEXT (" invalid schedule value"));
        // Fall through.

    case DAILY:
        pattern = DCMTK_LOG4CPLUS_TEXT("%Y-%m-%d");
        break;

    case TWICE_DAILY:
        pattern = DCMTK_LOG4CPLUS_TEXT("%Y-%m-%d-%p");
        break;

    case HOURLY:
        pattern = DCMTK_LOG4CPLUS_TEXT("%Y-%m-%d-%H");
        break;

    case MINUTELY:
        pattern = DCMTK_LOG4CPLUS_TEXT("%Y-%m-%d-%H-%M");
        break;
    };

    tstring result (filename);
    result += DCMTK_LOG4CPLUS_TEXT(".");
    result += t.getFormattedTime(pattern, false);
    return result;
}

} // namespace log4cplus

} // namespace dcmtk
