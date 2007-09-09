/***************************************************************************
 *   Copyright (c) 2007  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02111-1307, USA.         *
 ***************************************************************************/

#include "ServiceListDelegate.h"

#include "debug.h"
#include "servicebase.h"
#include "ServiceListModel.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>


ServiceListDelegate::ServiceListDelegate()
 : QItemDelegate()
{
    DEBUG_BLOCK


    QFile file( KStandardDirs::locate( "data","amarok/images/service-browser-element.svg" ) );
    file.open( QIODevice::ReadOnly );
    QString svg_source( file.readAll() );


    m_svgRendererActive = new  QSvgRenderer( svg_source.toAscii() );
    svg_source.replace("stop-color:#6193cf", "stop-color:" + QApplication::palette().window().color().name() );
    m_svgRendererInactive = new  QSvgRenderer( svg_source.toAscii() );


}

ServiceListDelegate::~ServiceListDelegate()
{
    delete m_svgRendererActive;
    delete m_svgRendererInactive;
}

void ServiceListDelegate::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    //DEBUG_BLOCK

    debug() << "Look ma' I am painting!";

    painter->save();
    painter->setRenderHint ( QPainter::Antialiasing );

    //lets try yo have some fun with an svg...

    QSvgRenderer * svgRenderer;

    if (option.state & QStyle::State_Selected)
        svgRenderer = m_svgRendererActive;
    else 
        svgRenderer = m_svgRendererInactive;

    svgRenderer->render ( painter,  QRectF( option.rect.topLeft().x() + 2, option.rect.topLeft().y() + 2 ,250,76 ) );


    if (option.state & QStyle::State_Selected)
        painter->setPen(Qt::blue);
    else 
        painter->setPen(Qt::gray);

    //painter->drawRoundRect( option.rect.topLeft().x() + 2, option.rect.topLeft().y() + 2 ,250,66, 8 ,8 );

    if (option.state & QStyle::State_Selected)
        painter->setPen(Qt::blue);
    else 
        painter->setPen(Qt::black);

    painter->setFont(QFont("Arial", 14));

    painter->drawPixmap( option.rect.topLeft() + QPoint( 8, 4 ) , index.data( Qt::DecorationRole ).value<QIcon>().pixmap( 32, 32 ) );

    painter->drawText( option.rect.topLeft() + QPoint( 47, 21 ) , index.data( Qt::DisplayRole ).toString() );

    painter->setFont(QFont("Arial", 10));
    
    QRectF textRect;
    textRect.setLeft( option.rect.topLeft().x() + 6 );
    textRect.setTop( option.rect.topLeft().y() + 34 );
    textRect.setWidth( 248 );
    textRect.setHeight( 44 );

    painter->drawText ( textRect, Qt::TextWordWrap | Qt::AlignHCenter | Qt::AlignHCenter, index.data( ShortDescriptionRole ).toString() );

    debug() << "Short description: " << index.data( ShortDescriptionRole ).toString() ;


    painter->restore();

}

QSize ServiceListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    Q_UNUSED( option );
    Q_UNUSED( index );

    //DEBUG_BLOCK

    return QSize ( 252, 80 );

    

}


