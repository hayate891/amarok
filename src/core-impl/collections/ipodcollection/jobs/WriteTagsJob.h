/****************************************************************************************
 * Copyright (c) 2012 Matěj Laitl <matej@laitl.cz>                                      *
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

#ifndef WRITETAGSJOB_H
#define WRITETAGSJOB_H

#include "MetaValues.h"

#include <ThreadWeaver/Job>


/**
 * Calls Meta::Tag::writeTags( path, changedFields ) in a thread so that main thread
 * is not blocked with IO. writeTags() respects AmarokConfig::writeBackStatistics,
 * AmarokConfig::writeBack().
 *
 * If @param changes contains IpodMeta::valImage, writes back image too, respecting
 * AmarokConfig.
 *
 * The caller is responsible to delete this job after use, perhaps by connecting its
 * done() signal to its deleteLater() slot.
 */
class WriteTagsJob : public ThreadWeaver::Job
{
    Q_OBJECT

    public:
        explicit WriteTagsJob( const QString &path, const Meta::FieldHash &changes );
        virtual void run();

    private:
        const QString m_path;
        const Meta::FieldHash m_changes;
};

#endif // WRITETAGSJOB_H
