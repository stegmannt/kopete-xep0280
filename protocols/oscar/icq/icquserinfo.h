 /*
    icquserinfo.h  -  ICQ Protocol Plugin

    Copyright (c) 2002 by Nick Betcher <nbetcher@kde.org>

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

#ifndef ICQUSERINFO_H
#define ICQUSERINFO_H

#include <kdebug.h>
#include <qhbox.h>
#include <kdialogbase.h>

#include <qmap.h>

class QComboBox;
class ICQAccount;
class ICQContact;
class ICQUserInfoWidget;

class ICQUserInfo : public KDialogBase
{
	Q_OBJECT

	public:
		ICQUserInfo(ICQContact *, ICQAccount *, bool editable=false,
			QWidget *parent = 0, const char* name = "ICQUserInfo");

	private:
		void sendInfo();
		void setEditable(bool);
		void setCombo(QComboBox *, int, int);

	private slots:
		void slotSaveClicked();
		void slotCloseClicked();
		void slotHomePageClicked(const QString &);
		void slotEmailClicked(const QString &);
		void slotFetchInfo(); // initiate fetching info from server
		void slotReadInfo(); // read in results from fetch

	signals:
//		void updateNickname(const QString);
		void closing();

	private:
		// TODO: move these somewhere else so it only gets
		// set ONCE and not on every userinfo dialog
		void fillCombo(QComboBox *, const QMap<int, QString> &);
		void setCombo(QComboBox *, const QMap<int, QString> &, int);

	private:
		ICQAccount *mAccount;
		ICQContact *mContact;
		bool mEditable;
		ICQUserInfoWidget *mMainWidget;
};
#endif
// vim: set noet ts=4 sts=4 sw=4:
