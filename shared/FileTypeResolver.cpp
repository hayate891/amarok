/****************************************************************************************
 * Copyright (c) 2005 Martin Aumueller <aumueller@reserv.at>                            *
 * Copyright (c) 2011 Ralf Engels <ralf-engels@gmx.de>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "FileTypeResolver.h"

#include <QFile>
#include <QFileInfo>
#include <QtDebug>

#include <kmimetype.h>

#include <fileref.h>

#ifdef TAGLIB_EXTRAS_FOUND
#include <audiblefile.h>
#include <realmediafile.h>
#endif
#include <aifffile.h>
#include <asffile.h>
#include <flacfile.h>
#include <mp4file.h>
#include <mpcfile.h>
#include <mpegfile.h>
#include <oggfile.h>
#include <oggflacfile.h>
#include <speexfile.h>
#include <trueaudiofile.h>
#include <vorbisfile.h>
#include <wavfile.h>
#include <wavpackfile.h>

TagLib::File *Meta::Tag::FileTypeResolver::createFile(TagLib::FileName fileName,
        bool readProperties,
        TagLib::AudioProperties::ReadStyle propertiesStyle) const
{
    TagLib::File* result = 0;

    QString fn = QFile::decodeName( fileName );
    QString suffix = QFileInfo( fn ).suffix();
    KMimeType::Ptr mimetype = KMimeType::findByPath( fn );

    // -- check by mime type
    if( mimetype->is( QLatin1String("audio/mpeg") )
            || mimetype->is( QLatin1String("audio/x-mpegurl") )
            || mimetype->is( QLatin1String("audio/mpeg") ))
    {
        result = new TagLib::MPEG::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/mp4") )
             || mimetype->is( QLatin1String("video/mp4") ) )
    {
        result = new TagLib::MP4::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-ms-wma") )
            || mimetype->is( QLatin1String("video/x-ms-asf") )
            || mimetype->is( QLatin1String("video/x-msvideo") )
            || mimetype->is( QLatin1String("video/x-ms-wmv") ) )
    {
        result = new TagLib::ASF::File(fileName, readProperties, propertiesStyle);
    }
#ifdef TAGLIB_EXTRAS_FOUND
    else if( mimetype->is( QLatin1String("audio/vnd.rn-realaudio") )
            || mimetype->is( QLatin1String("audio/x-pn-realaudioplugin") )
            || mimetype->is( QLatin1String("audio/vnd.rn-realvideo") ) )
    {
        result = new TagLibExtras::RealMedia::File(fileName, readProperties, propertiesStyle);
    }
#endif
    else if( mimetype->is( QLatin1String("audio/x-vorbis+ogg") ) )
    {
        result = new TagLib::Ogg::Vorbis::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-flac+ogg") ) )
    {
        result = new TagLib::Ogg::FLAC::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-aiff") ) )
    {
        result = new TagLib::RIFF::AIFF::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-flac") ) )
    {
        result = new TagLib::FLAC::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-musepack") ) )
    {
        result = new TagLib::MPC::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-wav") ) )
    {
        result = new TagLib::RIFF::WAV::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-wavpack") ) )
    {
        result = new TagLib::WavPack::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-tta") ) )
    {
        result = new TagLib::TrueAudio::File(fileName, readProperties, propertiesStyle);
    }
    else if( mimetype->is( QLatin1String("audio/x-speex") )
             || mimetype->is( QLatin1String("audio/x-speex+ogg") ) )
    {
        result = new TagLib::TrueAudio::File(fileName, readProperties, propertiesStyle);
    }

    // -- check by extension
    else if( suffix == QLatin1String("m4a")
        || suffix == QLatin1String("m4b")
        || suffix == QLatin1String("m4p")
        || suffix == QLatin1String("mp4")
        || suffix == QLatin1String("m4v")
        || suffix == QLatin1String("mp4v") )
    {
        result = new TagLib::MP4::File(fileName, readProperties, propertiesStyle);
    }
    else if( suffix == QLatin1String("wav") )
    {
        result = new TagLib::RIFF::WAV::File(fileName, readProperties, propertiesStyle);
    }
    else if( suffix == QLatin1String("wma")
             || suffix == QLatin1String("asf") )
    {
        result = new TagLib::ASF::File(fileName, readProperties, propertiesStyle);
    }

#ifndef Q_WS_WIN
     if( !result )
         qDebug() << "kmimetype filetype guessing failed for" << fileName;
#endif

    if( result && !result->isValid() ) {
        delete result;
        result = 0;
    }

    return result;
}
