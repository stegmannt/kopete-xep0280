
#define POSIX //FIXME
#include "talk/xmpp/constants.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/jid.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlprinter.h"
#include "talk/base/network.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/base/helpers.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/p2p/base/sessionclient.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/thread.h"
#include "talk/base/socketaddress.h"
#include "talk/session/phone/call.h"
#include "talk/session/phone/phonesessionclient.h"
#include "talk/p2p/client/sessionsendtask.h"
#include "talk/p2p/client/sessionmanagertask.h"

#include "talk/base/physicalsocketserver.h"

#include <qstring.h>
#include <qdom.h>



#include "im.h"
#include "xmpp.h"
#include "xmpp_xmlcommon.h"
#include "jinglevoicecaller.h"
#include "jabberprotocol.h"

// Should change in the future
#define JINGLE_NS "http://www.google.com/session"

#include "jabberaccount.h"
#include <kdebug.h>
#define qDebug( X )  kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << X << endl
#define qWarning( X )  kWarning() <<k_funcinfo<< X << endl

#include "jinglevoicecaller.moc"

// ----------------------------------------------------------------------------

class JingleIQResponder : public XMPP::Task
{
public:
	JingleIQResponder(XMPP::Task *);
	~JingleIQResponder();

	bool take(const QDomElement &);
};

/**
 * \class JingleIQResponder
 * \brief A task that responds to jingle candidate queries with an empty reply.
 */
 
JingleIQResponder::JingleIQResponder(Task *parent) :Task(parent)
{
}

JingleIQResponder::~JingleIQResponder()
{
}

#warning "This function wouldn't link, so it is copied here"
QDomElement createIQ(QDomDocument *doc, const QString &type, const QString &to, const QString &id)
{
	QDomElement iq = doc->createElement("iq");
	if(!type.isEmpty())
		iq.setAttribute("type", type);
	if(!to.isEmpty())
		iq.setAttribute("to", to);
	if(!id.isEmpty())
		iq.setAttribute("id", id);

	return iq;
}

bool JingleIQResponder::take(const QDomElement &e)
{
	if(e.tagName() != "iq")
		return false;
	
	QDomElement first = e.firstChild().toElement();
	if (!first.isNull() && first.attribute("xmlns") == JINGLE_NS) {
		QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
		send(iq);
		return true;
	}
	
	return false;
}

// ----------------------------------------------------------------------------

/**
 * \brief A class for handling signals from libjingle.
 */
class JingleClientSlots : public sigslot::has_slots<> 
{
public:
	JingleClientSlots(JingleVoiceCaller *voiceCaller);

	void callCreated(cricket::Call *call);
	void callDestroyed(cricket::Call *call);
	void sendStanza(cricket::SessionClient*, const buzz::XmlElement *stanza);
	void requestSignaling();
	void stateChanged(cricket::Call *call, cricket::Session *session, cricket::Session::State state);
/*	void jingleInfo(const std::string &relay_token,
		const std::vector<std::string> &relay_addresses,
		const std::vector<talk_base::SocketAddress> &stun_addresses);*/
	

private:
	JingleVoiceCaller* voiceCaller_;
};


JingleClientSlots::JingleClientSlots(JingleVoiceCaller *voiceCaller) : voiceCaller_(voiceCaller)
{
}

void JingleClientSlots::callCreated(cricket::Call *call) 
{
	kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << endl;
	call->SignalSessionState.connect(this, &JingleClientSlots::stateChanged);
}

void JingleClientSlots::callDestroyed(cricket::Call *call)
{
	qDebug("JingleClientSlots: Call destroyed");
	Jid jid(call->sessions()[0]->remote_name().c_str());
	if (voiceCaller_->calling(jid)) {
		qDebug(QString("Removing unterminated call to %1").arg(jid.full()));
		voiceCaller_->removeCall(jid);
		emit voiceCaller_->terminated(jid);
	}
}

void JingleClientSlots::sendStanza(cricket::SessionClient*, const buzz::XmlElement *stanza) 
{
	QString st(stanza->Str().c_str());
	st.replace("cli:iq","iq");
	st.replace(":cli=","=");
	fprintf(stderr,"bling\n");
	voiceCaller_->sendStanza(st.toLatin1());
	fprintf(stderr,"blong\n");
	fprintf(stderr,"Sending stanza \n%s\n\n",st.toLatin1());
}

void JingleClientSlots::requestSignaling() 
{
	voiceCaller_->session_manager_->OnSignalingReady();
}

void JingleClientSlots::stateChanged(cricket::Call *call, cricket::Session *session, cricket::Session::State state) 
{
	qDebug(QString("jinglevoicecaller.cpp: State changed (%1)").arg(state));
	// Why is c_str() stuff needed to make it compile on OS X ?
	Jid jid(session->remote_name().c_str());

	if (state == cricket::Session::STATE_INIT) { }
	else if (state == cricket::Session::STATE_SENTINITIATE) { 
		voiceCaller_->registerCall(jid,call);
	}
	else if (state == cricket::Session::STATE_RECEIVEDINITIATE) {
		voiceCaller_->registerCall(jid,call);
		emit voiceCaller_->incoming(jid);
	}
	else if (state == cricket::Session::STATE_SENTACCEPT) { }
	else if (state == cricket::Session::STATE_RECEIVEDACCEPT) {
		emit voiceCaller_->accepted(jid);
	}
	else if (state == cricket::Session::STATE_SENTMODIFY) { }
	else if (state == cricket::Session::STATE_RECEIVEDMODIFY) {
		qWarning(QString("jinglevoicecaller.cpp: RECEIVEDMODIFY not implemented yet (was from %1)").arg(jid.full()));
	}
	else if (state == cricket::Session::STATE_SENTREJECT) { }
	else if (state == cricket::Session::STATE_RECEIVEDREJECT) {
		voiceCaller_->removeCall(jid);
		emit voiceCaller_->rejected(jid);
	}
	else if (state == cricket::Session::STATE_SENTREDIRECT) { }
	else if (state == cricket::Session::STATE_SENTTERMINATE) {
		voiceCaller_->removeCall(jid);
		emit voiceCaller_->terminated(jid);
	}
	else if (state == cricket::Session::STATE_RECEIVEDTERMINATE) {
		voiceCaller_->removeCall(jid);
		emit voiceCaller_->terminated(jid);
	}
	else if (state == cricket::Session::STATE_INPROGRESS) {
		emit voiceCaller_->in_progress(jid);
	}
}

/*void JingleClientSlots::jingleInfo(const std::string &relay_token,
		const std::vector<std::string> &relay_addresses,
		const std::vector<talk_base::SocketAddress> &stun_addresses)
{
	JingleVoiceCaller::port_allocator_->SetStunHosts(stun_addresses);
	JingleVoiceCaller::port_allocator_->SetRelayHosts(relay_addresses);
	JingleVoiceCaller::port_allocator_->SetRelayToken(relay_token);
}*/

// ----------------------------------------------------------------------------

/**
 * \class JingleVoiceCaller
 * \brief A Voice Calling implementation using libjingle.
 */

JingleVoiceCaller::JingleVoiceCaller(PsiAccount* acc) : VoiceCaller(acc)
{
	initialized_ = false;
}

void JingleVoiceCaller::initialize()
{
	if (initialized_)
		return;

	QString jid = ((ClientStream&) account()->client()->client()->stream()).jid().full();
	qDebug(QString("jinglevoicecaller.cpp: Creating new caller for %1").arg(jid));
	if (jid.isEmpty()) {
		qWarning("jinglevoicecaller.cpp: Empty JID");
		return;
	}

	buzz::Jid j(jid.toAscii().constData());
	cricket::InitRandom(j.Str().c_str(),j.Str().size());

	// Global variables
	if (!socket_server_) {
		socket_server_ = new talk_base::PhysicalSocketServer();
		talk_base::Thread *t = new talk_base::Thread((talk_base::PhysicalSocketServer*)(socket_server_));
		talk_base::ThreadManager::SetCurrent(t);
		t->Start();
		thread_ = t;

		stun_addr_ = new talk_base::SocketAddress("64.233.167.126",19302);
		network_manager_ = new talk_base::NetworkManager();
		port_allocator_ = new cricket::BasicPortAllocator((talk_base::NetworkManager*)(network_manager_), (talk_base::SocketAddress*)(stun_addr_), /* relay server */ NULL);
	}

	// Session manager
	session_manager_ = new cricket::SessionManager((cricket::PortAllocator*)(port_allocator_), thread_);
	slots_ = new JingleClientSlots(this); 
	session_manager_->SignalRequestSignaling.connect(slots_, &JingleClientSlots::requestSignaling);
	session_manager_->OnSignalingReady();

	//session_manager_task_ = new cricket::SessionManagerTask(xmpp_client_, session_manager_);
	//session_manager_task_->EnableOutgoingMessages();
	//session_manager_task_->Start();

	// Phone Client
	phone_client_ = new cricket::PhoneSessionClient(j, (cricket::SessionManager*)(session_manager_));
	phone_client_->SignalCallCreate.connect(slots_, &JingleClientSlots::callCreated);
	phone_client_->SignalCallDestroy.connect(slots_, &JingleClientSlots::callDestroyed);

/*	// Jingle network addresses
	buzz::JingleInfoTask *jit = new buzz::JingleInfoTask(xmpp_client_);
	jit->RefreshJingleInfoNow();
	jit->SignalJingleInfo.connect(this, &CallClient::OnJingleInfo);
	jit->Start();*/
	
	// IQ Responder
	new JingleIQResponder(account()->client()->rootTask());

	// Listen to incoming packets
	connect(account()->client()->client(),SIGNAL(xmlIncoming(const QString&)),SLOT(receiveStanza(const QString&)));

	initialized_ = true;
}


void JingleVoiceCaller::deinitialize()
{
	if (!initialized_)
		return;

	// Stop listening to incoming packets
	disconnect(account()->client(),SIGNAL(xmlIncoming(const QString&)),this,SLOT(receiveStanza(const QString&)));

	// Disconnect signals (is this needed)
	//phone_client_->SignalCallCreate.disconnect(slots_);
	//phone_client_->SignalSendStanza.disconnect(slots_);
	
	// Delete objects
	delete phone_client_;
	delete session_manager_;
	delete slots_;

	initialized_ = false;
}


JingleVoiceCaller::~JingleVoiceCaller()
{
}

bool JingleVoiceCaller::calling(const Jid& jid)
{
	return calls_.contains(jid.full());
}

void JingleVoiceCaller::call(const Jid& jid)
{
	qDebug(QString("jinglevoicecaller.cpp: Calling %1").arg(jid.full()));
	cricket::Call *c = ((cricket::PhoneSessionClient*)(phone_client_))->CreateCall();
	c->InitiateSession(buzz::Jid(jid.full().toAscii().constData()), NULL);
	phone_client_->SetFocus(c);
}

void JingleVoiceCaller::accept(const Jid& j)
{
	qDebug("jinglevoicecaller.cpp: Accepting call");
	cricket::Call* call = calls_[j.full()];
	if (call != NULL) {
		call->AcceptSession(call->sessions()[0]);
		phone_client_->SetFocus(call);
	}
}

void JingleVoiceCaller::reject(const Jid& j)
{
	qDebug("jinglevoicecaller.cpp: Rejecting call");
	cricket::Call* call = calls_[j.full()];
	if (call != NULL) {
		call->RejectSession(call->sessions()[0]);
		calls_.remove(j.full());
	}
}

void JingleVoiceCaller::terminate(const Jid& j)
{
	qDebug(QString("jinglevoicecaller.cpp: Terminating call to %1").arg(j.full()));
	cricket::Call* call = calls_[j.full()];
	if (call != NULL) {
		call->Terminate();
		calls_.remove(j.full());
	}
}

void JingleVoiceCaller::sendStanza(const char* stanza)
{
	account()->client()->send(QString(stanza));
}

void JingleVoiceCaller::registerCall(const Jid& jid, cricket::Call* call)
{
	qDebug("jinglevoicecaller.cpp: Registering call\n");
	if (!calls_.contains(jid.full())) {
		calls_[jid.full()] = call;
	}
// 	else {
// 		qWarning("jinglevoicecaller.cpp: Auto-rejecting call because another call is currently open");
// 		call->RejectSession(call->sessions()[0]);
// 	}
}

void JingleVoiceCaller::removeCall(const Jid& j)
{
	qDebug(QString("JingleVoiceCaller: Removing call to %1").arg(j.full()));
	calls_.remove(j.full());
}

void JingleVoiceCaller::receiveStanza(const QString& stanza)
{
	QDomDocument doc;
	doc.setContent(stanza);

	// Check if it is offline presence from an open chat
	if (doc.documentElement().tagName() == "presence") {
		Jid from = Jid(doc.documentElement().attribute("from"));
		QString type = doc.documentElement().attribute("type");
		if (type == "unavailable" && calls_.contains(from.full())) {
			qDebug("JingleVoiceCaller: User went offline without closing a call.");
			removeCall(from);
			emit terminated(from);
		}
		return;
	}
	
	// Check if the packet is destined for libjingle.
	// We could use Session::IsClientStanza to check this, but this one crashes
	// for some reason.
	QDomNode n = doc.documentElement().firstChild();
	bool ok = false;
	while (!n.isNull() && !ok) {
		QDomElement e = n.toElement();
		if (!e.isNull() && e.attribute("xmlns") == JINGLE_NS) {
			ok = true;
		}
		n = n.nextSibling();
	}
	
	// Spread the word
	if (ok) {
		//qDebug(QString("jinglevoicecaller.cpp: Handing down %1").arg(stanza));
		buzz::XmlElement *e = buzz::XmlElement::ForStr(stanza.toAscii().constData());
		session_manager_->OnIncomingMessage(e);
	}
}

talk_base::SocketServer* JingleVoiceCaller::socket_server_ = NULL;
talk_base::Thread* JingleVoiceCaller::thread_ = NULL;
talk_base::NetworkManager* JingleVoiceCaller::network_manager_ = NULL;
cricket::BasicPortAllocator* JingleVoiceCaller::port_allocator_ = NULL;
talk_base::SocketAddress* JingleVoiceCaller::stun_addr_ = NULL;
