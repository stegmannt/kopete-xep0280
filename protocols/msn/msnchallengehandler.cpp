/*
    msnchallengehandler.h - Computes a msn challenge response hash key.

    Copyright (c) 2005 by Gregg Edghill       <gregg.edghill@gmail.com>
    Kopete    (c) 2003-2005 by The Kopete developers <kopete-devel@kde.org>

    Portions taken from
    	http://msnpiki.msnfanatic.com/index.php/MSNP11:Challenges

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "msnchallengehandler.h"

#include <qdatastream.h>
#include <QByteArray>

#include <kdebug.h>
#include <kcodecs.h>

MSNChallengeHandler::MSNChallengeHandler(const QString& productKey, const QString& productId)
{
	m_productKey = productKey;
	m_productId  = productId;
}


MSNChallengeHandler::~MSNChallengeHandler()
{
	kDebug(14140) ;
}

QString MSNChallengeHandler::computeHash(const QString& challengeString)
{
  	// Step One: THe MD5 Hash.

  	// Combine the received challenge string with the product key.
 	KMD5 md5((challengeString + m_productKey).toUtf8());
 	QByteArray digest = md5.hexDigest();

 	kDebug(14140) << "md5: " << digest;

 	QVector<qint32> md5Integers(4);
 	for(int i=0; i < md5Integers.count(); i++)
 	{
 		md5Integers[i] = hexSwap(digest.mid(i*8, 8)).toUInt(0, 16) & 0x7FFFFFFF;
 		kDebug(14140) << ("0x" + hexSwap(digest.mid(i*8, 8))) << " " << md5Integers[i];
 	}

	// Step Two: Create the challenge string key

	QString challengeKey = challengeString + m_productId;
	// Pad to multiple of 8.
	challengeKey = challengeKey.leftJustified(challengeKey.length() + (8 - challengeKey.length() % 8), '0');

	kDebug(14140) << "challenge key: " << challengeKey;

	QVector<qint32> challengeIntegers(challengeKey.length() / 4);
	for(int i=0; i < challengeIntegers.count(); i++)
	{
		QString sNum = challengeKey.mid(i*4, 4), sNumHex;

		// Go through the number string, determining the hex equivalent of each value
		// and add that to our new hex string for this number.
		for(int j=0; j < sNum.length(); j++) {
			sNumHex += QString::number((int)sNum[j].toLatin1(), 16);
		}

		// swap because of the byte ordering issue.
		sNumHex = hexSwap(sNumHex);
		// Assign the converted number.
		challengeIntegers[i] = sNumHex.toInt(0, 16);
		kDebug(14140) << sNum << (": 0x"+sNumHex) << " " << challengeIntegers[i];
	}

	// Step Three: Create the 64-bit hash key.

	// Get the hash key using the specified arrays.
	qint64 key = createHashKey(md5Integers, challengeIntegers);
	kDebug(14140) << "key: " << key;

	// Step Four: Create the final hash key.

	QString upper = QString::number(QString(digest.mid(0, 16)).toULongLong(0, 16)^key, 16);
	if(upper.length() % 16 != 0)
		upper = upper.rightJustified(upper.length() + (16 - upper.length() % 16), '0');

	QString lower = QString::number(QString(digest.mid(16, 16)).toULongLong(0, 16)^key, 16);
	if(lower.length() % 16 != 0)
		lower = lower.rightJustified(lower.length() + (16 - lower.length() % 16), '0');

	return (upper + lower);
}

qint64 MSNChallengeHandler::createHashKey(const QVector<qint32>& md5Integers,
	const QVector<qint32>& challengeIntegers)
{
	kDebug(14140) << "Creating 64-bit key.";

	qint64 magicNumber = 0x0E79A9C1L, high = 0L, low = 0L;
		
	for(int i=0; i < challengeIntegers.count(); i += 2)
	{
		qint64 temp = ((challengeIntegers[i] * magicNumber) % 0x7FFFFFFF) + high;
		temp = ((temp * md5Integers[0]) + md5Integers[1]) % 0x7FFFFFFF;

		high = (challengeIntegers[i + 1] + temp) % 0x7FFFFFFF;
		high = ((high * md5Integers[2]) + md5Integers[3]) % 0x7FFFFFFF;

		low += high + temp;
	}

	high = (high + md5Integers[1]) % 0x7FFFFFFF;
	low  = (low  + md5Integers[3]) % 0x7FFFFFFF;

	QByteArray tempArray;
	tempArray.reserve(8);
	QDataStream buffer(&tempArray,QIODevice::ReadWrite);
	//buffer.setVersion(QDataStream::Qt_3_1);
	buffer.setByteOrder(QDataStream::LittleEndian);
	buffer << (qint32)high;
	buffer << (qint32)low;

	buffer.device()->reset();
	buffer.setByteOrder(QDataStream::BigEndian);
	qint64 key;
	buffer >> key;
	
	return key;
}

QString MSNChallengeHandler::hexSwap(const QString& in)
{
	QString sHex = in, swapped;
	while(sHex.length() > 0)
	{
		swapped = swapped + sHex.mid(sHex.length() - 2, 2);
		sHex = sHex.remove(sHex.length() - 2, 2);
	}
	return swapped;
}

QString MSNChallengeHandler::productId()
{
	return m_productId;
}

#include "msnchallengehandler.moc"
