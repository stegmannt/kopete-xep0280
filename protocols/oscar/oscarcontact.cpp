/*
    oscarcontact.cpp  -  Oscar Protocol Plugin

    Copyright (c) 2002 by Tom Linsky <twl6@po.cwru.edu>

    Kopete    (c) 2002 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "oscarcontact.h"

#include <time.h>

#include <qapplication.h>
#include <qregexp.h>
#include <qstylesheet.h>
#include <qtimer.h>

#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpopupmenu.h>

#include "aim.h"
#include "kopeteaway.h"
#include "kopetemessagemanager.h"
#include "kopetemessagemanagerfactory.h"
#include "kopetemetacontact.h"
#include "kopetestdaction.h"
#include "oscarprotocol.h"
#include "oscarsocket.h"
#include "oscaruserinfo.h"

OscarContact::OscarContact(const QString name, OscarProtocol *protocol,
		KopeteMetaContact *parent)
: KopeteContact(protocol, name, parent)
{
	kdDebug() << "[OscarContact] OscarContact(), name=" << name << endl;

	mName = name;
	mProtocol = protocol;
	mMsgManager = 0L;
	mIdle = 0;
	mLastAutoResponseTime = 0;
	mTypingTimer = new QTimer();
	
	// Buddy Changed
	QObject::connect(mProtocol->engine, SIGNAL(gotBuddyChange(UserInfo)),
					this,SLOT(slotBuddyChanged(UserInfo)));
	// Buddy offline
	QObject::connect(mProtocol->engine, SIGNAL(gotOffgoingBuddy(QString)),
					this,SLOT(slotOffgoingBuddy(QString)));
	// Got IM
	QObject::connect(mProtocol->engine, SIGNAL(gotIM(QString,QString,bool)),
					this,SLOT(slotIMReceived(QString,QString,bool)));
	// User's status changed (I don't understand this)
	QObject::connect(mProtocol->engine, SIGNAL(statusChanged(int)),
					this, SLOT(slotMainStatusChanged(int)));
	// Contact moved
	QObject::connect (this , SIGNAL( moved(KopeteMetaContact*,KopeteContact*) ),
			this, SLOT (slotMoved(KopeteMetaContact*) ));
	// Incoming minitype notification
	QObject::connect(mProtocol->engine, SIGNAL(gotMiniTypeNotification(QString, int)),
					this, SLOT(slotGotMiniType(QString, int)));
  // New direct connection
  QObject::connect(mProtocol->engine, SIGNAL(directIMReady(QString)),
  	this, SLOT(slotDirectIMReady(QString)));
  // Direct connection closed
  QObject::connect(mProtocol->engine, SIGNAL(directIMConnectionClosed(QString)),
  	this, SLOT(slotDirectIMConnectionClosed(QString)));
	// Set up the mTypingTimer
	QObject::connect(mTypingTimer, SIGNAL(timeout()),
					this, SLOT(slotTextEntered()));
		
	if(parent){
		connect (parent , SIGNAL( aboutToSave(KopeteMetaContact*) ),
				protocol, SLOT (serialize(KopeteMetaContact*) ));
	}
	
	initActions();
	TBuddy tmpBuddy;
	int num = mProtocol->buddyList()->getNum(mName);
	if ( mProtocol->buddyList()->get(&tmpBuddy, num) != -1 )
	{
		if ( !tmpBuddy.alias.isEmpty() ){
			setDisplayName(tmpBuddy.alias);
		} else {
			setDisplayName(tmpBuddy.name);
		}
		
		slotUpdateBuddy(num);
	}	else 	{
		setDisplayName(mName);
	}
	theContacts.append(this);
}

OscarContact::~OscarContact()
{
	kdDebug() << "[OscarContact] ~OscarContact()" << endl;
	delete mTypingTimer;
}

/** Pops up a chat window */
void OscarContact::execute(void)
{
	kdDebug() << "[OscarContact] execute()" << endl;

	if ( mStatus == OSCAR_OFFLINE )
	{
		KMessageBox::sorry(qApp->mainWidget(), i18n(
			"<qt>Sorry, this user is not online at the moment for you" \
			" to message him/her. AIM users must be online for you to be" \
			" able to message them.</qt>"), i18n("User not Online") );
		return;
	}
	msgManager()->readMessages();
}

/** Return the protocol specific serialized data that a plugin may want to store a contact list. */
QString OscarContact::data(void) const
{
	TBuddy tmpBuddy;
	int num = mProtocol->buddyList()->getNum(mName);

	if (mProtocol->buddyList()->get(&tmpBuddy, num) != -1)
		if (tmpBuddy.alias)
			return tmpBuddy.alias;
	return QString::null;
}

KopeteMessageManager* OscarContact::msgManager()
{
	if ( mMsgManager )
	{
		//printf("REturning a mmsgmanager: %d\n",mMsgManager);fflush(stdout);
		return mMsgManager;
	}
	else
	{
		//printf("Creating a mmsgmanager: %d\n",mProtocol->myself());fflush(stdout);
		mMsgManager =
				KopeteMessageManagerFactory::factory()->create(
								mProtocol->myself(), theContacts,
								mProtocol, KopeteMessageManager::ChatWindow);
		QObject::connect(
						mMsgManager,
						SIGNAL(messageSent(const KopeteMessage&, KopeteMessageManager *)),
						this,
						SLOT(slotSendMsg(const KopeteMessage&, KopeteMessageManager *)));
		// TODO
		QObject::connect(
						mMsgManager,
						SIGNAL(typingMsg(bool)),
						this,
						SLOT(slotTyping(bool)));
		
		return mMsgManager;
	}
}

void OscarContact::slotMainStatusChanged(int newStatus)
{
	if (newStatus == OSCAR_OFFLINE)
	{
		mStatus = OSCAR_OFFLINE;
		emit statusChanged( this, status() );
		// Try to do this, otherwise no big deal
		TBuddy tmpBuddy;
		int buddyNum = mProtocol->buddyList()->getNum(mName);
		if ( mProtocol->buddyList()->get(&tmpBuddy, buddyNum) == -1 )
			return;
		mProtocol->buddyList()->setStatus(buddyNum, OSCAR_OFFLINE);
	}
}

/** Called when a buddy changes */
void OscarContact::slotUpdateBuddy(int buddyNum)
{
	TBuddy tmpBuddy;

	// buddy not found in our list of buddies
	if ( mProtocol->buddyList()->get(&tmpBuddy, buddyNum) == -1 )
		return;

	QString tmpBuddyName = tocNormalize ( tmpBuddy.name );

	if ( tmpBuddyName != tocNormalize(mName) ) // that's not our contact
		return;

	// status did not change, do nothing
	if ( ( mStatus == tmpBuddy.status ) && ( mIdle == tmpBuddy.idleTime ) )
		return;

	// if we have become idle
	if ( tmpBuddy.idleTime > 0 )
	{
		kdDebug() << "[OscarContact] setting " << mName << " idle! Idletime: " << tmpBuddy.idleTime << endl;
		setIdleState(Idle);			
	}
	// we have become un-idle
	else 
	{
		kdDebug() << "[OscarContact] setting " << mName << " active!" << endl;
		setIdleState(Active);
	}

	mStatus = tmpBuddy.status;
  mIdle = tmpBuddy.idleTime;
	kdDebug() << "[OscarContact] slotUpdateBuddy(), Contact " << mName << " is now " << mStatus << endl;

	if ( mProtocol->isConnected() ) // oscar-plugin is online
	{
		if ( mName != tmpBuddyName ) // contact changed his nickname
		{
			if ( !tmpBuddy.alias.isEmpty() )
				setDisplayName(tmpBuddy.alias);
			else
				setDisplayName(tmpBuddy.name);
		}
	}
	else // oscar-plugin is offline so all users are offline too
	{
		mStatus = OSCAR_OFFLINE;
		mProtocol->buddyList()->setStatus(buddyNum, OSCAR_OFFLINE);

//		actionSendMessage->setEnabled(false);
//		actionInfo->setEnabled(false);

//		emit userStatusChanged(OSCAR_OFFLINE);
//		emit statusChanged();
		emit statusChanged( this, status() );
		return;
	}

	// We can only send messages to online user
//	actionSendMessage->setEnabled(mStatus != TAIM_OFFLINE);
//	actionInfo->setEnabled(mStatus != TAIM_OFFLINE);

	//emit userStatusChanged(tmpBuddy.status);
	//emit statusChanged();
	emit statusChanged( this, status() );
}

/** Returns the online status of the contact */
OscarContact::ContactStatus OscarContact::status(void) const
{
	if (mStatus == OSCAR_ONLINE)
		return Online;
	else if (mStatus == OSCAR_AWAY)
		return Away;
	else
		return Offline;
}

/** Initialzes the actions */
void OscarContact::initActions(void)
{
	actionCollection = 0L;

	actionWarn = new KAction(i18n("&Warn"), 0, this, SLOT(slotWarn()), this, "actionWarn");
	actionBlock = new KAction(i18n("&Block"), 0, this, SLOT(slotBlock()), this, "actionBlock");
	actionDirectConnect = new KAction(i18n("&Direct IM"), 0, this, SLOT(slotDirectConnect()), this, "actionDirectConnect");
}

/** Returns the status icon of the contact */
QString OscarContact::statusIcon(void) const
{
	if (mStatus == OSCAR_ONLINE)
		return "oscar_online";
	else if (mStatus == OSCAR_AWAY)
		return "oscar_away";
	else
		return "oscar_offline";
}

/** Called when a buddy is oncoming */
void OscarContact::slotBuddyChanged(UserInfo u)
{
	if (tocNormalize(u.sn) == tocNormalize(mName))
	//if we are the contact that is oncoming
	{
		TBuddy *tmpBuddy;
		int num = mProtocol->buddyList()->getNum(mName);
		kdDebug() << "[OscarContact] Names match... " << u.sn << endl;
		if ( (tmpBuddy = mProtocol->buddyList()->getByNum(num)) != NULL )
		{
			kdDebug() << "[OscarContact] Setting status for " << u.sn << endl;
			if ( u.userclass & USERCLASS_AWAY )
				tmpBuddy->status = OSCAR_AWAY;
			else
				tmpBuddy->status = OSCAR_ONLINE;
			tmpBuddy->evil = u.evil;
			tmpBuddy->idleTime = u.idletime;
			tmpBuddy->signonTime = u.onlinesince;
			slotUpdateBuddy(num);
		}
		else
			kdDebug() << "[OscarContact] Buddy is oncoming but is not in buddy list" << endl;
	}
}

// Called when we get a minityping notification
void OscarContact::slotGotMiniType(QString screenName, int type){
		//TODO
		// Check to see if it's us
		//kdDebug() << "[OSCAR] Minitype: Comparing "
		//					<< tocNormalize(screenName) << " and "
		//					<< tocNormalize(mName) << endl;
		
		if(tocNormalize(screenName) != tocNormalize(mName)){
				return;
		}
		
		kdDebug() << "[OSCAR] OscarContact got minitype notification for " << mName << endl;
		
		// If we already have a message manager
		if(mMsgManager){
				if(type == 2){
						mMsgManager->userTypingMsg(this, true);
				} else {
						mMsgManager->userTypingMsg(this, false);
				}
		}
		
}

// Called when we want to send a typing notification to
// the other person
void OscarContact::slotTyping(bool typing){
		kdDebug() << "[OSCAR] Sending typing notify" << endl;
		
		if(typing){
				kdDebug() << "[OSCAR TYPING] Typing" << endl;
				if(mTypingTimer->isActive()){
						mTypingTimer->stop();
						// Start the timer at 3 seconds
						mTypingTimer->start(1000*3, true);
				} else {
						mProtocol->engine->sendMiniTypingNotify(tocNormalize(mName),
								OscarSocket::TypingBegun);
						// Start the timer
						mTypingTimer->start(1000*3, true);
				}
		} else {
				kdDebug() << "[OSCAR TYPING] Finished" << endl;
				mProtocol->engine->sendMiniTypingNotify(tocNormalize(mName),
								OscarSocket::TypingFinished);
				// Stop the timer
				mTypingTimer->stop();
		}
}

void OscarContact::slotTextEntered(){
		mProtocol->engine->sendMiniTypingNotify(tocNormalize(mName),
						OscarSocket::TextTyped);
}

/** Called when a buddy is offgoing */
void OscarContact::slotOffgoingBuddy(QString sn)
{
	if (tocNormalize(sn) == tocNormalize(mName))
	//if we are the contact that is offgoing
	{
		TBuddy *tmpBuddy;
		int num = mProtocol->buddyList()->getNum(mName);
		if ( (tmpBuddy = mProtocol->buddyList()->getByNum(num)) != NULL )
		{
			tmpBuddy->status = OSCAR_OFFLINE;
			slotUpdateBuddy(num);
		}
		else
			kdDebug() << "[OscarContact] Buddy is offgoing but not in buddy list" << endl;
	}
}

/** Called when user info is requested */
void OscarContact::slotUserInfo(void)
{
	TBuddy tmpBuddy;
	int num = mProtocol->buddyList()->getNum(mName);

	if (mProtocol->buddyList()->get(&tmpBuddy, num) == -1)
		return;

	if (!mProtocol->isConnected())
	{
		KMessageBox::sorry(qApp->mainWidget(),
			i18n("<qt>Sorry, you must be connected to the AIM server to retrieve user information, but you will be allowed to continue if you	would like to change the user's nickname.</qt>"),
			i18n("You Must be Connected") );
	}
	else
	{
		if (tmpBuddy.status == TAIM_OFFLINE)
		{
			KMessageBox::sorry(qApp->mainWidget(),
				i18n("<qt>Sorry, this user isn't online for you to view his/her information, but you will be allowed to only change his/her nickname. Please wait until this user becomes available and try again</qt>" ),
				i18n("User not Online"));
		}
	}
	OscarUserInfo *Oscaruserinfo =
		new OscarUserInfo(mName, tmpBuddy.alias, mProtocol, tmpBuddy);

	connect(Oscaruserinfo, SIGNAL(updateNickname(const QString)),
		this, SLOT(slotUpdateNickname(const QString)));

	Oscaruserinfo->show();
}

// Called when an IM is received
void OscarContact::slotIMReceived(QString message, QString sender, bool /*isAuto*/)
{
	// Check if we're the one who sent the message
	if ( tocNormalize(sender) != tocNormalize(mName) )
		return;

	TBuddy tmpBuddy;
	mProtocol->buddyList()->get(&tmpBuddy, mProtocol->buddyList()->getNum(mName));

	// Tell the message manager that the buddy is done typing
	msgManager()->userTypingMsg(this, false);
		
	// Build a KopeteMessage and set the body as Rich Text
	KopeteContactPtrList tmpList;
	tmpList.append(mProtocol->myself());
	KopeteMessage msg = parseAIMHTML( message );
	msgManager()->appendMessage(msg);

	// send our away message in fire-and-forget-mode :)
	if ( mProtocol->isAway() )
	{
		// Get the current time
		long currentTime = time(0L);

		// Compare to the last time we sent a message
		// We'll wait 2 minutes between responses
		if( (currentTime - mLastAutoResponseTime) > 120 )
		{
			kdDebug() << "[OscarContact] slotIMReceived() while we are away, sending away-message to annoy buddy :)" << endl;
			// Send the autoresponse
			mProtocol->engine->sendIM(
					KopeteAway::getInstance()->message(),
					mName, true);
			// Build a pointerlist to insert this contact into
			KopeteContactPtrList toContact;
			toContact.append(this);
			// Display the autoresponse
			// Make it look different
			QString responseDisplay = KopeteAway::getInstance()->message();
			responseDisplay.prepend("<font color='#666699'>Autoresponse: </font>");

			KopeteMessage message( mProtocol->myself(), toContact,
					responseDisplay,
					KopeteMessage::Outbound,
					KopeteMessage::RichText);

			msgManager()->appendMessage(message);

			// Set the time we last sent an autoresponse
			// which is right now
			mLastAutoResponseTime = time(0L);
		}
	}
}

/** Called when we want to send a message */
void OscarContact::slotSendMsg(const KopeteMessage& message, KopeteMessageManager *)
{
	if ( message.body().isEmpty() ) // no text, do nothing
		return;

	TBuddy *tmpBuddy = mProtocol->buddyList()->getByNum(mProtocol->buddyList()->getNum(mName));

	// Check to see if we're even online
	if (!mProtocol->isConnected())
	{
		KMessageBox::sorry(qApp->mainWidget(),
			i18n("<qt>You must be logged on to AIM before you can send a message to a user.</qt>"),
			i18n("Not Signed On"));
		return;
	}

	// Check to see if the person we're sending the message to is online
	if (tmpBuddy->status == TAIM_OFFLINE || mStatus == TAIM_OFFLINE)
	{
			KMessageBox::sorry(qApp->mainWidget(),
							i18n("<qt>This user is not online at the moment for you to message him/her. AIM users must be online for you to be able to message them.</qt>"),
							i18n("User not Online"));
			return;
	}

	mProtocol->engine->sendIM(message.escapedBody(), mName, false);

	// Show the message we just sent in the chat window
	msgManager()->appendMessage(message);
}

/** Called when nickname needs to be updated */
void OscarContact::slotUpdateNickname(const QString newNickname)
{
	setDisplayName( newNickname );
	//emit updateNickname ( newNickname );

	TBuddy *tmp;
	tmp = mProtocol->buddyList()->getByNum(mProtocol->buddyList()->getNum(mName));
	tmp->alias = newNickname;
}

/** Return whether or not this contact is REACHABLE. */
bool OscarContact::isReachable(void)
{
	return (mStatus != OSCAR_OFFLINE);
}

/** Returns a set of custom menu items for the context menu */
KActionCollection *OscarContact::customContextMenuActions(void)
{
	if( actionCollection != 0L )
		delete actionCollection;

	actionCollection = new KActionCollection(this);
	actionCollection->insert( actionWarn );
	actionCollection->insert( actionBlock );
	//experimental
	actionCollection->insert( actionDirectConnect );
	return actionCollection;
}

/** Method to delete a contact from the contact list */
void OscarContact::slotDeleteContact(void)
{
	TBuddy tmpBuddy;
	mProtocol->buddyList()->get( &tmpBuddy, mProtocol->buddyList()->getNum(mName) );
	QString buddyName = (tmpBuddy.alias.isEmpty() ? mName : tmpBuddy.alias);

	if (
		KMessageBox::warningYesNo(
			qApp->mainWidget(),
			i18n("<qt>Are you sure you want to remove %1 from your contact list?</qt>").arg(buddyName),
			i18n("Confirmation")
			) == KMessageBox::Yes )
	{
		mProtocol->buddyList()->del(tocNormalize(mName));
		mProtocol->engine->sendDelBuddy(tmpBuddy.name,mProtocol->buddyList()->getNameGroup(tmpBuddy.group));
		deleteLater();
	}
}

void OscarContact::slotWarn()
{
	QString message = i18n( "<qt>Would you like to warn %1 anonymously?" \
	" Select \"Yes\" to warn anonymously, \"No\" to warn" \
	" the user showing them your name, or \"Cancel\" to abort" \
	" warning. (Warning a user on AIM will result in a \"Warning Level\"" \
	" increasing for the user you warn. Once this level has reached a" \
	" certain point, they will not be able to sign on. Please do not abuse" \
	" this function, it is meant for legitimate practices.)</qt>" ).arg(mName);
	QString title = i18n("Warn User %1 anonymously?").arg(mName);

	int result = KMessageBox::questionYesNoCancel(qApp->mainWidget(), message, title);
	if (result == KMessageBox::Yes)
	{
		mProtocol->engine->sendWarning(mName, true);
	}
	else if (result == KMessageBox::No)
	{
		mProtocol->engine->sendWarning(mName, false);
	} 
}



KopeteMessage OscarContact::parseAIMHTML ( QString m )
{
/*	============================================================================================
	Original AIM-Messages, just a few to get the idea of the weird format[tm]:

	From original AIM:
	<HTML><BODY BGCOLOR="#ffffff"><FONT FACE="Verdana" SIZE=4>some text message</FONT></BODY></HTML>

	From Trillian 0.7something:
	<HTML><BODY BGCOLOR="#ffffff"><font face="Arial"><b>bin ich ueberhaupt ein standard?</b></BODY></HTML>
	<HTML><BODY BGCOLOR="#ffffff"><font face="Arial"><font color="#ffff00">ups</BODY></HTML>
	<HTML><BODY BGCOLOR="#ffffff"><font face="Arial"><font back="#00ff00">bggruen</BODY></HTML>
	<HTML><BODY BGCOLOR="#ffffff"><font face="Arial"><font back="#00ff00"><font color="#ffff00">both</BODY></HTML>
	<HTML><BODY BGCOLOR="#ffffff"><font face="Arial">LOL</BODY></HTML>

	From GAIM:
	<FONT COLOR="#0002A6"><FONT SIZE="2">cool cool</FONT></FONT>
	============================================================================================ */

	kdDebug() << "AIM Plugin: original message: " << m << endl;

	QRegExp expr;
	expr.setCaseSensitive( false );
	expr.setWildcard( true );
	expr.setMinimal( true );

	QString result = m;
	expr.setPattern( "^<html.*>" );
	result.remove( expr );
	expr.setPattern( "^<body.*>" );
	result.remove( expr );
	expr.setPattern( "</html>$" );
	result.remove( expr );
	expr.setPattern( "</body>$" );
	result.remove( expr );

	KopeteContactPtrList tmpList;
	tmpList.append(mProtocol->myself());
	KopeteMessage msg( this, tmpList, result, KopeteMessage::Inbound, KopeteMessage::RichText);

	// We don't actually do anything in there yet, but we might eventually

	return msg;
}

// removes a weird html-tag (and returns the attributes it contained)
/*
QStringList OscarContact::removeTag ( QString &message, QString tag )
{
	QStringList attr;
	// first occurance of <TAG *> where * is anything except a '>'
	// regexp is NOT case-sensitive
	int tagStart = message.find ( QRegExp(QString("<"+tag+"\\s+[^>]*>"),false) );
	int tagStartEnd = message.find ( ">", tagStart+4, false );

	while((tagStart != -1 && tagStart != -1))
	{
		if ( tagStart != -1 && tagStartEnd != -1)
		{
			// we found a proper opening-tag
			QString tagAttr = message.mid(tagStart, (tagStartEnd - tagStart));

			// Strip the <>'s
			tagAttr.remove(0, 1);
			tagAttr.remove(tagAttr.length(), 1);

			// Now grab the attributes
			tagAttr = tagAttr.section(' ', 1);
			attr += QStringList::split(' ', tagAttr);

			message.remove ( tagStart, tagStartEnd - tagStart + 1 ); // remove the opening-tag
			// find last closing of TAG (NOT case-sensitive)
			int tagEnd = message.findRev( QString("</"+tag+">"), -1, false );
			if ( tagEnd != -1 ) // found closing of font-tag
			{
				message.remove ( tagEnd, tag.length()+3  );
			}
		}
		tagStart = message.find ( QRegExp(QString("<"+tag+"\\s+[^>]*>"),false) );
		tagStartEnd = message.find ( ">", tagStart+4, false );
	}
	return attr;
}
*/
void OscarContact::slotMoved(KopeteMetaContact * old )
{
	disconnect(old, SIGNAL(aboutToSave(KopeteMetaContact*)), 0, 0);

	connect (metaContact() , SIGNAL( aboutToSave(KopeteMetaContact*) ),
		protocol(), SLOT (serialize(KopeteMetaContact*) ));
}

/** Called when we want to block the contact */
void OscarContact::slotBlock(void)
{
	QString message = i18n( "<qt>Are you sure you want to block %1? \
	Once blocked, this user will no longer be visible to you. The block can be \
	removed later in the preferences dialog.</qt>" ).arg(mName);
	QString title = i18n("Block User %1?").arg(mName);

	int result = KMessageBox::questionYesNo(qApp->mainWidget(), message, title);
	if (result == KMessageBox::Yes)
	{
		mProtocol->engine->sendBlock(mName);
	}
}

/** Called when we want to connect directly to this contact */
void OscarContact::slotDirectConnect(void)
{
	kdDebug() << "[OscarContact] Requesting direct IM with " << mName << endl;
	QString message = i18n( "<qt>Are you sure you want to establish a direct connection to %1? \
	This will allow %2 to know your IP address, which can be dangerous if you do not trust this contact</qt>" ).arg(mName).arg(mName);
	QString title = i18n("Request Direct IM with %1?").arg(mName);
	int result = KMessageBox::questionYesNo(qApp->mainWidget(), message, title);
	if ( result == KMessageBox::Yes )
	{
		execute();
		KopeteMessage m;
		m.setBody(i18n("Waiting for %1 to connect...").arg(mName), KopeteMessage::PlainText );
		msgManager()->appendMessage(m);
		mProtocol->engine->sendDirectIMRequest(mName);
	}
}

/** Called when we become directly connected to the contact */
void OscarContact::slotDirectIMReady(QString name)
{
	// Check if we're the one who is directly connected
	if ( tocNormalize(name) != tocNormalize(mName) )
		return;

	kdDebug() << "[OscarContact] Setting direct connect state for " << mName << " to true." << endl;
	mDirectlyConnected = true;
	execute();
	KopeteMessage m;
	m.setBody(i18n("Direct connection to %1 established").arg(mName), KopeteMessage::PlainText );
	msgManager()->appendMessage(m);
}

/** Called when the direct connection to contact @name has been terminated */
void OscarContact::slotDirectIMConnectionClosed(QString name)
{
	// Check if we're the one who is directly connected
	if ( tocNormalize(name) != tocNormalize(mName) )
		return;

	kdDebug() << "[OscarContact] Setting direct connect state for " << mName << " to false." << endl;
	mDirectlyConnected = false;
}

/*	if ( (this->status() != m_cachedOldStatus) || ( size != m_cachedSize ) )
	{
		QImage afScal = ((QPixmap(SmallIcon(this->statusIcon()))).convertToImage()).smoothScale( size, size );
		m_cachedScaledIcon = QPixmap(afScal);
		m_cachedOldStatus = this->status();
		m_cachedSize = size;
	}
	if ( m_idleState == Idle || mDirectlyConnected )
	{
		QImage tmp;
		QPixmap pm;
		tmp = m_cachedScaledIcon.convertToImage();
		if ( m_idleState == Idle ) // if this contact is idle, make its icon semi-transparent
			KIconEffect::semiTransparent(tmp);
		if ( mDirectlyConnected ) //if we are directly connected, change color of icon
			KIconEffect::colorize(tmp, red, 1.0F);
		pm.convertFromImage(tmp);
		return pm;
	}
	return m_cachedScaledIcon; */

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:

#include "oscarcontact.moc"

