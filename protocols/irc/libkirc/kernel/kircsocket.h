/*
    kircsocket.h - IRC socket.

    Copyright (c) 2003-2007 by Michel Hermier <michel.hermier@gmail.com>
    Copyright (c) 2006      by Tommi Rantala <tommi.rantala@cs.helsinki.fi>

    Kopete    (c) 2002-2007 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef KIRCSOCKET_H
#define KIRCSOCKET_H

#include "kircconst.h"
#include "kircentity.h"
#include "kircevent.h"
#include "kircmessage.h"

#include <QTcpSocket>

class QUrl;

class QTextCodec;

namespace KIrc
{

class CommandHandler;
class EntityManager;
class Event;

/**
 * @author Michel Hermier <michel.hermier@gmail.com>
 */
class KIRC_EXPORT Socket
	: public QObject
{
	Q_OBJECT

	Q_PROPERTY(ConnectionState connectionState READ connectionState)
	Q_ENUMS(ConnectionState)

public:
	enum ConnectionState
	{
		Idle,
		HostLookup,
		HostFound,
//		Bound,
		Connecting,
		Open,
		Authentifying,
		Authentified,
		Closing
	};

	explicit Socket(QObject *parent = 0);
	~Socket();

public: // READ properties accessors.
	ConnectionState connectionState() const;

public:
	QTcpSocket *socket();

	QTextCodec *defaultCodec() const;

	CommandHandler *commandHandler() const;
	EntityManager *entityManager() const;
	Entity::Ptr owner() const;

	/**
	 * The connection url.
	 */
	const QUrl &url() const;

public slots:
	void setDefaultCodec(QTextCodec *codec);

	void setCommandHandler(CommandHandler *newCommandHandler);
	void setEntityManager(EntityManager *newEntityManager);
	void setOwner(const Entity::Ptr &newOwner);

	void connectToServer(const QUrl &url);
	void close();

	void writeMessage(const Message &message);

	void showInfoDialog();

signals:
//	void eventOccured(const KIrc::Event *);

	void connectionStateChanged(Socket::ConnectionState newState);

	void receivedMessage(const KIrc::Message &message);

protected:
	void setConnectionState(Socket::ConnectionState newstate);

private slots:
	void onReadyRead();

	void socketStateChanged(QAbstractSocket::SocketState);
	void socketGotError(QAbstractSocket::SocketError);

private:
	Q_DISABLE_COPY(Socket)

	class Private;
	Private * const d;
};

}

#endif

