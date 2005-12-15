/*
    yahooconferencemessagemanager.h - Yahoo Conference Message Manager

    Copyright (c) 2003 by Duncan Mac-Vicar Prett <duncan@kde.org>

    Kopete    (c) 2002-2003 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef YAHOOCONFERENCEMESSAGEMANAGER_H
#define YAHOOCONFERENCEMESSAGEMANAGER_H

#include "kopetechatsession.h"

class KActionCollection;
class YahooContact;
class KActionMenu;

class YahooContact;

/**
 * @author Duncan Mac-Vicar Prett
 */
class YahooConferenceChatSession : public Kopete::ChatSession
{
	Q_OBJECT

public:
	YahooConferenceChatSession( const QString &m_yahooRoom, Kopete::Protocol *protocol, const Kopete::Contact *user, Kopete::ContactPtrList others, const char *name = 0 );
	~YahooConferenceChatSession();

	void joined( YahooContact *c );
	const QString &room();
signals:
	void leavingConference( YahooConferenceChatSession *s );
protected slots:
	void slotMessageSent( Kopete::Message &message, Kopete::ChatSession * );
private:
	QString m_yahooRoom;
};

#endif

// vim: set noet ts=4 sts=4 tw=4:

