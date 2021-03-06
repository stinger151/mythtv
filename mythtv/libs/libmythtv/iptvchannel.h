/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_CHANNEL_H_
#define _IPTV_CHANNEL_H_

// Qt headers
#include <QMutex>

// MythTV headers
#include "dtvchannel.h"

class IPTVStreamHandler;
class IPTVTuningData;
class IPTVRecorder;

class IPTVChannel : public DTVChannel
{
  public:
    IPTVChannel(TVRec*, const QString&);
    ~IPTVChannel();

    // Commands
    virtual bool Open(void);
    virtual void Close(void); 

<<<<<<< HEAD
    virtual bool Tune(const IPTVTuningData&);
    virtual bool Tune(const QString &freqid, int finetune) { return false; }
    virtual bool Tune(const DTVMultiplex&, QString) { return false; }
=======
    // Channel scanning stuff
    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex&, QString) { return true; }
>>>>>>> fcf176db6e06928364f35f32aa022696fb19edf6

    // Sets
    void SetStreamData(MPEGStreamData*);

    // Gets
    bool IsOpen(void) const;
    virtual bool IsIPTV(void) const { return true; } // DTVChannel

  private:
    void SetStreamDataInternal(MPEGStreamData*);

  private:
    mutable QMutex m_lock;
    volatile bool m_open;
    IPTVTuningData m_last_tuning;
    IPTVStreamHandler *m_stream_handler;
    MPEGStreamData *m_stream_data;
};

#endif // _IPTV_CHANNEL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
