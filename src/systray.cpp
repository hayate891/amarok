//
// AmarokSystray
//
// Author: Stanislav Karchebny <berkus@users.sf.net>, (C) 2003
//
// Copyright: like rest of amaroK
//

#include "systray.h"
#include "app.h"
#include "enginecontroller.h"

#include <qevent.h>
#include <kaction.h>
#include <klocale.h>
#include <kpopupmenu.h>


amaroK::TrayIcon::TrayIcon( QWidget *playerWidget, KActionCollection *ac ) : KSystemTray( playerWidget )
{
    setPixmap( KSystemTray::loadIcon("amarok") ); // @since 3.2
    setAcceptDrops( true );

    ac->action( "prev"  )->plug( contextMenu() );
    ac->action( "play"  )->plug( contextMenu() );
    ac->action( "pause" )->plug( contextMenu() );
    ac->action( "stop"  )->plug( contextMenu() );
    ac->action( "next"  )->plug( contextMenu() );

    QPopupMenu &p = *contextMenu();
    QStringList shortcuts; shortcuts << "" << "Z" << "X" << "C" << "V" << "B";
    QString body = "|&%1| %2";

    for( uint index = 1; index < 6; ++index )
    {
        int id = p.idAt( index );
        p.changeItem( id, body.arg( *shortcuts.at( index ), p.text( id ) ) );
    }

    contextMenu()->insertSeparator();

    ac->action( "options_configure" )->plug( contextMenu() );

    //seems to be necessary
    actionCollection()->action( "file_quit" )->disconnect();
    connect( actionCollection()->action( "file_quit" ), SIGNAL( activated() ), kapp, SLOT( quit() ) );
}

bool
amaroK::TrayIcon::event( QEvent *e )
{
    switch( e->type() ) {
    case QEvent::Wheel:
    case QEvent::DragEnter:
    case QEvent::Drop:

        //ignore() doesn't pass the event to parent unless
        //the parent isVisible() and the active window!

        //send the event to the parent PlayerWidget, it'll handle it with much wisdom
        QApplication::sendEvent( parentWidget(), e );
        return TRUE;

    case QEvent::MouseButtonPress:
        if( static_cast<QMouseEvent*>(e)->button() == Qt::MidButton )
        {
            EngineController::instance()->playPause();

            return TRUE;
        }

    default:
        return KSystemTray::event( e );
    }
}

#include "systray.moc"
