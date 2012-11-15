/*
 *
 *  Copyright (C) 2002-2012, OFFIS e.V.
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmdata
 *
 *  Author:  Marco Eichelberg
 *
 *  Purpose: DcmInputFileStream and related classes,
 *    implements streamed input from files.
 *
 */

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcistrmf.h"
#include "dcmtk/dcmdata/dcerror.h"

#define INCLUDE_CSTDIO
#define INCLUDE_CERRNO
#include "dcmtk/ofstd/ofstdinc.h"


DcmFileProducer::DcmFileProducer(const OFFilename &filename, offile_off_t offset)
: DcmProducer()
, status_(EC_Normal)
, size_(0)
{
  file_.open(filename.getCharPointer(), std::ios_base::in | std::ios_base::binary);

  if (!file_.fail())
  {
     // Get number of bytes in file
     file_.seekg(0, std::ios_base::end);
     size_ =  file_.tellg();
     if (file_.seekg(offset, std::ios_base::beg).fail())
     {
       OFString s("(unknown error code)");
       status_ = makeOFCondition(OFM_dcmdata, 18, OF_error, s.c_str());
     }
  }
  else
  {
    OFString s("(unknown error code)");
    status_ = makeOFCondition(OFM_dcmdata, 18, OF_error, s.c_str());
  }
}

DcmFileProducer::~DcmFileProducer()
{
}

OFBool DcmFileProducer::good() const
{
  return status_.good();
}

OFCondition DcmFileProducer::status() const
{
  return status_;
}

OFBool DcmFileProducer::eos()
{
  if (file_.is_open())
  {
    return (file_.eof() || (size_ == file_.tellg()));
  }
  else return OFTrue;
}

offile_off_t DcmFileProducer::avail()
{
  if (file_.is_open()) return size_ - file_.tellg(); else return 0;
}

offile_off_t DcmFileProducer::read(void *buf, offile_off_t buflen)
{
  offile_off_t result = 0;
  if (status_.good() && file_.is_open() && buf && buflen)
  {
    result = file_.readsome(static_cast<char *>(buf), buflen);
  }
  return result;
}

offile_off_t DcmFileProducer::skip(offile_off_t skiplen)
{
  offile_off_t result = 0;
  if (status_.good() && file_.is_open() && skiplen)
  {
    offile_off_t pos = file_.tellg();
    result = (size_ - pos < skiplen) ? (size_ - pos) : skiplen;
    if (file_.seekg(result, std::ios_base::cur).fail())
    {
      OFString s("std::fstream::seekg() failed");
      status_ = makeOFCondition(OFM_dcmdata, 18, OF_error, s.c_str());
    }
  }
  return result;
}

void DcmFileProducer::putback(offile_off_t num)
{
  if (status_.good() && file_.is_open() && num)
  {
    offile_off_t pos = file_.tellg();
    if (num <= pos)
    {
      if (file_.seekg(-num, std::ios_base::cur).fail())
      {
        OFString s("std::fstream::seekg() failed");
        status_ = makeOFCondition(OFM_dcmdata, 18, OF_error, s.c_str());
      }
    }
    else status_ = EC_PutbackFailed; // tried to putback before start of file
  }
}


/* ======================================================================= */

DcmInputFileStreamFactory::DcmInputFileStreamFactory(const OFFilename &filename, offile_off_t offset)
: DcmInputStreamFactory()
, filename_(filename)
, offset_(offset)
{
}

DcmInputFileStreamFactory::DcmInputFileStreamFactory(const DcmInputFileStreamFactory& arg)
: DcmInputStreamFactory(arg)
, filename_(arg.filename_)
, offset_(arg.offset_)
{
}

DcmInputFileStreamFactory::~DcmInputFileStreamFactory()
{
}

DcmInputStream *DcmInputFileStreamFactory::create() const
{
  return new DcmInputFileStream(filename_, offset_);
}

/* ======================================================================= */

DcmInputFileStream::DcmInputFileStream(const OFFilename &filename, offile_off_t offset)
: DcmInputStream(&producer_) // safe because DcmInputStream only stores pointer
, producer_(filename, offset)
, filename_(filename)
{
}

DcmInputFileStream::~DcmInputFileStream()
{
}

DcmInputStreamFactory *DcmInputFileStream::newFactory() const
{
  DcmInputStreamFactory *result = NULL;
  if (currentProducer() == &producer_)
  {
    // no filter installed, can create factory object
    result = new DcmInputFileStreamFactory(filename_, tell());
  }
  return result;
}

/* ======================================================================= */

DcmInputStream *DcmTempFileHandler::create() const
{
    return new DcmInputFileStream(filename_, 0);
}

DcmTempFileHandler::DcmTempFileHandler(const OFFilename &filename)
#ifdef WITH_THREADS
: refCount_(1), mutex_(), filename_(filename)
#else
: refCount_(1), filename_(filename)
#endif
{
}

DcmTempFileHandler::~DcmTempFileHandler()
{
    OFStandard::deleteFile(filename_);
}

DcmTempFileHandler *DcmTempFileHandler::newInstance(const OFFilename &filename)
{
    return new DcmTempFileHandler(filename);
}

void DcmTempFileHandler::increaseRefCount()
{
#ifdef WITH_THREADS
    mutex_.lock();
#endif
    ++refCount_;
#ifdef WITH_THREADS
    mutex_.unlock();
#endif
}

void DcmTempFileHandler::decreaseRefCount()
{
#ifdef WITH_THREADS
    mutex_.lock();
#endif
    size_t result = --refCount_;
#ifdef WITH_THREADS
    mutex_.unlock();
#endif
    if (result == 0) delete this;
}

/* ======================================================================= */

DcmInputTempFileStreamFactory::DcmInputTempFileStreamFactory(DcmTempFileHandler *handler)
: DcmInputStreamFactory()
, fileHandler_(handler)
{
    fileHandler_->increaseRefCount();
}

DcmInputTempFileStreamFactory::DcmInputTempFileStreamFactory(const DcmInputTempFileStreamFactory &arg)
: DcmInputStreamFactory(arg)
, fileHandler_(arg.fileHandler_)
{
    fileHandler_->increaseRefCount();
}

DcmInputTempFileStreamFactory::~DcmInputTempFileStreamFactory()
{
    fileHandler_->decreaseRefCount();
}

DcmInputStream *DcmInputTempFileStreamFactory::create() const
{
    return fileHandler_->create();
}

DcmInputStreamFactory *DcmInputTempFileStreamFactory::clone() const
{
    return new DcmInputTempFileStreamFactory(*this);
}
