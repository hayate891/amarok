/***************************************************************************
                      titleproxy.h  -  description
                         -------------------
begin                : Nov 20 14:35:18 CEST 2003
copyright            : (C) 2003 by Mark Kretschmann
email                :
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TITLEPROXY_H
#define TITLEPROXY_H

#include <kextendedsocket.h>
#include <kurl.h>

#include <qobject.h>

class QString;

/**
 * @brief: Proxy for decoding Shoutcast metadata.
 */

class TitleProxy : public QObject
{
        Q_OBJECT

    public:
        TitleProxy( KURL url );
        ~TitleProxy();

        KURL proxyUrl();

        // ATTRIBUTES ------
        struct metaPacket {
            QString streamName;
            QString streamGenre;
            QString streamUrl;
            QString bitRate;
            QString title;
            QString url;
        };
        
    signals:
        void metaData( TitleProxy::metaPacket );

    public slots:

    private slots:
        void readRemote();
        void processHeader( Q_LONG &index, Q_LONG bytesRead );
        void accept();

    private:
        void transmitData( const QString &data );
        QString extractStr( const QString &str, const QString &key );

        // ATTRIBUTES ------
        KURL m_url;
        bool m_initSuccess;
        int m_metaInt;
        QString m_bitRate;
        int m_byteCount;
        uint m_metaLen;
        QString m_metaData;
        bool m_headerFinished;
        QString m_headerStr;
        int m_usedPort;

        QString m_streamName;
        QString m_streamGenre;
        QString m_streamUrl;

        char *m_pBuf;

        KExtendedSocket m_sockRemote;
        KExtendedSocket m_sockPassive;
        KExtendedSocket *m_pSockProxy;
};
#endif
