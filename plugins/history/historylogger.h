/*
    historylogger.cpp

    Copyright (c) 2003 by Olivier Goffart        <ogoffart@tiscalinet.be>
    Kopete    (c) 2003 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef HISTORYLOGGER_H
#define HISTORYLOGGER_H

#include <qobject.h>
#include "kopetemessage.h" //TODO: REMOVE

class KopeteContact;
class KopeteMetaContact;
class QFile;
class QDomDocument;

/**
 * One hinstance of this class is opened for every KopeteMessageManager,
 * or for the history dialog
 *
 * @author Olivier Goffart
 */
class HistoryLogger : public QObject
{
Q_OBJECT
public:

	/**
	 * Chronological: messages are read from the first to the last, in the time order
	 * AntiChronological: messages are read from the last to the first, in the time reversed order
	 */
	enum Sens { Default , Chronological , AntiChronological };

	/*
	 * constructor: it take the contacvt, and the color of messages
	 */
	HistoryLogger( KopeteMetaContact *m ,const QColor &col=QColor(), QObject *parent = 0, const char *name = 0);
	HistoryLogger( KopeteContact *c ,const QColor &col=QColor(), QObject *parent = 0, const char *name = 0);


	~HistoryLogger();

	bool hideOutgoing() { return m_hideOutgoing; }
	void setHideOutgoing(bool);


	//----------------------------------

	/**
	 * log a message
	 * @param c add a presision to the contact to use, if null, autodetect.
	 */
	void appendMessage( const KopeteMessage &msg , const KopeteContact *c=0L );

	/**
	 * read @param nb message from the current position, from the given contact, in the given sens
	 */
	QValueList<KopeteMessage> readMessages( unsigned int nb , const KopeteContact *c=0L , Sens sens=Default, bool reverseOrder=false );

	/**
	 * The pausition is set to the last message
	 */
	void setPositionToLast();
	/**
	 * The position is set to the first message
	 */
	void setPositionToFirst();

	/**
	 * Set the current month  (in number of month since the actual month)
	 */
	void setCurrentMonth(int month);


private:
//	QFile m_file;
	// xml stuff
//	QDomDocument *xmllist;

	// total amount of messages in the log
//	int messages;

//	int m_currentPos;
	bool m_hideOutgoing;
//	bool m_reversed;

//	QString m_logFileName;

	/*
	 *contais all QDomDocument, for a KC, for a specified Month
	 */

	QMap<const KopeteContact*,QMap<unsigned int , QDomDocument> > m_documents;

	/**
	 * Contains the current message.
	 * in fact, this is the next, still not showed
	 */
	QMap<const KopeteContact*, QDomElement>  m_currentElements;

	/**
	 * Get the document, open it is @param canload is true, contain is set to false if the document
	 * is not already contained
	 */
	QDomDocument getDocument(const KopeteContact *c, unsigned int month , bool canLoad=true , bool* contain=0L);

	/**
	 * look over files to get the last month for this contact
	 */
	unsigned int getFistMonth(const KopeteContact *c) ;
	unsigned int getFistMonth() ;


	/*
	 * the current month
	 */
	unsigned int m_currentMonth;

	/*
	 * the cached getFirstMonth
	 */
	int m_cachedMonth;

	/*
	 * get the filename of the xml file which contains the history from the contact in the specified month
	 */
	static QString getFileName(const KopeteContact* , unsigned int month);

	/*
	 * the metacontact we are using
	 */
	KopeteMetaContact *m_metaContact;
	/*
	 * color of messages
	 */
	QColor m_color;



	/*
	 * keep the old position in memory, so if we change the sens, we can begin here
	 */
	 QMap<const KopeteContact*, QDomElement>  m_oldElements;
	 unsigned int m_oldMonth;
	 Sens m_oldSens;



	/*
	 * WORKAROUND
	 * due to a bug in QT, i have to keep the document element in the memory to prevent crashes
	 */
	QValueList<QDomElement> workaround;
};

#endif
