#include "kopetewindow.h"
#include "kopetewindow.moc"

#include <kopete.h>
#include <contactlist.h>

#include <qlayout.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kaction.h>
#include <kdebug.h>

KopeteWindow::KopeteWindow(QWidget *parent, const char *name ): KMainWindow(parent,name)
{
	kdDebug() << "KopeteWindow::KopeteWindow()" << endl;

	mainwidget = new QWidget(this);
	setCentralWidget(mainwidget);

	QBoxLayout *layout = new QBoxLayout(mainwidget,QBoxLayout::TopToBottom);
	contactlist = new ContactList(mainwidget);
	layout->insertWidget ( -1,contactlist );
	
	/* ---------------------------------- */
	
	actionAddContact = new KAction( i18n("&Add contact"),"bookmark_add",0 ,
							kopeteapp, SLOT(slotAddContact()),
							actionCollection(), "AddContact" );

	actionConnect = new KAction( i18n("&Connect All"),"connect_no",0 ,
							kopeteapp, SLOT(slotConnectAll()),
							actionCollection(), "Connect" );

	actionDisconnect = new KAction( i18n("&Disconnect All"),"connect_established",0 ,
							kopeteapp, SLOT(slotDisconnectAll()),
							actionCollection(), "Disconnect" );

	actionAboutPlugins = new KAction( i18n("&Plugins"),"input_devices_settings", 0,
							kopeteapp, SLOT(slotAboutPlugins()),
							actionCollection(), "AboutPlugins" );

	actionPrefs = KStdAction::preferences(kopeteapp, SLOT(slotPreferences()), actionCollection());

	actionQuit = KStdAction::quit(kopeteapp, SLOT(slotExit()), actionCollection());

	toolbarAction = KStdAction::showToolbar(this, SLOT(showToolbar()), actionCollection());

	/* ---------------------------------- */

	createGUI("kopeteui.rc");
	
	loadOptions();

	toolbarAction->setChecked( !toolBar("mainToolBar")->isHidden() );

	statusBar()->show();
//	mainwidget->show();

	show();
}

KopeteWindow::~KopeteWindow()
{
	kdDebug() << "KopeteWindow::~KopeteWindow()" << endl;
}

bool KopeteWindow::queryExit()
{
	kdDebug() << "KopeteWindow::queryExit()" << endl;
	saveOptions();
	return true;
}


void KopeteWindow::loadOptions(void)
{
	KConfig *config = KGlobal::config();

	applyMainWindowSettings ( config, "General Options" );

	QSize size = config->readSizeEntry("Geometry");
	if(!size.isEmpty())
	{
		resize(size);
	}

	QPoint pos = config->readPointEntry("Position");
	move(pos);

	QString tmp = config->readEntry("State", "Shown");
	if ( tmp == "Minimized" )
		showMinimized();
	else if ( tmp == "Hidden" )
		hide();
	else
		show();
}

void KopeteWindow::saveOptions(void)
{
	KConfig *config = KGlobal::config();

	saveMainWindowSettings( config, "General Options" );

	config->setGroup("General Options");
	config->writeEntry("Position", pos());
	config->writeEntry("Geometry", size());

	if(isMinimized())
	{
		config->writeEntry("State", "Minimized");
	}
	else if(isHidden())
	{
		config->writeEntry("State", "Hidden");
	}
	else
	{
		config->writeEntry("State", "Shown");
	}

	config->sync();
}

void KopeteWindow::showToolbar(void)
{
	if( toolbarAction->isChecked() )
		toolBar("mainToolBar")->show();
	else
		toolBar("mainToolBar")->hide();
}
