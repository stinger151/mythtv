// -*- Mode: c++ -*-

// System headers
#ifdef _WIN32
# ifndef _MSC_VER
#  include <ws2tcpip.h>
# endif
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
#endif

// Qt headers
#include <QTcpSocket>
#include <QQueue>

// MythTV headers
#include "iptvstreamhandler.h"
#include "rtppacketbuffer.h"
#include "udppacketbuffer.h"
#include "rtptsdatapacket.h"
#include "rtpdatapacket.h"
#include "rtpfecpacket.h"
#include "mythlogging.h"
#include "cetonrtsp.h"

#define LOC QString("IPTVSH(%1): ").arg(_device)

QMap<QString,IPTVStreamHandler*> IPTVStreamHandler::s_handlers;
QMap<QString,uint>               IPTVStreamHandler::s_handlers_refcnt;
QMutex                           IPTVStreamHandler::s_handlers_lock;

IPTVStreamHandler *IPTVStreamHandler::Get(const IPTVTuningData &tuning)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devkey = tuning.GetDeviceKey();

    QMap<QString,IPTVStreamHandler*>::iterator it = s_handlers.find(devkey);

    if (it == s_handlers.end())
    {
        IPTVStreamHandler *newhandler = new IPTVStreamHandler(tuning);
        newhandler->Start();
        s_handlers[devkey] = newhandler;
        s_handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH: Creating new stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()));
    }
    else
    {
        s_handlers_refcnt[devkey]++;
        uint rcount = s_handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH: Using existing stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devkey];
}

void IPTVStreamHandler::Return(IPTVStreamHandler * & ref)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = s_handlers_refcnt.find(devname);
    if (rit == s_handlers_refcnt.end())
        return;

    LOG(VB_RECORD, LOG_INFO, QString("IPTVSH: Return(%1) has %2 handlers")
        .arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,IPTVStreamHandler*>::iterator it = s_handlers.find(devname);
    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("IPTVSH: Closing handler for %1")
                           .arg(devname));
        ref->Stop();
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("IPTVSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    s_handlers_refcnt.erase(rit);
    ref = NULL;
}

IPTVStreamHandler::IPTVStreamHandler(const IPTVTuningData &tuning) :
    StreamHandler(tuning.GetDeviceKey()),
    m_tuning(tuning),
    m_write_helper(NULL),
    m_buffer(NULL),
    m_use_rtp_streaming(true)
{
    memset(m_sockets, 0, sizeof(m_sockets));
    memset(m_read_helpers, 0, sizeof(m_read_helpers));
    m_use_rtp_streaming = m_tuning.GetDataURL().scheme().toUpper() == "RTP";
}

void IPTVStreamHandler::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, LOC + "run()");

    SetRunning(true, false, false);

    // TODO Error handling..

    // Setup
    CetonRTSP *rtsp = NULL;
    IPTVTuningData tuning = m_tuning;
    if (m_tuning.GetURL(0).scheme().toLower() == "rtsp")
    {
        rtsp = new CetonRTSP(m_tuning.GetURL(0));

        // Check RTSP capabilities
        QStringList options;
        if (!(rtsp->GetOptions(options)     && options.contains("OPTIONS")  &&
              options.contains("DESCRIBE")  && options.contains("SETUP")    &&
              options.contains("PLAY")      && options.contains("TEARDOWN")))
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "RTSP interface did not support the necessary options");
            delete rtsp;
            SetRunning(false, false, false);
            RunEpilog();
            return;
        }

        if (!rtsp->Describe())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "RTSP Describe command failed");
            delete rtsp;
            SetRunning(false, false, false);
            RunEpilog();
            return;
        }

        tuning = IPTVTuningData(
            QString("rtp://%1@%2:0")
            .arg(m_tuning.GetURL(0).host())
            .arg(QHostAddress(QHostAddress::Any).toString()), 0,
            IPTVTuningData::kNone,
            QString("rtp://%1@%2:0")
            .arg(m_tuning.GetURL(0).host())
            .arg(QHostAddress(QHostAddress::Any).toString()), 0,
            "", 0);
    }
  LOG(VB_GENERAL, LOG_DEBUG, LOC + "pre for socket");
    for (uint i = 0; i < IPTV_SOCKET_COUNT; i++)
    {
        QUrl url = tuning.GetURL(i);
        if (url.port() < 0)
            continue;

        m_sockets[i] = new QTcpSocket();
		 m_sockets[i]->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        m_read_helpers[i] = new IPTVStreamHandlerReadHelper(this, m_sockets[i], i);   
	  LOG(VB_GENERAL, LOG_DEBUG, LOC + "pre http GET");
        if (!url.userInfo().isEmpty())
            m_sender[i] = QHostAddress(url.userInfo());
			m_sockets[i]->connectToHost(url.host().toAscii(), 3000);	
                           m_sockets[i]->write("GET " + url.path().toAscii() + " HTTP/1.0\r\n\r\n\r\n\r\n");
                           m_sockets[i]->waitForBytesWritten(1000);
	 }
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "after socket count");
        m_buffer = new UDPPacketBuffer(tuning.GetBitrate(0));
    m_write_helper = new IPTVStreamHandlerWriteHelper(this);
    m_write_helper->Start();
LOG(VB_GENERAL, LOG_INFO, LOC + "HTTP Stream open!");
    bool error = false;
  
    if (!error)
    {
        // Enter event loop
        exec();
    }

    // Clean up
    for (uint i = 0; i < IPTV_SOCKET_COUNT; i++)
    {
        if (m_sockets[i])
        {
            delete m_sockets[i];
            m_sockets[i] = NULL;
            delete m_read_helpers[i];
            m_read_helpers[i] = NULL;
        }
    }
    delete m_buffer;
    m_buffer = NULL;
    delete m_write_helper;
    m_write_helper = NULL;

    if (rtsp)
    {
        rtsp->Teardown();
        delete rtsp;
    }

    SetRunning(false, false, false);
    RunEpilog();
}

IPTVStreamHandlerReadHelper::IPTVStreamHandlerReadHelper(
    IPTVStreamHandler *p, QTcpSocket *s, uint stream) :
    m_parent(p), m_socket(s), m_sender(p->m_sender[stream]),
    m_stream(stream)
{
    connect(m_socket, SIGNAL(readyRead()),
            this,     SLOT(ReadPending()));
}

void IPTVStreamHandlerReadHelper::ReadPending(void)
{
   
  QByteArray baSyncByte;
        baSyncByte.resize(1);
        baSyncByte[0] = 0x47;
        QHostAddress sender;
    
       bool sender_null = m_sender.isNull();


                while (m_socket->bytesAvailable() > 0)
            {
                QByteArray newFrame = m_socket->readAll();

                int PosFirst=newFrame.indexOf(baSyncByte);

                if(PosFirst!=-1)
                {
                if(tsFramequeue.size()>0)
                {
                    tsFramequeue.append(newFrame);

                      newFrame=tsFramequeue;

                    tsFramequeue.clear();

                }

                PosFirst=newFrame.indexOf(baSyncByte);


    int remain;
                while(PosFirst >= 0 && PosFirst < newFrame.size())
                {
                    if(newFrame.at(PosFirst)==0x47)
                    {

                        if(PosFirst+188<=newFrame.size())
                        {

                        FrameOUT+=newFrame.mid(PosFirst,188);

                        }
                        else
                        {
                        remain=PosFirst;
                        }
                        PosFirst= PosFirst+188;

                    }
                    else
                    {
                        LOG(VB_RECORD, LOG_ERR,  "TCP /HTTP STREAM RESYNC");

                    PosFirst=newFrame.indexOf(baSyncByte,PosFirst+1);
                    }


                }
                if(remain>0)
                {

                    tsFramequeue.clear();
                tsFramequeue=newFrame.mid(remain);

                }
   }
	else
                {
                    if(tsFramequeue.size()>0)
                    {
                        tsFramequeue.append(newFrame);

                          FrameOUT+=tsFramequeue;

                        tsFramequeue.clear();
                    }
                    else
                    {
                    
                    return;
                    }
                }

                newFrame.clear();
            }
          
           int rest =  FrameOUT.size()  %  188;

           if(rest!=0)
           {
               
               if(!FrameOUT.startsWith(baSyncByte))
               {
               FrameOUT.clear();
               LOG(VB_RECORD, LOG_ERR,  "TCP /HTTP STREAM PACKETS LOST DISCARDING BUFFER");
               }
               FrameOUT.clear();

               return;
           }

          if (0 == m_stream)
              {
                  if(FrameOUT.size() > 0 ) ///1 ts frame ist 188 *7 = udppacket size von 1316 && sync byte 0x47 an stelle 0
                      {
                          UDPPacket packet(m_parent->m_buffer->GetEmptyPacket());
                          QByteArray &data = packet.GetDataReference();
                          data.resize(FrameOUT.size());
                          data = FrameOUT;
                          if (sender_null || sender == m_sender)
                              m_parent->m_buffer->PushDataPacket(packet);
                      }

                  else
                      {
                          LOG(VB_RECORD, LOG_ERR,  "TCP /HTTP STREAM FRAMEOUT SIZE = 0");
                      }
              }

          FrameOUT.clear();
 }


#define LOC_WH QString("IPTVSH(%1): ").arg(m_parent->_device)

void IPTVStreamHandlerWriteHelper::timerEvent(QTimerEvent*)
{
    if (!m_parent->m_buffer->HasAvailablePacket())
        return;

    while (!m_parent->m_use_rtp_streaming)
    {
        UDPPacket packet(m_parent->m_buffer->PopDataPacket());

        if (packet.GetDataReference().isEmpty())
            break;

        int remainder = 0;
        {
            QMutexLocker locker(&m_parent->_listener_lock);
            QByteArray &data = packet.GetDataReference();
            IPTVStreamHandler::StreamDataList::const_iterator sit;
            sit = m_parent->_stream_data_list.begin();
            for (; sit != m_parent->_stream_data_list.end(); ++sit)
            {
                remainder = sit.key()->ProcessData(
                    reinterpret_cast<const unsigned char*>(data.data()),
                    data.size());
            }
        }

        if (remainder != 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC_WH +
                QString("data_length = %1 remainder = %2")
                .arg(packet.GetDataReference().size()).arg(remainder));
        }

        m_parent->m_buffer->FreePacket(packet);
    }

    while (m_parent->m_use_rtp_streaming)
    {
        RTPDataPacket packet(m_parent->m_buffer->PopDataPacket());

        if (!packet.IsValid())
            break;

        if (packet.GetPayloadType() == RTPDataPacket::kPayLoadTypeTS)
        {
            RTPTSDataPacket ts_packet(packet);

            if (!ts_packet.IsValid())
            {
                m_parent->m_buffer->FreePacket(packet);
                continue;
            }

            uint exp_seq_num = m_last_sequence_number + 1;
            uint seq_num = ts_packet.GetSequenceNumber();
            if (m_last_sequence_number &&
                ((exp_seq_num&0xFFFF) != (seq_num&0xFFFF)))
            {
                LOG(VB_RECORD, LOG_INFO, LOC_WH +
                    QString("Sequence number mismatch %1!=%2")
                    .arg(seq_num).arg(exp_seq_num));
            }
            m_last_sequence_number = seq_num;

            m_parent->_listener_lock.lock();

            int remainder = 0;
            IPTVStreamHandler::StreamDataList::const_iterator sit;
            sit = m_parent->_stream_data_list.begin();
            for (; sit != m_parent->_stream_data_list.end(); ++sit)
            {
                remainder = sit.key()->ProcessData(
                    ts_packet.GetTSData(), ts_packet.GetTSDataSize());
            }

            m_parent->_listener_lock.unlock();

            if (remainder != 0)
            {
                LOG(VB_RECORD, LOG_INFO, LOC_WH +
                    QString("data_length = %1 remainder = %2")
                    .arg(ts_packet.GetTSDataSize()).arg(remainder));
            }
        }

        m_parent->m_buffer->FreePacket(packet);
    }
}
