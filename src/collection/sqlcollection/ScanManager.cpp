/*
 *  Copyright (c) 2003-2005 Mark Kretschmann <kretschmann@kde.org>
 *  Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>
 *  Copyright (c) 2007 Casey Link <unnamedrambler@gmail.com>
 *  Copyright (c) 2008 Leo Franchi <lfranchi@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ScanManager.h"

#include "amarokconfig.h"
#include "statusbar/StatusBar.h"
#include "statusbar/progressBar.h"
#include "Debug.h"
#include "meta/MetaConstants.h"
#include "meta/MetaUtility.h"
#include "mountpointmanager.h"
#include "playlistmanager/PlaylistManager.h"
#include "ScanResultProcessor.h"
#include "SqlCollection.h"

#include <QFileInfo>
#include <QListIterator>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <QThread>
#include <QTextCodec>
#include <QXmlStreamAttributes>

#include <threadweaver/ThreadWeaver.h>

static const int MAX_RESTARTS = 80;
static const int MAX_FAILURE_PERCENTAGE = 5;

ScanManager::ScanManager( SqlCollection *parent )
    :QObject( parent )
    , m_collection( parent )
    , m_scanner( 0 )
    , m_parser( 0 )
    , m_restartCount( 0 )
    , m_isIncremental( false )
    , m_blockScan( false )
{
    //nothing to do
}

ScanManager::~ScanManager()
{
    DEBUG_BLOCK

    if( m_parser ) {
        m_parser->requestAbort();
        while( !m_parser->isFinished() ) {};
        delete m_parser;
    }
}


void
ScanManager::startFullScan()
{
    DEBUG_BLOCK
    if( m_parser )
    {
        debug() << "scanner already running";
        return;
    }
    if( m_blockScan )
    {
        debug() << "scanning currently blocked";
        return;
    }
    cleanTables();
    m_scanner = new AmarokProcess( this );
    *m_scanner << "amarokcollectionscanner" << "--nocrashhandler" << "-p";
    if( AmarokConfig::scanRecursively() ) *m_scanner << "-r";
    *m_scanner << MountPointManager::instance()->collectionFolders();
    m_scanner->setOutputChannelMode( KProcess::OnlyStdoutChannel );
    connect( m_scanner, SIGNAL( readyReadStandardOutput() ), this, SLOT( slotReadReady() ) );
    connect( m_scanner, SIGNAL( finished( int ) ), SLOT( slotFinished(  ) ) );
    connect( m_scanner, SIGNAL( error( QProcess::ProcessError ) ), SLOT( slotError( QProcess::ProcessError ) ) );
    m_scanner->start();
    if( m_parser )
    {
        //TODO remove old parser, make sure this code actually works
        m_parser->requestAbort();
        ThreadWeaver::Weaver::instance()->dequeue( m_parser );
        m_parser->deleteLater();
    }
    m_parser = new XmlParseJob( this, m_collection );
    m_parser->setFilesAddedHash( &m_filesAdded );
    m_parser->setFilesDeletedHash( &m_filesDeleted );
    m_parser->setChangedUrlsHash( &m_changedUrls );
    m_parser->setIsIncremental( false );
    m_isIncremental = false;
    connect( m_parser, SIGNAL( done( ThreadWeaver::Job* ) ), SLOT( slotJobDone() ) );
    ThreadWeaver::Weaver::instance()->enqueue( m_parser );
}

void ScanManager::startIncrementalScan()
{
    DEBUG_BLOCK
    if( m_parser )
    {
        debug() << "scanner already running";
        return;
    }
    if( m_blockScan )
    {
        debug() << "scanning currently blocked";
        return;
    }
    QStringList dirs = getDirsToScan();
    if( dirs.isEmpty() )
    {
        return;
    }
    m_scanner = new AmarokProcess( this );
    *m_scanner << "amarokcollectionscanner" << "--nocrashhandler" << "-i";
    if( AmarokConfig::scanRecursively() ) *m_scanner << "-r";
    *m_scanner << dirs;
    m_scanner->setOutputChannelMode( KProcess::OnlyStdoutChannel );
    connect( m_scanner, SIGNAL( readyReadStandardOutput() ), this, SLOT( slotReadReady() ) );
    connect( m_scanner, SIGNAL( finished( int ) ), SLOT( slotFinished() ) );
    connect( m_scanner, SIGNAL( error( QProcess::ProcessError ) ), SLOT( slotError( QProcess::ProcessError ) ) );
    m_scanner->start();
    if( m_parser )
    {
        //TODO remove old parser, make sure this code actually works
        m_parser->requestAbort();
        ThreadWeaver::Weaver::instance()->dequeue( m_parser );
        m_parser->deleteLater();
    }
    m_parser = new XmlParseJob( this, m_collection );
    m_parser->setFilesAddedHash( &m_filesAdded );
    m_parser->setFilesDeletedHash( &m_filesDeleted );
    m_parser->setChangedUrlsHash( &m_changedUrls );
    m_parser->setIsIncremental( true );
    m_isIncremental = true;
    connect( m_parser, SIGNAL( done( ThreadWeaver::Job* ) ), SLOT( slotJobDone() ) );
    ThreadWeaver::Weaver::instance()->enqueue( m_parser );
}

bool
ScanManager::isDirInCollection( QString path )
{
    if ( path.endsWith( '/' ) )
        path = path.left( path.length() - 1 );
    int deviceid = MountPointManager::instance()->getIdForUrl( path );
    QString rpath = MountPointManager::instance()->getRelativePath( deviceid, path );

    QStringList values =
            m_collection->query( QString( "SELECT changedate FROM directories WHERE dir = '%2' AND deviceid = %1;" )
            .arg( QString::number( deviceid ), m_collection->escape( rpath ) ) );

    return !values.isEmpty();
}

bool
ScanManager::isFileInCollection( const QString &url  )
{
    int deviceid = MountPointManager::instance()->getIdForUrl( url );
    QString rpath = MountPointManager::instance()->getRelativePath( deviceid, url );

    QString sql = QString( "SELECT id FROM urls WHERE rpath = '%2' AND deviceid = %1" )
            .arg( QString::number( deviceid ),  m_collection->escape( rpath ) );
    if ( deviceid == -1 )
    {
        sql += ';';
    }
    else
    {
        QString rpath2 = '.' + url;
        sql += QString( " OR rpath = '%1' AND deviceid = -1;" )
                .arg( m_collection->escape( rpath2 ) );
    }
    QStringList values = m_collection->query( sql );

    return !values.isEmpty();
}

void
ScanManager::setBlockScan( bool blockScan )
{
    m_blockScan = blockScan;
    //TODO what happens if the collection scanner is currently running?
}

void
ScanManager::slotReadReady()
{
    QByteArray line;
    QString newData;
    line = m_scanner->readLine();

    while( !line.isEmpty() ) {
        //important! see
        //http://www.qtcentre.org/forum/f-general-programming-9/t-passing-to-a-console-application-managed-via-qprocess-utf-8-encoded-parameters-5375.html
        //for an explanation of the QString::fromLocal8Bit call
#ifdef Q_OS_WIN32
        QString data = QTextCodec::codecForName( "UTF-8" )->toUnicode( line ); // on windows we're UTF-8 regardless of what the codepage says
#else
        QString data = QString::fromLocal8Bit( line );
#endif
        if( !data.startsWith( "exepath=" ) ) // skip binary location info from scanner
            newData += data;
        line = m_scanner->readLine();
    }
    if( m_parser )
        m_parser->addNewXmlData( newData );
}

void
ScanManager::slotFinished( )
{
    DEBUG_BLOCK

    //make sure that we read the complete buffer
    slotReadReady();
    m_scanner->deleteLater();
    m_scanner = 0;
    m_restartCount = 0;
}

void
ScanManager::slotError( QProcess::ProcessError error )
{
    DEBUG_BLOCK
    if( error == QProcess::Crashed )
    {
        handleRestart();
    }
}

QStringList
ScanManager::getDirsToScan() const
{
    DEBUG_BLOCK

    const IdList list = MountPointManager::instance()->getMountedDeviceIds();
    QString deviceIds;
    foreach( int id, list )
    {
        if ( !deviceIds.isEmpty() ) deviceIds += ',';
        deviceIds += QString::number( id );
    }

    const QStringList values = m_collection->query(
            QString( "SELECT id, deviceid, dir, changedate FROM directories WHERE deviceid IN (%1);" )
            .arg( deviceIds ) );

    QList<int> changedFolderIds;

    QStringList result;
    for( QListIterator<QString> iter( values ); iter.hasNext(); )
    {
        int id = iter.next().toInt();
        int deviceid = iter.next().toInt();
        const QString folder = MountPointManager::instance()->getAbsolutePath( deviceid, iter.next() );
        const uint mtime = iter.next().toUInt();

        QFileInfo info( folder );
        if( info.exists() )
        {
            if( info.lastModified().toTime_t() != mtime )
            {
                result << folder;
                changedFolderIds << id;
//                 debug() << "Collection dir changed: " << folder;
            }
        }
        else
        {
            // this folder has been removed
            result << folder;
            changedFolderIds << id;
//             debug() << "Collection dir removed: " << folder;
        }
    }
    {
        QString ids;
        foreach( int id, changedFolderIds )
        {
            if( !ids.isEmpty() )
                ids += ',';
            ids += QString::number( id );
        }
        QString query = QString( "SELECT id FROM urls WHERE directory IN ( %1 );" ).arg( ids );
        QStringList urlIds = m_collection->query( query );
        ids.clear();
        foreach( const QString &id, urlIds )
        {
            if( !ids.isEmpty() )
                ids += ',';
            ids += id;
        }
        QString sql = QString( "DELETE FROM tracks WHERE url IN ( %1 );" ).arg( ids );
        m_collection->query( sql );
    }
// //     if( result.isEmpty() )
// //         debug() << "incremental scan not necessary";
// // //     else
// // //         debug() << "scanning dirs: " << result;
    return result;
}

void
ScanManager::slotJobDone()
{
    m_parser->deleteLater();
    m_parser = 0;

    //this must be done here instead of in the ScanResultProcessor because that's running
    //in a different thread, and when notifyObservers is called it does GUI things...
    foreach( const QString &key, m_changedUrls.keys() )
    {
        if( m_collection->registry()->checkUidExists( key ) )
        {
            Meta::TrackPtr track = m_collection->registry()->getTrackFromUid( key );
            if( track )
                KSharedPtr<Meta::SqlTrack>::staticCast( track )->setUrl( m_changedUrls[key] );
        }
    }   
}

void
ScanManager::handleRestart()
{
    DEBUG_BLOCK
    //TODO handle collection scanner crash
    m_restartCount++;
    debug() << "Collection scanner crashed, restart count is " << m_restartCount;
    if( m_restartCount >= MAX_RESTARTS )
    {
        //TODO:abort scan, inform user
    }
    else
    {
        if( m_parser )
        {
        //TODO remove old parser, make sure this code actually works
            m_parser->requestAbort();
            ThreadWeaver::Weaver::instance()->dequeue( m_parser );
            m_parser->deleteLater();
            m_parser = 0;
        }
        disconnect( m_scanner, SIGNAL( readyReadStandardOutput() ), this, SLOT( slotReadReady() ) );
        disconnect( m_scanner, SIGNAL( finished( int ) ), this, SLOT( slotFinished(  ) ) );
        disconnect( m_scanner, SIGNAL( error( QProcess::ProcessError ) ), this, SLOT( slotError( QProcess::ProcessError ) ) );
        m_scanner->kill();
        m_scanner->deleteLater();
        m_scanner = 0;
        QTimer::singleShot( 0, this, SLOT( restartScanner() ) );
    }
}

void
ScanManager::restartScanner()
{
    DEBUG_BLOCK
    m_scanner = new AmarokProcess( this );
    *m_scanner << "amarokcollectionscanner" << "--nocrashhandler";
    if( m_isIncremental )
    {
        *m_scanner << "-i";
    }
    *m_scanner << "-s";
    m_scanner->setOutputChannelMode( KProcess::OnlyStdoutChannel );
    connect( m_scanner, SIGNAL( readyReadStandardOutput() ), this, SLOT( slotReadReady() ) );
    connect( m_scanner, SIGNAL( finished( int ) ), SLOT( slotFinished(  ) ) );
    connect( m_scanner, SIGNAL( error( QProcess::ProcessError ) ), SLOT( slotError( QProcess::ProcessError ) ) );
    m_scanner->start();
    m_parser = new XmlParseJob( this, m_collection );
    m_parser->setIsIncremental( m_isIncremental );
    m_parser->setFilesAddedHash( &m_filesAdded );
    m_parser->setFilesDeletedHash( &m_filesDeleted );
    m_parser->setChangedUrlsHash( &m_changedUrls );
    connect( m_parser, SIGNAL( done( ThreadWeaver::Job* ) ), SLOT( slotJobDone() ) );
    ThreadWeaver::Weaver::instance()->enqueue( m_parser );
}

void
ScanManager::cleanTables()
{
    m_collection->query( "DELETE FROM tracks;" );
    m_collection->query( "DELETE FROM genres;" );
    m_collection->query( "DELETE FROM years;" );
    m_collection->query( "DELETE FROM composers;" );
    m_collection->query( "DELETE FROM albums;" );
    m_collection->query( "DELETE FROM artists;" );
}

//XmlParseJob

XmlParseJob::XmlParseJob( ScanManager *parent, SqlCollection *collection )
    : ThreadWeaver::Job( parent )
    , m_collection( collection )
    , m_abortRequested( false )
    , m_isIncremental( false )
    , m_filesAdded( 0 )
    , m_filesDeleted( 0 )
    , m_changedUrls( 0 )
{
    DEBUG_BLOCK
    The::statusBar()->newProgressOperation( this )
            .setDescription( i18n( "Scanning music" ) )
            .setAbortSlot( parent, SLOT( deleteLater() ) );

    connect( this, SIGNAL( incrementProgress() ), The::statusBar(), SLOT( incrementProgress() ), Qt::QueuedConnection );
}

XmlParseJob::~XmlParseJob()
{
    DEBUG_BLOCK
    The::statusBar()->endProgressOperation( this );
}

void
XmlParseJob::setIsIncremental( bool incremental )
{
    m_isIncremental = incremental;
}

void
XmlParseJob::setFilesAddedHash( QHash<QString, QString>* hash )
{
    m_filesAdded = hash;
}

void
XmlParseJob::setFilesDeletedHash( QHash<QString, QString>* hash )
{
    m_filesDeleted = hash;
}

void
XmlParseJob::setChangedUrlsHash( QHash<QString, QString>* hash )
{
    m_changedUrls = hash;
}

void
XmlParseJob::run()
{
    DEBUG_BLOCK
    QList<QVariantMap > directoryData;
    bool firstTrack = true;
    QString currentDir;

    ScanResultProcessor processor( m_collection );
    processor.setFilesDeletedHash( m_filesDeleted );
    processor.setChangedUrlsHash( m_changedUrls );
    if( m_isIncremental )
    {
        processor.setScanType( ScanResultProcessor::IncrementalScan );
    }
    else
    {
        processor.setScanType( ScanResultProcessor::FullScan );
    }
    
    do
    {
        m_abortMutex.lock();
        bool abort = m_abortRequested;
        m_abortMutex.unlock();
        if( abort )
        {
            break;
        }
        //get new xml data or wait till new xml data is available
        m_mutex.lock();
        if( m_nextData.isEmpty() )
        {
            m_wait.wait( &m_mutex );
        }
        m_reader.addData( m_nextData );
        m_nextData.clear();
        m_mutex.unlock();

        while( !m_reader.atEnd() )
        {
            m_reader.readNext();
            if( m_reader.isStartElement() )
            {
                QStringRef localname = m_reader.name();
                if( localname == "dud" || localname == "tags" || localname == "playlist" )
                {
                    emit incrementProgress();
                }
                if( localname == "itemcount" )
                {
                    The::statusBar()->incrementProgressTotalSteps( this, m_reader.attributes().value( "count" ).toString().toInt() );
                }
                else if( localname == "tags" )
                {
                    //TODO handle tag data
                    QXmlStreamAttributes attrs = m_reader.attributes();
                    QVariantMap data;
                    data.insert( Meta::Field::URL, attrs.value( "path" ).toString() );
                    data.insert( Meta::Field::TITLE, attrs.value( "title" ).toString() );
                    data.insert( Meta::Field::ARTIST, attrs.value( "artist" ).toString() );
                    data.insert( Meta::Field::COMPOSER, attrs.value( "composer" ).toString() );
                    data.insert( Meta::Field::ALBUM, attrs.value( "album" ).toString() );
                    data.insert( Meta::Field::COMMENT, attrs.value( "comment" ).toString() );
                    data.insert( Meta::Field::GENRE, attrs.value( "genre" ).toString() );
                    data.insert( Meta::Field::YEAR, attrs.value( "year" ).toString() );
                    data.insert( Meta::Field::TRACKNUMBER, attrs.value( "track" ).toString() );
                    data.insert( Meta::Field::DISCNUMBER, attrs.value( "discnumber" ).toString() );
                    data.insert( Meta::Field::BPM, attrs.value( "bpm" ).toString() );
                    //filetype and uniqueid are missing in the fields, compilation is not used here
                    if( attrs.value( "audioproperties" ) == "true" )
                    {
                        data.insert( Meta::Field::BITRATE, attrs.value( "bitrate" ).toString() );
                        data.insert( Meta::Field::LENGTH, attrs.value( "length" ).toString() );
                        data.insert( Meta::Field::SAMPLERATE, attrs.value( "samplerate" ).toString() );
                    }
                    if( !attrs.value( "filesize" ).isEmpty() )
                        data.insert( Meta::Field::FILESIZE, attrs.value( "filesize" ).toString() );

                    data.insert( Meta::Field::UNIQUEID, attrs.value( "uniqueid" ).toString() );
                    if( m_filesAdded )
                        m_filesAdded->insert( attrs.value( "uniqueid").toString(), attrs.value( "path" ).toString() );

                    KUrl url( data.value( Meta::Field::URL ).toString() );
                    if( firstTrack )
                    {
                        currentDir = url.directory();
                        firstTrack = false;
                    }

                    if( url.directory() == currentDir )
                    {
                        directoryData.append( data );
                    }
                    else
                    {
                        processor.processDirectory( directoryData );
                        directoryData.clear();
                        directoryData.append( data );
                        currentDir = url.directory();
                    }
                }
                else if( localname == "folder" )
                {
                    QXmlStreamAttributes attrs = m_reader.attributes();
                    const QString folder = attrs.value( "path" ).toString();
                    const QFileInfo info( folder );

                    processor.addDirectory( folder, info.lastModified().toTime_t() );

                    /*// Update dir statistics for rescanning purposes
                    if( info.exists() )
                        CollectionDB::instance()->updateDirStats( folder, info.lastModified().toTime_t(), true);

                    if( m_incremental ) {
                        m_foldersToRemove += folder;
                    }*/
                }
                else if( localname == "playlist" )
                {
                    //TODO check for duplicates
                    The::playlistManager()->save( m_reader.attributes().value( "path" ).toString() );
                }
                else if( localname == "image" )
                {
                    QXmlStreamAttributes attrs = m_reader.attributes();
                    // Deserialize CoverBundle list
                    QStringList list = attrs.value( "list" ).toString().split( "AMAROK_MAGIC" );
                    QList< QPair<QString, QString> > covers;
                   
                    // Don't iterate if the list only has one element
                    if( list.size() > 1 )
                    {
                        for( int i = 0; i < list.count(); i += 2 )
                            covers += qMakePair( list[i], list[i + 1] );

                        processor.addImage( attrs.value( "path" ).toString(), covers );
                    }
                }
            }
        }
    }
    while( m_reader.error() == QXmlStreamReader::PrematureEndOfDocumentError );
    if( m_abortRequested || m_reader.error() != QXmlStreamReader::NoError )
    {
        debug() << "do-while done with error";
        //the error cannot be PrematureEndOfDocumentError, so handle
        //an unrecoverable error here
        processor.rollback();
    }
    else
    {
        if( !directoryData.isEmpty() )
            processor.processDirectory( directoryData );
        processor.commit();
        if( !m_isIncremental )
        {
            m_collection->emitFilesDeleted( *m_filesDeleted );
            m_collection->emitFilesAdded( *m_filesAdded );
        }
        else
        {
            QHash<QString, QString>::Iterator it;
            for( it = m_filesAdded->begin(); it != m_filesAdded->end(); ++it )
            {
                if( m_filesDeleted->contains( it.key() ) )
                    m_filesDeleted->remove( it.key() );
            }
            for( it = m_filesAdded->begin(); it != m_filesAdded->end(); ++it )
                m_collection->emitFileAdded( it.value(), it.key() );
            for( it = m_filesDeleted->begin(); it != m_filesDeleted->end(); ++it )
                m_collection->emitFileDeleted( it.value(), it.key() );
        }
    } 
}

void
XmlParseJob::addNewXmlData( const QString &data )
{
    m_mutex.lock();
    //append the new xml data because the parser thread
    //might not have retrieved all xml data yet
    m_nextData += data;
    m_wait.wakeOne();
    m_mutex.unlock();
}

void
XmlParseJob::requestAbort()
{
    DEBUG_BLOCK

    m_abortMutex.lock();
    m_abortRequested = true;
    m_abortMutex.unlock();
    m_wait.wakeOne();
}

#include "ScanManager.moc"

