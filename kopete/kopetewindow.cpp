/*
    kopetewindow.cpp  -  Kopete Main Window

    Copyright (c) 2001-2002 by Duncan Mac-Vicar Prett <duncan@kde.org>
    Copyright (c) 2001-2002 by Stefan Gehn            <metz AT gehn.net>
    Copyright (c) 2002-2003 by Martijn Klingens       <klingens@kde.org>
    Copyright (c) 2002-2005 by Olivier Goffart        <ogoffart at kde.org>
    Copyright (c) 2005-2006 by Will Stephenson        <wstephenson@kde.org>

    Kopete    (c) 2002-2005 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "kopetewindow.h"

#include <QCursor>
#include <QLayout>
#include <QToolTip>
#include <QTimer>
#include <QPixmap>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QLabel>
#include <QShowEvent>
#include <QLineEdit>
#include <QSignalMapper>

#include <khbox.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kstdaction.h>
#include <ktoggleaction.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kglobalaccel.h>
#include <klocale.h>
#include <kicon.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <knotifyconfigwidget.h>
#include <kmenu.h>
#include <kkeydialog.h>
#include <kedittoolbar.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <kglobalaccel.h>
#include <kwin.h>
#include <kdeversion.h>
#include <kinputdialog.h>
#include <kplugininfo.h>
#include <ksqueezedtextlabel.h>
#include <kstringhandler.h>
#include <kurl.h>
#include <kxmlguifactory.h>
#include <ktoolbar.h>
#include <kdialog.h>
#include <kstdaction.h>

#include "addcontactpage.h"
#include "addressbooklinkwidget.h"
#include "ui_groupkabcselectorwidget.h"
#include "kabcexport.h"
#include "kopeteappearancesettings.h"
#include "kopeteapplication.h"
#include "kopeteaccount.h"
#include "kopeteaway.h"
#include "kopeteaccountmanager.h"
#include "kopeteaccountstatusbaricon.h"
#include "kopetebehaviorsettings.h"
#include "kopetecontact.h"
#include "kopetecontactlist.h"
#include "kopetecontactlistview.h"
#include "kopetegroup.h"
#include "kopetelistviewsearchline.h"
#include "kopetechatsessionmanager.h"
#include "kopetepluginconfig.h"
#include "kopetepluginmanager.h"
#include "kopeteprotocol.h"
#include "kopetestdaction.h"
#include "kopeteawayaction.h"
#include "kopeteuiglobal.h"
#include "systemtray.h"
#include "kopeteonlinestatusmanager.h"
#include "kopeteeditglobalidentitywidget.h"

//BEGIN GlobalStatusMessageIconLabel
GlobalStatusMessageIconLabel::GlobalStatusMessageIconLabel(QWidget *parent)
 : QLabel(parent)
{
}

void GlobalStatusMessageIconLabel::mouseReleaseEvent( QMouseEvent *event )
{
      if( event->button() == Qt::LeftButton || event->button() == Qt::RightButton )
      {
              emit iconClicked( event->globalPos() );
              event->accept();
      }
}
//END GlobalStatusMessageIconLabel

class KopeteWindow::Private
{
public:
	Private()
	 : contactlist(0), actionAddContact(0), actionDisconnect(0), actionExportContacts(0),
	actionAwayMenu(0), actionDockMenu(0), selectAway(0), selectBusy(0), actionSetAvailable(0),
	actionSetInvisible(0), actionPrefs(0), actionQuit(0), actionSave(0), menubarAction(0),
	statusbarAction(0), actionShowOffliners(0), actionShowEmptyGroups(0), editGlobalIdentityWidget(0),
	docked(0), hidden(false), deskRight(0), statusBarWidget(0), tray(0), autoHide(false),
	autoHideTimeout(0), autoHideTimer(0), addContactMapper(0), pluginConfig(0),
	globalStatusMessage(0), globalStatusMessageMenu(0), newMessageEdit(0)
	{}

	~Private()
	{
		delete pluginConfig;
	}

	KopeteContactListView *contactlist;

	// Some Actions
	KActionMenu *actionAddContact;

	KAction *actionDisconnect;
	KAction *actionExportContacts;

	KActionMenu *actionAwayMenu;
	KActionMenu *actionDockMenu;
	KAction *selectAway;
	KAction *selectBusy;
	KAction *actionSetAvailable;
	KAction *actionSetInvisible;


	KAction *actionPrefs;
	KAction *actionQuit;
	KAction *actionSave;
	KToggleAction *menubarAction;
	KToggleAction *statusbarAction;
	KToggleAction *actionShowOffliners;
	KToggleAction *actionShowEmptyGroups;

	KopeteEditGlobalIdentityWidget *editGlobalIdentityWidget;

	int docked;
	bool hidden;
	int deskRight;
	QPoint position;
	KHBox *statusBarWidget;
	KopeteSystemTray *tray;
	bool autoHide;
	unsigned int autoHideTimeout;
	QTimer *autoHideTimer;
	QSignalMapper *addContactMapper;

	KopetePluginConfig *pluginConfig;

	QHash<const Kopete::Account*, KopeteAccountStatusBarIcon*> accountStatusBarIcons;
	KSqueezedTextLabel *globalStatusMessage;
	KMenu *globalStatusMessageMenu;
	QLineEdit *newMessageEdit;
	QString globalStatusMessageStored;
};

/* KMainWindow is very broken from our point of view - it deref()'s the app
 * when the last visible KMainWindow is destroyed. But when our main window is
 * hidden when it's in the tray,closing the last chatwindow would cause the app
 * to quit. - Richard
 *
 * Fortunately KMainWindow checks queryExit before deref()ing the Kapplication.
 * KopeteWindow reimplements queryExit() and only returns true if it is shutting down
 * (either because the user quit Kopete, or the session manager did).
 *
 * KopeteWindow and ChatWindows are closed by session management.
 * App shutdown is not performed by the KopeteWindow but by KopeteApplication:
 * 1) user quit - KopeteWindow::slotQuit() was called, calls KopeteApplication::quitKopete(),
 *                which closes all chatwindows and the KopeteWindow.  The last window to close
 *                shuts down the PluginManager in queryExit().  When the PluginManager has completed its
 *                shutdown, the app is finally deref()ed, and the contactlist and accountmanager
 *                are saved.
 *                and calling KApplication::quit()
 * 2) session   - KopeteWindow and all chatwindows are closed by KApplication session management.
 *     quit        Then the shutdown proceeds as above.
 *
 * queryClose() is honoured so group chats and chats receiving recent messages can interrupt
 * (session) quit.
 */
 
KopeteWindow::KopeteWindow( QWidget *parent, const char *name )
: KMainWindow( parent, Qt::WType_TopLevel ), d(new Private)
{
	setObjectName( name );
	// Applications should ensure that their StatusBar exists before calling createGUI()
	// so that the StatusBar is always correctly positioned when KDE is configured to use
	// a MacOS-style MenuBar.
	// This fixes a "statusbar drawn over the top of the toolbar" bug
	// e.g. it can happen when you switch desktops on Kopete startup
	d->statusBarWidget = new KHBox(statusBar());
	d->statusBarWidget->setObjectName( "m_statusBarWidget" );
	d->statusBarWidget->setMargin( 2 );
	d->statusBarWidget->setSpacing( 1 );
	statusBar()->addPermanentWidget(d->statusBarWidget, 0);
	KHBox *statusBarMessage = new KHBox(statusBar());
	d->statusBarWidget->setMargin( 2 );
	d->statusBarWidget->setSpacing( 1 );

	GlobalStatusMessageIconLabel *label = new GlobalStatusMessageIconLabel( statusBarMessage );
	label->setObjectName( QLatin1String("statusmsglabel") );
	label->setFixedSize( 16, 16 );
	label->setPixmap( SmallIcon( "kopetestatusmessage" ) );
	connect(label, SIGNAL(iconClicked( const QPoint& )),
		this, SLOT(slotGlobalStatusMessageIconClicked( const QPoint& )));
	label->setToolTip( i18n( "Global status message" ) );
	d->globalStatusMessage = new KSqueezedTextLabel( statusBarMessage );
	statusBar()->addWidget(statusBarMessage, 1);

	d->autoHideTimer = new QTimer( this );

	// --------------------------------------------------------------------------------
	initView();
	initActions();
	d->contactlist->initActions(actionCollection());
	initSystray();
	// --------------------------------------------------------------------------------

	// Trap all loaded plugins, so we can add their status bar icons accordingly , also used to add XMLGUIClient
	connect( Kopete::PluginManager::self(), SIGNAL( pluginLoaded( Kopete::Plugin * ) ),
		this, SLOT( slotPluginLoaded( Kopete::Plugin * ) ) );
	connect( Kopete::PluginManager::self(), SIGNAL( allPluginsLoaded() ),
		this, SLOT( slotAllPluginsLoaded() ));
	//Connect the appropriate account signals
	/* Please note that I tried to put this in the slotAllPluginsLoaded() function
	 * but it seemed to break the account icons in the statusbar --Matt */

	connect( Kopete::AccountManager::self(), SIGNAL(accountRegistered(Kopete::Account*)),
		this, SLOT(slotAccountRegistered(Kopete::Account*)));
	connect( Kopete::AccountManager::self(), SIGNAL(accountUnregistered(const Kopete::Account*)),
		this, SLOT(slotAccountUnregistered(const Kopete::Account*)));

	connect( d->autoHideTimer, SIGNAL( timeout() ), this, SLOT( slotAutoHide() ) );
	connect( Kopete::AppearanceSettings::self(), SIGNAL( contactListAppearanceChanged() ),
		this, SLOT( slotContactListAppearanceChanged() ) );
	createGUI ( QLatin1String("kopeteui.rc") );

	// call this _after_ createGUI(), otherwise menubar is not set up correctly
	loadOptions();

	// If some plugins are already loaded, merge the GUI
	Kopete::PluginList plugins = Kopete::PluginManager::self()->loadedPlugins();
	foreach(Kopete::Plugin *plug, plugins)
		slotPluginLoaded( plug );

	// If some account alrady loaded, build the status icon
	QList<Kopete::Account *> accountList = Kopete::AccountManager::self()->accounts();
	foreach(Kopete::Account *a, accountList)
		slotAccountRegistered( a );

    //install an event filter for the quick search toolbar so we can
    //catch the hide events
    toolBar( "quickSearchBar" )->installEventFilter( this );
}

void KopeteWindow::initView()
{
	d->contactlist = new KopeteContactListView(this);
	setCentralWidget(d->contactlist);
}

void KopeteWindow::initActions()
{
	// this action menu contains one action per account and is updated when accounts are registered/unregistered
	d->actionAddContact = new KActionMenu( KIcon("add_user"), i18n( "&Add Contact" ),
		actionCollection(), "AddContact" );
	d->actionAddContact->setDelayed( false );
	// this signal mapper is needed to call slotAddContact with the correct arguments
	d->addContactMapper = new QSignalMapper( this );
	connect( d->addContactMapper, SIGNAL( mapped( const QString & ) ),
		 this, SLOT( slotAddContactDialogInternal( const QString & ) ) );

	d->actionDisconnect = new KAction( KIcon("connect_no"), i18n( "O&ffline" ), actionCollection(), "DisconnectAll" );
	connect( d->actionDisconnect, SIGNAL( triggered(bool) ), this, SLOT( slotDisconnectAll() ) );
	d->actionDisconnect->setEnabled(false);

	d->actionExportContacts = new KAction( i18n( "&Export Contacts..." ), actionCollection(), "ExportContacts" );
	connect( d->actionExportContacts, SIGNAL( triggered(bool) ), this, SLOT( showExportDialog() ) );

	d->selectAway = new KAction( KIcon("kopeteaway"), i18n("&Away"), actionCollection(), "SetAwayAll" );
	connect( d->selectAway, SIGNAL( triggered(bool) ), this, SLOT( slotGlobalAway() ) );

	d->selectBusy = new KAction( KIcon("kopeteaway"), i18n("&Busy"), actionCollection(), "SetBusyAll" );
	connect( d->selectBusy, SIGNAL( triggered(bool) ), this, SLOT( slotGlobalBusy() ) );


	d->actionSetInvisible = new KAction( KIcon("kopeteavailable"), i18n( "&Invisible" ), actionCollection(), "SetInvisibleAll" );
	connect( d->actionSetInvisible, SIGNAL( triggered(bool) ), this, SLOT( slotSetInvisibleAll() ) );

	d->actionSetAvailable = new KAction( KIcon("kopeteavailable"), i18n("&Online"), actionCollection(), "SetAvailableAll" );
	connect( d->actionSetAvailable, SIGNAL( triggered(bool) ), this, SLOT( slotGlobalAvailable() ) );

	d->actionAwayMenu = new KActionMenu( KIcon("kopeteavailable"), i18n("&Set Status"),
							actionCollection(), "Status" );
	d->actionAwayMenu->setDelayed( false );
	d->actionAwayMenu->addAction(d->actionSetAvailable);
	d->actionAwayMenu->addAction(d->selectAway);
	d->actionAwayMenu->addAction(d->selectBusy);
	d->actionAwayMenu->addAction(d->actionSetInvisible);
	d->actionAwayMenu->addAction(d->actionDisconnect);

	d->actionPrefs = KopeteStdAction::preferences( actionCollection(), "settings_prefs" );

	KStdAction::quit(this, SLOT(slotQuit()), actionCollection());

	setStandardToolBarMenuEnabled(true);
	d->menubarAction = KStdAction::showMenubar(this, SLOT(showMenubar()), actionCollection(), "settings_showmenubar" );
	d->statusbarAction = KStdAction::showStatusbar(this, SLOT(showStatusbar()), actionCollection(), "settings_showstatusbar");

	KStdAction::keyBindings( guiFactory(), SLOT( configureShortcuts() ), actionCollection(), "settings_keys" );
	KAction *configurePluginAction = new KAction( KIcon("input_devices_settings"), i18n( "Configure Plugins..." ),
		actionCollection(), "settings_plugins" );
	connect( configurePluginAction, SIGNAL( triggered(bool) ), this, SLOT( slotConfigurePlugins() ) );

	KAction *configureGlobalShortcutsAction = new KAction( KIcon("configure_shortcuts"), i18n( "Configure &Global Shortcuts..." ),
		actionCollection(), "settings_global" );
	connect( configureGlobalShortcutsAction, SIGNAL( triggered(bool) ), this, SLOT( slotConfGlobalKeys() ) );

	KStdAction::configureToolbars( this, SLOT(slotConfToolbar()), actionCollection() );
	KStdAction::configureNotifications(this, SLOT(slotConfNotifications()), actionCollection(), "settings_notifications" );

	d->actionShowOffliners = new KToggleAction( KIcon("show_offliners"), i18n( "Show Offline &Users" ), actionCollection(), "settings_show_offliners" );
	d->actionShowOffliners->setShortcut( KShortcut(Qt::CTRL + Qt::Key_U) );
	connect( d->actionShowOffliners, SIGNAL( triggered(bool) ), this, SLOT( slotToggleShowOffliners() ) );

	d->actionShowEmptyGroups = new KToggleAction( KIcon("folder_green"), i18n( "Show Empty &Groups" ), actionCollection(), "settings_show_empty_groups" );
	d->actionShowEmptyGroups->setShortcut( KShortcut(Qt::CTRL + Qt::Key_G) );
	connect( d->actionShowEmptyGroups, SIGNAL( triggered(bool) ), this, SLOT( slotToggleShowEmptyGroups() ) );

	d->actionShowOffliners->setCheckedState(i18n("Hide Offline &Users"));
	d->actionShowEmptyGroups->setCheckedState(i18n("Hide Empty &Groups"));

	// quick search bar
	QLabel *searchLabel = new QLabel( i18n("Se&arch:"), 0 );
	searchLabel->setObjectName( QLatin1String("kde toolbar widget") );
	QWidget *searchBar = new Kopete::UI::ListView::SearchLine( 0, d->contactlist, "quicksearch_bar" );
	searchLabel->setBuddy( searchBar );
	KAction *quickSearch = new KAction( i18n("Quick Search Bar"), actionCollection(), "quicksearch_bar" );
	quickSearch->setDefaultWidget( searchBar );
	KAction *searchLabelAction = new KAction( i18n("Search:"), actionCollection(), "quicksearch_label" );
	searchLabelAction->setDefaultWidget( searchLabel );

	// quick search bar - clear button
	KAction *resetQuickSearch = new KAction( QApplication::isRightToLeft() ? KIcon("clear_left") : KIcon("locationbar_erase"), i18n( "Reset Quick Search" ),
		actionCollection(), "quicksearch_reset" );
	connect( resetQuickSearch, SIGNAL( triggered(bool) ),  searchBar, SLOT( clear() ) );
	resetQuickSearch->setWhatsThis( i18n( "Reset Quick Search\n"
		"Resets the quick search so that all contacts and groups are shown again." ) );

	// Edit global identity widget/bar
	d->editGlobalIdentityWidget = new KopeteEditGlobalIdentityWidget(this);
	d->editGlobalIdentityWidget->setObjectName( QLatin1String("editglobalBar") );
	KAction *editGlobalAction = new KAction( i18n("Edit Global Identity Widget"), actionCollection(), "editglobal_widget");
	editGlobalAction->setDefaultWidget( d->editGlobalIdentityWidget );

	// KActionMenu for selecting the global status message
	KActionMenu * setStatusMenu = new KActionMenu( KIcon("kopeteeditstatusmessage"), i18n( "Set Status Message" ), actionCollection(), "SetStatusMessage" );
	setStatusMenu->setDelayed( false );
	connect( setStatusMenu->menu(), SIGNAL( aboutToShow() ), SLOT(slotBuildStatusMessageMenu() ) );

	// sync actions, config and prefs-dialog
	connect ( Kopete::AppearanceSettings::self(), SIGNAL(configChanged()), this, SLOT(slotConfigChanged()) );
	slotConfigChanged();

	// Global actions
	KAction *globalReadMessage = new KAction( i18n("Read Message"), actionCollection(), QLatin1String("Read Message") );
	connect( globalReadMessage, SIGNAL( triggered(bool) ), Kopete::ChatSessionManager::self(), SLOT( slotReadMessage() ) );
	globalReadMessage->setGlobalShortcut( KShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_I) );
	globalReadMessage->setWhatsThis( i18n("Read the next pending message") );

	KAction *globalShowContactList = new KAction( i18n("Show/Hide Contact List"), actionCollection(), QLatin1String("Show/Hide Contact List") );
	connect( globalShowContactList, SIGNAL( triggered(bool) ), this, SLOT( slotShowHide() ) );
	globalShowContactList->setGlobalShortcut( KShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_S) );
	globalShowContactList->setWhatsThis( i18n("Show or hide the contact list") );
	
	KAction *globalSetAway = new KAction( i18n("Set Away/Back"), actionCollection(), QLatin1String("Set Away/Back") );
	connect( globalSetAway, SIGNAL( triggered(bool) ), this, SLOT( slotToggleAway() ) );
	globalSetAway->setGlobalShortcut( KShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_W) );
	
	KGlobalAccel::self()->readSettings();
}

void KopeteWindow::slotShowHide()
{
	if(isActiveWindow())
	{
		d->autoHideTimer->stop(); //no timeouts if active
		hide();
	}
	else
	{
		show();
		//raise() and show() should normaly deIconify the window. but it doesn't do here due
		// to a bug in QT or in KDE  (qt3.1.x or KDE 3.1.x) then, i have to call KWin's method
		if(isMinimized())
			KWin::deIconifyWindow(winId());

		if(!KWin::windowInfo(winId(),NET::WMDesktop).onAllDesktops())
			KWin::setOnDesktop(winId(), KWin::currentDesktop());
		raise();
		activateWindow();
	}
}

void KopeteWindow::slotToggleAway()
{
	Kopete::Away *mAway = Kopete::Away::getInstance();
	if ( mAway->globalAway() )
	{
		Kopete::AccountManager::self()->setAvailableAll();
	}
	else
	{
		QString awayReason = mAway->getMessage( 0 );
		slotGlobalAway();
	}
}

void KopeteWindow::initSystray()
{
	d->tray = KopeteSystemTray::systemTray( this );
#warning PORT ME
//	Kopete::UI::Global::setSysTrayWId( d->tray->winId() );

	QObject::connect( d->tray, SIGNAL( aboutToShowMenu( KMenu * ) ),
	                  this, SLOT( slotTrayAboutToShowMenu( KMenu * ) ) );
	QObject::connect( d->tray, SIGNAL( quitSelected() ), this, SLOT( slotQuit() ) );
}

KopeteWindow::~KopeteWindow()
{
	delete d;
}

bool KopeteWindow::eventFilter( QObject* target, QEvent* event )
{
    KToolBar *toolBar = dynamic_cast<KToolBar*>( target );
    KAction *resetAction = actionCollection()->action( "quicksearch_reset" );

    if ( toolBar && resetAction && resetAction->isPlugged( toolBar ) )
    {

        if ( event->type() == QEvent::Hide )
        {
            resetAction->trigger();
            return true;
        }
        return KMainWindow::eventFilter( target, event );
    }

    return KMainWindow::eventFilter( target, event );
}

void KopeteWindow::loadOptions()
{
	KConfig *config = KGlobal::config();

	toolBar("mainToolBar")->applySettings( config, "ToolBar Settings" );
	toolBar("quickSearchBar")->applySettings( config, "QuickSearchBar Settings" );
	toolBar("editGlobalIdentityBar")->applySettings( config, "EditGlobalIdentityBar Settings" );

	// FIXME: HACK: Is there a way to do that automatic ?
	d->editGlobalIdentityWidget->setIconSize( toolBar("editGlobalIdentityBar")->iconSize() );
	connect(toolBar("editGlobalIdentityBar"), SIGNAL(iconSizeChanged(const QSize &)), d->editGlobalIdentityWidget, SLOT(setIconSize(const QSize &)));

	applyMainWindowSettings( config, "General Options" );
	config->setGroup("General Options");
	QPoint pos = config->readEntry("Position", QPoint());
	move(pos);

	QSize size = config->readEntry("Geometry", QSize() );
	if(size.isEmpty()) // Default size
		resize( QSize(220, 350) );
	else
		resize(size);

	d->autoHide = Kopete::AppearanceSettings::self()->contactListAutoHide();
	d->autoHideTimeout = Kopete::AppearanceSettings::self()->contactListAutoHideTimeout();


	QString tmp = config->readEntry("State", "Shown");
	if ( tmp == "Minimized" && Kopete::BehaviorSettings::self()->showSystemTray())
	{
		showMinimized();
	}
	else if ( tmp == "Hidden" && Kopete::BehaviorSettings::self()->showSystemTray())
	{
		hide();
	}
	else if ( !Kopete::BehaviorSettings::self()->startDocked() || !Kopete::BehaviorSettings::self()->showSystemTray() )
		show();

	d->menubarAction->setChecked( !menuBar()->isHidden() );
	d->statusbarAction->setChecked( !statusBar()->isHidden() );
}

void KopeteWindow::saveOptions()
{
	KConfig *config = KGlobal::config();

	toolBar("mainToolBar")->saveSettings ( config, "ToolBar Settings" );
	toolBar("quickSearchBar")->saveSettings( config, "QuickSearchBar Settings" );
	toolBar("editGlobalIdentityBar")->saveSettings( config, "EditGlobalIdentityBar Settings" );

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

void KopeteWindow::showMenubar()
{
	if( d->menubarAction->isChecked() )
		menuBar()->show();
	else
		menuBar()->hide();
}

void KopeteWindow::showStatusbar()
{
	if( d->statusbarAction->isChecked() )
		statusBar()->show();
	else
		statusBar()->hide();
}

void KopeteWindow::slotToggleShowOffliners()
{
	Kopete::AppearanceSettings::self()->setShowOfflineUsers ( d->actionShowOffliners->isChecked() );
	Kopete::AppearanceSettings::self()->writeConfig();
}

void KopeteWindow::slotToggleShowEmptyGroups()
{
	Kopete::AppearanceSettings::self()->setShowEmptyGroups ( d->actionShowEmptyGroups->isChecked() );
	Kopete::AppearanceSettings::self()->writeConfig();
}

void KopeteWindow::slotConfigChanged()
{
	if( isHidden() && !Kopete::BehaviorSettings::self()->showSystemTray()) // user disabled systray while kopete is hidden, show it!
		show();

	d->actionShowOffliners->setChecked( Kopete::AppearanceSettings::self()->showOfflineUsers() );
	d->actionShowEmptyGroups->setChecked( Kopete::AppearanceSettings::self()->showEmptyGroups() );
}

void KopeteWindow::slotContactListAppearanceChanged()
{
	d->autoHide = Kopete::AppearanceSettings::self()->contactListAutoHide();
	d->autoHideTimeout = Kopete::AppearanceSettings::self()->contactListAutoHideTimeout();

	startAutoHideTimer();
}

void KopeteWindow::slotConfNotifications()
{
	KNotifyConfigWidget::configure( this );
}

void KopeteWindow::slotConfigurePlugins()
{
	if ( !d->pluginConfig )
		d->pluginConfig = new KopetePluginConfig( this );
	d->pluginConfig->show();

	d->pluginConfig->raise();

	KWin::activateWindow( d->pluginConfig->winId() );
}

void KopeteWindow::slotConfGlobalKeys()
{
	KKeyDialog::configure( actionCollection() );
}

void KopeteWindow::slotConfToolbar()
{
	saveMainWindowSettings(KGlobal::config(), "General Options");
	KEditToolbar *dlg = new KEditToolbar(factory());
	connect( dlg, SIGNAL(newToolbarConfig()), this, SLOT(slotUpdateToolbar()) );
	connect( dlg, SIGNAL(finished()) , dlg, SLOT(deleteLater()));
	dlg->show();
}

void KopeteWindow::slotUpdateToolbar()
{
	applyMainWindowSettings(KGlobal::config(), "General Options");
}

void KopeteWindow::slotGlobalAway()
{
	Kopete::AccountManager::self()->setAwayAll( d->globalStatusMessageStored );
}

void KopeteWindow::slotGlobalBusy()
{
	Kopete::AccountManager::self()->setOnlineStatus(
			Kopete::OnlineStatusManager::Busy, d->globalStatusMessageStored );
}

void KopeteWindow::slotGlobalAvailable()
{
	Kopete::AccountManager::self()->setAvailableAll( d->globalStatusMessageStored );
}

void KopeteWindow::slotSetInvisibleAll()
{
	Kopete::AccountManager::self()->setOnlineStatus( Kopete::OnlineStatusManager::Invisible  );
}

void KopeteWindow::slotDisconnectAll()
{
	d->globalStatusMessage->setText( "" );
	d->globalStatusMessageStored = QString();
	Kopete::AccountManager::self()->disconnectAll();
}

bool KopeteWindow::queryClose()
{
	KopeteApplication *app = static_cast<KopeteApplication *>( kapp );
	if ( !app->sessionSaving()	// if we are just closing but not shutting down
		&& !app->isShuttingDown()
		&& Kopete::BehaviorSettings::self()->showSystemTray()
		&& !isHidden() )
		// I would make this a KMessageBox::queuedMessageBox but there doesn't seem to be don'tShowAgain support for those
		KMessageBox::information( this,
								  i18n( "<qt>Closing the main window will keep Kopete running in the "
								        "system tray. Use 'Quit' from the 'File' menu to quit the application.</qt>" ),
								  i18n( "Docking in System Tray" ), "hideOnCloseInfo" );
// 	else	// we are shutting down either user initiated or session management
// 		Kopete::PluginManager::self()->shutdown();

	return true;
}

bool KopeteWindow::queryExit()
{
	KopeteApplication *app = static_cast<KopeteApplication *>( kapp );
 	if ( app->sessionSaving()
		|| app->isShuttingDown() /* only set if KopeteApplication::quitKopete() or
									KopeteApplication::commitData() called */
		|| !Kopete::BehaviorSettings::self()->showSystemTray() /* also close if our tray icon is hidden! */
		|| isHidden() )
	{
		kDebug( 14000 ) << k_funcinfo << " shutting down plugin manager" << endl;
		Kopete::PluginManager::self()->shutdown();
		return true;
	}
	else
		return false;
}

void KopeteWindow::closeEvent( QCloseEvent *e )
{
	// if there's a system tray applet and we are not shutting down then just do what needs to be done if a
	// window is closed.
	KopeteApplication *app = static_cast<KopeteApplication *>( kapp );
	if ( Kopete::BehaviorSettings::self()->showSystemTray() && !app->isShuttingDown() && !app->sessionSaving() ) {
		// BEGIN of code borrowed from KMainWindow::closeEvent
		// Save settings if auto-save is enabled, and settings have changed
		if ( settingsDirty() && autoSaveSettings() )
			saveAutoSaveSettings();

		if ( queryClose() ) {
			e->accept();
		}
		// END of code borrowed from KMainWindow::closeEvent
		kDebug( 14000 ) << k_funcinfo << "just closing because we have a system tray icon" << endl;
	}
	else
	{
		kDebug( 14000 ) << k_funcinfo << "delegating to KMainWindow::closeEvent()" << endl;
		KMainWindow::closeEvent( e );
	}
}

void KopeteWindow::slotQuit()
{
	saveOptions();
	KopeteApplication *app = static_cast<KopeteApplication *>( kapp );
	app->quitKopete();
}

void KopeteWindow::slotPluginLoaded( Kopete::Plugin *  p  )
{
	guiFactory()->addClient(p);
}

void KopeteWindow::slotAllPluginsLoaded()
{
//	actionConnect->setEnabled(true);
	d->actionDisconnect->setEnabled(true);
}

void KopeteWindow::slotAccountRegistered( Kopete::Account *account )
{
//	kDebug(14000) << k_funcinfo << "Called." << endl;
	if ( !account )
		return;

	//enable the connect all toolbar button
//	actionConnect->setEnabled(true);
	d->actionDisconnect->setEnabled(true);

	connect( account->myself(),
		SIGNAL(onlineStatusChanged( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus &) ),
		this, SLOT( slotAccountStatusIconChanged( Kopete::Contact * ) ) );

//	connect( account, SIGNAL( iconAppearanceChanged() ), SLOT( slotAccountStatusIconChanged() ) );
	connect( account, SIGNAL( colorChanged(const QColor& ) ), SLOT( slotAccountStatusIconChanged() ) );

	//FIXME:  i don't know why this is require, cmmenting for now)
#if 0
	connect( account->myself(),
		SIGNAL(propertyChanged( Kopete::Contact *, const QString &, const QVariant &, const QVariant & ) ),
		this, SLOT( slotAccountStatusIconChanged( Kopete::Contact* ) ) );
#endif

	KopeteAccountStatusBarIcon *sbIcon = new KopeteAccountStatusBarIcon( account, d->statusBarWidget );
	connect( sbIcon, SIGNAL( rightClicked( Kopete::Account *, const QPoint & ) ),
		SLOT( slotAccountStatusIconRightClicked( Kopete::Account *,
		const QPoint & ) ) );
	connect( sbIcon, SIGNAL( leftClicked( Kopete::Account *, const QPoint & ) ),
		SLOT( slotAccountStatusIconRightClicked( Kopete::Account *,
		const QPoint & ) ) );

	d->accountStatusBarIcons.insert( account, sbIcon );
	slotAccountStatusIconChanged( account->myself() );
	
	// add an item for this account to the add contact actionmenu
	QString s = QString("actionAdd%1Contact").arg( account->accountId() );
	KAction *action = new KAction( KIcon(account->accountIcon()), account->accountLabel(), actionCollection(), s );
	connect( action, SIGNAL(triggered(bool)), d->addContactMapper, SLOT(map()) );

	d->addContactMapper->setMapping( action, account->protocol()->pluginId() + QChar(0xE000) + account->accountId() );
	d->actionAddContact->addAction( action );
}

void KopeteWindow::slotAccountUnregistered( const Kopete::Account *account)
{
	kDebug(14000) << k_funcinfo << endl;
	QList<Kopete::Account *> accounts = Kopete::AccountManager::self()->accounts();
	if (accounts.isEmpty())
	{
//		actionConnect->setEnabled(false);
		d->actionDisconnect->setEnabled(false);
	}

	KopeteAccountStatusBarIcon *sbIcon = d->accountStatusBarIcons[account];

	if( !sbIcon )
		return;

	d->accountStatusBarIcons.remove( account );
	delete sbIcon;

	makeTrayToolTip();
	
	// update add contact actionmenu
	QString s = QString("actionAdd%1Contact").arg( account->accountId() );
	KAction *action = actionCollection()->action( s );
	if ( action )
	{
		kDebug(14000) << " found KAction " << action << " with name: " << action->objectName() << endl;
		d->addContactMapper->removeMappings( action );
		d->actionAddContact->removeAction( action );
	}
}

void KopeteWindow::slotAccountStatusIconChanged()
{
	if ( const Kopete::Account *from = dynamic_cast<const Kopete::Account*>(sender()) )
		slotAccountStatusIconChanged( from->myself() );
}

void KopeteWindow::slotAccountStatusIconChanged( Kopete::Contact *contact )
{
	kDebug( 14000 ) << k_funcinfo << contact->property( Kopete::Global::Properties::self()->statusMessage() ).value() << endl;
	// update the global status label if the change doesn't 
//	QString newAwayMessage = contact->property( Kopete::Global::Properties::self()->awayMessage() ).value().toString();
	Kopete::OnlineStatus status = contact->onlineStatus();
// 	if ( status.status() != Kopete::OnlineStatus::Connecting )
// 	{
// 		QString globalMessage = m_globalStatusMessage->text();
// 		if ( newAwayMessage != globalMessage )
// 			m_globalStatusMessage->setText( ""i18n("status message to show when different accounts have different status messages", "(multiple)" );
// 	}
//	kDebug(14000) << k_funcinfo << "Icons: '" <<
//		status.overlayIcons() << "'" << endl;

	if ( status != Kopete::OnlineStatus::Connecting )
	{
		d->globalStatusMessageStored = contact->property( Kopete::Global::Properties::self()->statusMessage() ).value().toString();
		d->globalStatusMessage->setText( d->globalStatusMessageStored );
	}
	
	KopeteAccountStatusBarIcon *i = d->accountStatusBarIcons[ contact->account() ];
	if( !i )
		return;

	// Adds tooltip for each status icon,
	// useful in case you have many accounts
	// over one protocol
	i->setToolTip( contact->toolTip() );

	// Because we want null pixmaps to detect the need for a loadMovie
	// we can't use the SmallIcon() method directly
#if 0
	KIconLoader *loader = KGlobal::instance()->iconLoader();

	QMovie *mv = new QMovie(loader->loadMovie( status.overlayIcons().first(), K3Icon::Small ));

	if ( mv->isNull() )
	{
		// No movie found, fallback to pixmap
		// Get the icon for our status
#endif
		//QPixmap pm = SmallIcon( icon );
		QPixmap pm = status.iconFor( contact->account() );

		// No Pixmap found, fallback to Unknown
		if( pm.isNull() )
			i->setPixmap( KIconLoader::unknown() );
		else
			i->setPixmap( pm );
#if 0
	}
	else
	{
		//kDebug( 14000 ) << k_funcinfo << "Using movie."  << endl;
		i->setMovie( mv );
	}
#endif
	makeTrayToolTip();
}

void KopeteWindow::makeTrayToolTip()
{
	//the tool-tip of the systemtray.
	if(d->tray)
	{
		QString tt = QLatin1String("<qt>");
		QList<Kopete::Account *> accountList = Kopete::AccountManager::self()->accounts();
		foreach(Kopete::Account *a, accountList)
		{
			Kopete::Contact *self = a->myself();
			tt += i18nc( "Account tooltip information: <nobr>ICON <b>PROTOCOL:</b> NAME (<i>STATUS</i>)<br/>",
			             "<nobr><img src=\"kopete-account-icon:%3:%4\"> <b>%1:</b> %2 (<i>%5</i>)<br/>",
				     a->protocol()->displayName(), a->accountLabel(), QString(QUrl::toPercentEncoding( a->protocol()->pluginId() )),
				     QString(QUrl::toPercentEncoding( a->accountId() )), self->onlineStatus().description() );
		}
		tt += QLatin1String("</qt>");
		d->tray->setToolTip(tt);
	}
}

void KopeteWindow::slotAccountStatusIconRightClicked( Kopete::Account *account, const QPoint &p )
{
	KActionMenu *actionMenu = account->actionMenu();
	if ( !actionMenu )
		return;

	connect( actionMenu->menu(), SIGNAL( aboutToHide() ), actionMenu, SLOT( deleteLater() ) );
	actionMenu->menu()->popup( p );
}

void KopeteWindow::slotTrayAboutToShowMenu( KMenu * popup )
{
	popup->clear();
	popup->addTitle( qApp->windowIcon(), KInstance::caption() );

	QList<Kopete::Account *> accountList = Kopete::AccountManager::self()->accounts();
	foreach(Kopete::Account *a, accountList)
	{
		KActionMenu *menu = a->actionMenu();
		if( menu )
			popup->addAction( menu );

		connect(popup , SIGNAL(aboutToHide()) , menu , SLOT(deleteLater()));
	}

	popup->addSeparator();
	popup->addAction( d->actionAwayMenu );
	popup->addSeparator();
	popup->addAction( d->actionPrefs );
	popup->addAction( d->actionAddContact );
	popup->addSeparator();

	KAction *action = 0;
	action = d->tray->actionCollection()->action( "minimizeRestore" );
	popup->addAction( action );

	action = d->tray->actionCollection()->action( KStdAction::name( KStdAction::Quit ) );
	popup->addAction( action );
}

void KopeteWindow::showExportDialog()
{
	KabcExportWizard* wizard = new KabcExportWizard( this );
	wizard->setObjectName( QLatin1String("export_contact_dialog") );	
	wizard->show();
}

void KopeteWindow::leaveEvent( QEvent * )
{
	startAutoHideTimer();
}

void KopeteWindow::showEvent( QShowEvent * )
{
	startAutoHideTimer();
}

void KopeteWindow::slotAutoHide()
{
	if ( this->geometry().contains( QCursor::pos() ) == false )
	{
		/* The autohide-timer doesn't need to emit
		* timeouts when the window is hidden already. */
		d->autoHideTimer->stop();
		hide();
	}
}

void KopeteWindow::startAutoHideTimer()
{
	if ( d->autoHideTimeout > 0 && d->autoHide == true && isVisible() && Kopete::BehaviorSettings::self()->showSystemTray())
		d->autoHideTimer->start( d->autoHideTimeout * 1000 );
}

// Iterate each connected account, updating its status message bug keeping the 
// same onlinestatus.  Then update Kopete::Away and the UI.
void KopeteWindow::setStatusMessage( const QString & message )
{
	bool changed = false;
	QList<Kopete::Account*> accountList = Kopete::AccountManager::self()->accounts();
	foreach(Kopete::Account *account, accountList)
	{
		Kopete::Contact *self = account->myself();
		bool isInvisible = self && self->onlineStatus().status() == Kopete::OnlineStatus::Invisible;
		if ( account->isConnected() && !isInvisible )
		{
			changed = true;
			account->setOnlineStatus( self->onlineStatus(), message );
		}
	}
	Kopete::Away::getInstance()->setGlobalAwayMessage( message );
	d->globalStatusMessageStored = message;
	d->globalStatusMessage->setText( message );
}

void KopeteWindow::slotBuildStatusMessageMenu()
{
	//There are two ways for the menu to be avtivated:
	//Via the menu or the 'Global Status Message' button
	//Find out which menu asked us to be built
	QObject * senderObj = const_cast<QObject *>( sender() );
	d->globalStatusMessageMenu = static_cast<KMenu *>( senderObj );
	d->globalStatusMessageMenu->clear();

	connect( d->globalStatusMessageMenu, SIGNAL( activated( int ) ),
		SLOT( slotStatusMessageSelected( int ) ) );

	d->globalStatusMessageMenu->addTitle( i18n("Status Message") );
	//BEGIN: Add new message widget to the Set Status Message Menu.
	KHBox * newMessageBox = new KHBox( 0 );
	newMessageBox->setMargin( 1 );
	QLabel * newMessagePix = new QLabel( newMessageBox );
	newMessagePix->setPixmap( SmallIcon( "edit" ) );
	QLabel * newMessageLabel = new QLabel( i18n( "Add " ), newMessageBox );
	d->newMessageEdit = new QLineEdit( newMessageBox );
	newMessageBox->setFocusProxy( d->newMessageEdit );
	newMessageBox->setFocusPolicy( Qt::ClickFocus );
	newMessageLabel->setFocusProxy( d->newMessageEdit );
	newMessageLabel->setBuddy( d->newMessageEdit );
	newMessageLabel->setFocusPolicy( Qt::ClickFocus );
	newMessagePix->setFocusProxy( d->newMessageEdit );
	newMessagePix->setFocusPolicy( Qt::ClickFocus );
	connect( d->newMessageEdit, SIGNAL( returnPressed() ), SLOT( slotNewStatusMessageEntered() ) );

	KAction *newMessageAction = new KAction( KIcon("edit"), i18n("New Message..."), 0, "new_message" );
	newMessageAction->setDefaultWidget( newMessageBox );

	d->globalStatusMessageMenu->addAction( newMessageAction );
	//END

	// NOTE: The following code still use insertItem because it require the behavior of those (-DarkShock)
	int i = 0;
	d->globalStatusMessageMenu->insertItem( SmallIcon( "remove" ), i18n( "No Message" ), i++ );
	d->globalStatusMessageMenu->addSeparator();
	
	QStringList awayMessages = Kopete::Away::getInstance()->getMessages();
	foreach(QString message, awayMessages)
	{
		d->globalStatusMessageMenu->insertItem( KStringHandler::rsqueeze( message ), i );
	}
	//connect( m_globalStatusMessageMenu, SIGNAL( aboutToHide() ), m_globalStatusMessageMenu, SLOT( deleteLater() ) );

	//m_newMessageEdit->setFocus();

	//messageMenu->popup( e->globalPos(), 1 );
}

void KopeteWindow::slotStatusMessageSelected( int i )
{
	if ( 0 == i )
		setStatusMessage( "" );
	else if( i > 0 )
		setStatusMessage(  Kopete::Away::getInstance()->getMessage( i - 1 ) );
}

void KopeteWindow::slotNewStatusMessageEntered()
{
	d->globalStatusMessageMenu->close();
	QString newMessage = d->newMessageEdit->text();
	if ( !newMessage.isEmpty() )
		Kopete::Away::getInstance()->addMessage( newMessage );
	setStatusMessage( d->newMessageEdit->text() );
}

void KopeteWindow::slotGlobalStatusMessageIconClicked( const QPoint &position )
{
	KMenu *statusMessageIconMenu = new KMenu(this);

	connect(statusMessageIconMenu, SIGNAL( aboutToShow() ),
		this, SLOT(slotBuildStatusMessageMenu()));

	statusMessageIconMenu->popup(position);
}

void KopeteWindow::slotAddContactDialogInternal( const QString & accountIdentifier )
{
	QString protocolId = accountIdentifier.section( QChar(0xE000), 0, 0 );
	QString accountId = accountIdentifier.section( QChar(0xE000), 1, 1 );
	Kopete::Account *account = Kopete::AccountManager::self()->findAccount( protocolId, accountId );
 	showAddContactDialog( account );
}

void KopeteWindow::showAddContactDialog( Kopete::Account * account )
{
	if ( !account ) {
		kDebug( 14000 ) << k_funcinfo << "no account given" << endl; 
		return;
	}

	KDialog *addDialog = new KDialog( this );
	addDialog->setCaption( i18n( "Add Contact" ) );
	addDialog->setButtons( KDialog::Ok | KDialog::Cancel );
	addDialog->setDefaultButton( KDialog::Ok );
	addDialog->showButtonSeparator( true );

	KVBox * mainWid = new KVBox( addDialog );
	
	AddContactPage *addContactPage =
		account->protocol()->createAddContactWidget( mainWid, account );

	QWidget* groupKABC = new QWidget( mainWid );
	groupKABC->setObjectName( "groupkabcwidget" );
	Ui::GroupKABCSelectorWidget ui_groupKABC;
	ui_groupKABC.setupUi( groupKABC );

	// Populate the groups list
	Kopete::GroupList groups=Kopete::ContactList::self()->groups();
	QHash<QString, Kopete::Group*> groupItems;
	foreach( Kopete::Group *group, groups )
    {
		QString groupname = group->displayName();
		if ( !groupname.isEmpty() )
		{
			groupItems.insert( groupname, group );
			ui_groupKABC.groupCombo->addItem( groupname );
		}
	}

	if (!addContactPage)
	{
		kDebug(14000) << k_funcinfo <<
			"Error while creating addcontactpage" << endl;
	}
	else
	{
		addDialog->setMainWidget( mainWid );
		if( addDialog->exec() == QDialog::Accepted )
		{
			if( addContactPage->validateData() )
			{
				Kopete::MetaContact * metacontact = new Kopete::MetaContact();
				metacontact->addToGroup( groupItems[ ui_groupKABC.groupCombo->currentText() ] );
				metacontact->setMetaContactId( ui_groupKABC.widAddresseeLink->uid() );
				if (addContactPage->apply( account, metacontact ))
				{
					Kopete::ContactList::self()->addMetaContact( metacontact );
				}
				else
				{
					delete metacontact;
				}
			}
		}
	}
	addDialog->deleteLater();
}

#include "kopetewindow.moc"
// vim: set noet ts=4 sts=4 sw=4:
