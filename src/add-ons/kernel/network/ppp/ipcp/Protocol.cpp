//-----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//
//  Copyright (c) 2003-2004 Waldemar Kornewald, Waldemar.Kornewald@web.de
//-----------------------------------------------------------------------

#include "Protocol.h"
#include <KPPPConfigurePacket.h>
#include <KPPPInterface.h>
#include <settings_tools.h>

#include <LockerHelper.h>

#include <cstring>
#include <netinet/in_var.h>
#include <core_funcs.h>
#include <sys/sockio.h>


#define PPP_STATE_MACHINE_TIMEOUT			3000000
	// 3 seconds


IPCP::IPCP(KPPPInterface& interface, driver_parameter *settings)
	: KPPPProtocol("IPCP", PPP_NCP_PHASE, IPCP_PROTOCOL, PPP_PROTOCOL_LEVEL,
		AF_INET, 0, interface, settings, PPP_INCLUDES_NCP),
	fDefaultRoute(NULL),
	fRequestPrimaryDNS(false),
	fRequestSecondaryDNS(false),
	fState(PPP_INITIAL_STATE),
	fID(system_time() & 0xFF),
	fMaxRequest(10),
	fMaxTerminate(2),
	fMaxNak(5),
	fRequestID(0),
	fTerminateID(0),
	fNextTimeout(0)
{
	ProfileChanged();
}


IPCP::~IPCP()
{
	RemoveRoutes();
}


status_t
IPCP::StackControl(uint32 op, void *data)
{
#if DEBUG
	dprintf("IPCP: StackControl(op=%ld)\n", op);
#endif
	
	// TODO:
	// check values
	
	switch(op) {
		case SIOCSIFADDR:
		break;
		
		case SIOCSIFFLAGS:
		break;
		
		case SIOCSIFDSTADDR:
		break;
		
		case SIOCSIFNETMASK:
		break;
		
		default:
			dprintf("IPCP: Unknown ioctl: %ld\n", op);
			return KPPPProtocol::StackControl(op, data);
	}
	
	return B_OK;
}


void
IPCP::ProfileChanged()
{
	// reset configurations
	memset(&fLocalConfiguration, 0, sizeof(ipcp_configuration));
	memset(&fPeerConfiguration, 0, sizeof(ipcp_configuration));
	
	// reset requests
	memset(&fLocalRequests, 0, sizeof(ipcp_requests));
	memset(&fPeerRequests, 0, sizeof(ipcp_requests));
	
	// Parse settings:
	// "Local" and "Peer" describe each side's settings
	const driver_parameter *profile
		= Interface().Profile().SettingsFor("protocol", "ipcp");
	ParseSideRequests(get_parameter_with_name("Local", profile), PPP_LOCAL_SIDE);
	ParseSideRequests(get_parameter_with_name("Peer", profile), PPP_PEER_SIDE);
}


bool
IPCP::Up()
{
#if DEBUG
	dprintf("IPCP: Up() state=%d\n", State());
#endif
	
	// Servers do not send a configure-request when Up() is called. They wait until
	// the client requests this protocol.
	if(Interface().Mode() == PPP_SERVER_MODE)
		return true;
	
	LockerHelper locker(fLock);
	
	switch(State()) {
		case PPP_INITIAL_STATE:
			NewState(PPP_REQ_SENT_STATE);
			InitializeRestartCount();
			locker.UnlockNow();
			SendConfigureRequest();
		break;
		
		default:
			;
	}
	
	return true;
}


bool
IPCP::Down()
{
#if DEBUG
	dprintf("IPCP: Down() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	switch(Interface().Phase()) {
		case PPP_DOWN_PHASE:
			// interface finished terminating
			NewState(PPP_INITIAL_STATE);
			locker.UnlockNow();
			ReportDownEvent();
				// this will also reset and update addresses
		break;
		
/*		case PPP_TERMINATION_PHASE:
			// interface is terminating
		break;
		
		case PPP_ESTABLISHMENT_PHASE:
			// interface is reconfiguring
		break;
*/		
		case PPP_ESTABLISHED_PHASE:
			// terminate this NCP individually (block until we finished terminating)
			if(State() != PPP_INITIAL_STATE && State() != PPP_CLOSING_STATE) {
				NewState(PPP_CLOSING_STATE);
				InitializeRestartCount();
				locker.UnlockNow();
				SendTerminateRequest();
			}
			
			while(State() == PPP_CLOSING_STATE)
				snooze(50000);
		break;
		
		default:
			;
	}
	
	return true;
}


status_t
IPCP::Send(struct mbuf *packet, uint16 protocolNumber = IPCP_PROTOCOL)
{
#if DEBUG
	dprintf("IPCP: Send(0x%X)\n", protocolNumber);
#endif
	
	if((protocolNumber == IP_PROTOCOL && State() == PPP_OPENED_STATE)
			|| protocolNumber == IPCP_PROTOCOL)
		return SendToNext(packet, protocolNumber);
	
	dprintf("IPCP: Send() failed because of wrong state or protocol number!\n");
	
	m_freem(packet);
	return B_ERROR;
}


status_t
IPCP::Receive(struct mbuf *packet, uint16 protocolNumber)
{
#if DEBUG
	dprintf("IPCP: Receive(0x%X)\n", protocolNumber);
#endif
	
	if(!packet)
		return B_ERROR;
	
	if(protocolNumber == IP_PROTOCOL)
		return ReceiveIPPacket(packet, protocolNumber);
	
	if(protocolNumber != IPCP_PROTOCOL)
		return PPP_UNHANDLED;
	
	ppp_lcp_packet *data = mtod(packet, ppp_lcp_packet*);
	
	// remove padding
	int32 length = packet->m_len;
	if(packet->m_flags & M_PKTHDR)
		length = packet->m_pkthdr.len;
	length -= ntohs(data->length);
	if(length)
		m_adj(packet, -length);
	
	if(ntohs(data->length) < 4)
		return B_ERROR;
	
	// packet is freed by event methods
	switch(data->code) {
		case PPP_CONFIGURE_REQUEST:
			RCREvent(packet);
		break;
		
		case PPP_CONFIGURE_ACK:
			RCAEvent(packet);
		break;
		
		case PPP_CONFIGURE_NAK:
		case PPP_CONFIGURE_REJECT:
			RCNEvent(packet);
		break;
		
		case PPP_TERMINATE_REQUEST:
			RTREvent(packet);
		break;
		
		case PPP_TERMINATE_ACK:
			RTAEvent(packet);
		break;
		
		case PPP_CODE_REJECT:
			RXJBadEvent(packet);
				// we implemented the minimum requirements
		break;
		
		default:
			RUCEvent(packet);
			return PPP_REJECTED;
	}
	
	return B_OK;
}


status_t
IPCP::ReceiveIPPacket(struct mbuf *packet, uint16 protocolNumber)
{
	if(protocolNumber != IP_PROTOCOL || State() != PPP_OPENED_STATE)
		return PPP_UNHANDLED;
	
	// TODO: add VJC support (the packet would be decoded here)
	
	if (gProto[IPPROTO_IP] && gProto[IPPROTO_IP]->pr_input) {
		gProto[IPPROTO_IP]->pr_input(packet, 0);
		return B_OK;
	} else {
		dprintf("IPCP: Error: Could not find input function for IP!\n");
		m_freem(packet);
		return B_ERROR;
	}
}


void
IPCP::Pulse()
{
	LockerHelper locker(fLock);
	if(fNextTimeout == 0 || fNextTimeout > system_time())
		return;
	fNextTimeout = 0;
	locker.UnlockNow();
	
	switch(State()) {
		case PPP_CLOSING_STATE:
			if(fTerminateCounter <= 0)
				TOBadEvent();
			else
				TOGoodEvent();
		break;
		
		case PPP_REQ_SENT_STATE:
		case PPP_ACK_RCVD_STATE:
		case PPP_ACK_SENT_STATE:
			if(fRequestCounter <= 0)
				TOBadEvent();
			else
				TOGoodEvent();
		break;
		
		default:
			;
	}
}


bool
IPCP::ParseSideRequests(const driver_parameter *requests, ppp_side side)
{
	if(!requests)
		return false;
	
	ipcp_requests *selectedRequests;
	
	if(side == PPP_LOCAL_SIDE) {
		selectedRequests = &fLocalRequests;
		fRequestPrimaryDNS = fRequestSecondaryDNS = false;
	} else
		selectedRequests = &fPeerRequests;
	
	memset(selectedRequests, 0, sizeof(ipcp_requests));
		// reset current requests
	
	// The following values are allowed:
	//  "Address"		the ip address that will be suggested
	//  "Netmask"		the netmask that should be used
	//  "PrimaryDNS"	primary DNS server
	//  "SecondaryDNS"	secondary DNS server
	// Setting any value to 0.0.0.0 or "auto" means it should be chosen automatically.
	
	in_addr_t address = INADDR_ANY;
	for(int32 index = 0; index < requests->parameter_count; index++) {
		if(requests->parameters[index].value_count == 0)
			continue;
		
		// all values are IP addresses, so parse the address here
		if(strcasecmp(requests->parameters[index].values[0], "auto")) {
			address = inet_addr(requests->parameters[index].values[0]);
			if(address == INADDR_NONE)
				continue;
		}
		
		if(!strcasecmp(requests->parameters[index].name, "Address"))
			selectedRequests->address = address;
		else if(!strcasecmp(requests->parameters[index].name, "Netmask"))
			selectedRequests->netmask = address;
		else if(!strcasecmp(requests->parameters[index].name, "PrimaryDNS")) {
			selectedRequests->primaryDNS = address;
			if(side == PPP_LOCAL_SIDE)
				fRequestPrimaryDNS = true;
		} else if(!strcasecmp(requests->parameters[index].name, "SecondaryDNS")) {
			selectedRequests->secondaryDNS = address;
			if(side == PPP_LOCAL_SIDE)
				fRequestSecondaryDNS = true;
		}
	}
	
	return true;
}


void
IPCP::UpdateAddresses()
{
	RemoveRoutes();
	
	if(State() != PPP_OPENED_STATE && !Interface().DoesDialOnDemand())
		return;
	
	struct in_aliasreq inreq;
	struct ifreq ifreqAddress, ifreqDestination;
	memset(&inreq, 0, sizeof(struct in_aliasreq));
	memset(&ifreqAddress, 0, sizeof(struct ifreq));
	memset(&ifreqDestination, 0, sizeof(struct ifreq));
	memset(&fGateway, 0, sizeof(struct sockaddr_in));
	
	// create local address
	inreq.ifra_addr.sin_family = AF_INET;
	if(fLocalRequests.address != INADDR_ANY)
		inreq.ifra_addr.sin_addr.s_addr = fLocalRequests.address;
	else if(fLocalConfiguration.address == INADDR_ANY)
		inreq.ifra_addr.sin_addr.s_addr = INADDR_BROADCAST;
	else
		inreq.ifra_addr.sin_addr.s_addr = fLocalConfiguration.address;
	inreq.ifra_addr.sin_len = sizeof(struct sockaddr_in);
	memcpy(&ifreqAddress.ifr_addr, &inreq.ifra_addr, sizeof(struct sockaddr_in));
	
	// create destination address
	fGateway.sin_family = AF_INET;
	if(fPeerRequests.address != INADDR_ANY)
		fGateway.sin_addr.s_addr = fPeerRequests.address;
	else if(fPeerConfiguration.address == INADDR_ANY)
		fGateway.sin_addr.s_addr = INADDR_BROADCAST;
	else
		fGateway.sin_addr.s_addr = fPeerConfiguration.address;
	fGateway.sin_len = sizeof(struct sockaddr_in);
	memcpy(&inreq.ifra_dstaddr, &fGateway,
		sizeof(struct sockaddr_in));
	memcpy(&ifreqDestination.ifr_dstaddr, &inreq.ifra_dstaddr,
		sizeof(struct sockaddr_in));
	
	// create netmask
	inreq.ifra_mask.sin_family = AF_INET;
	inreq.ifra_mask.sin_addr.s_addr = fLocalRequests.netmask;
	inreq.ifra_mask.sin_len = sizeof(struct sockaddr_in);
	
	// tell stack to use these values
	if(in_control(NULL, SIOCAIFADDR, (caddr_t) &inreq,
			Interface().Ifnet()) != B_OK)
		dprintf("IPCP: UpdateAddress(): SIOCAIFADDR returned error!\n");
	if(in_control(NULL, SIOCSIFADDR, (caddr_t) &ifreqAddress,
			Interface().Ifnet()) != B_OK)
		dprintf("IPCP: UpdateAddress(): SIOCSIFADDR returned error!\n");
	if(in_control(NULL, SIOCSIFDSTADDR, (caddr_t) &ifreqDestination,
			Interface().Ifnet()) != B_OK)
		dprintf("IPCP: UpdateAddress(): SIOCSIFDSTADDR returned error!\n");
	memcpy(&inreq.ifra_addr, &inreq.ifra_mask, sizeof(struct sockaddr_in));
		// SIOCISFNETMASK wants the netmask to be in ifra_addr
	if(in_control(NULL, SIOCSIFNETMASK, (caddr_t) &inreq,
			Interface().Ifnet()) != B_OK)
		dprintf("IPCP: UpdateAddress(): SIOCSIFNETMASK returned error!\n");
	
	// add default/subnet route
	if(Side() == PPP_LOCAL_SIDE) {
		if(rtrequest(RTM_ADD, (struct sockaddr*) &inreq.ifra_mask,
				(struct sockaddr*) &fGateway, (struct sockaddr*) &inreq.ifra_mask,
				RTF_UP | RTF_GATEWAY, &fDefaultRoute) != B_OK)
			dprintf("IPCP: UpdateAddress(): could not add default/subnet route!\n");
		
		--fDefaultRoute->rt_refcnt;
	}
}


void
IPCP::RemoveRoutes()
{
	// note: netstack creates and deletes destination route automatically
	
	if(fDefaultRoute) {
		struct sockaddr_in netmask;
		memset(&netmask, 0, sizeof(struct sockaddr_in));
		
		netmask.sin_family = AF_INET;
		netmask.sin_addr.s_addr = fLocalRequests.netmask;
		netmask.sin_len = sizeof(struct sockaddr_in);
		
		if(rtrequest(RTM_DELETE, (struct sockaddr*) &netmask,
				(struct sockaddr*) &fGateway, (struct sockaddr*) &netmask,
				RTF_GATEWAY, &fDefaultRoute) != B_OK)
			dprintf("IPCP: RemoveRoutes(): could not remove default/subnet route!\n");
		
		fDefaultRoute = NULL;
	}
}


uint8
IPCP::NextID()
{
	return (uint8) atomic_add(&fID, 1);
}


void
IPCP::NewState(ppp_state next)
{
#if DEBUG
	dprintf("IPCP: NewState(%d) state=%d\n", next, State());
#endif
	
	// report state changes
	if(State() == PPP_INITIAL_STATE && next != State())
		UpStarted();
	else if(State() == PPP_OPENED_STATE && next != State())
		DownStarted();
	
	// maybe we do not need the timer anymore
	if(next < PPP_CLOSING_STATE || next == PPP_OPENED_STATE)
		fNextTimeout = 0;
	
	fState = next;
}


void
IPCP::TOGoodEvent()
{
#if DEBUG
	dprintf("IPCP: TOGoodEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	switch(State()) {
		case PPP_CLOSING_STATE:
			locker.UnlockNow();
			SendTerminateRequest();
		break;
		
		case PPP_ACK_RCVD_STATE:
			NewState(PPP_REQ_SENT_STATE);
		
		case PPP_REQ_SENT_STATE:
		case PPP_ACK_SENT_STATE:
			locker.UnlockNow();
			SendConfigureRequest();
		break;
		
		default:
			IllegalEvent(PPP_TO_GOOD_EVENT);
	}
}


void
IPCP::TOBadEvent()
{
#if DEBUG
	dprintf("IPCP: TOBadEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	switch(State()) {
		case PPP_CLOSING_STATE:
			NewState(PPP_INITIAL_STATE);
			locker.UnlockNow();
			ReportDownEvent();
		break;
		
		case PPP_REQ_SENT_STATE:
		case PPP_ACK_RCVD_STATE:
		case PPP_ACK_SENT_STATE:
			NewState(PPP_INITIAL_STATE);
			locker.UnlockNow();
			ReportUpFailedEvent();
		break;
		
		default:
			IllegalEvent(PPP_TO_BAD_EVENT);
	}
}


void
IPCP::RCREvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RCREvent() state=%d\n", State());
#endif
	
	KPPPConfigurePacket request(packet);
	KPPPConfigurePacket nak(PPP_CONFIGURE_NAK);
	KPPPConfigurePacket reject(PPP_CONFIGURE_REJECT);
	
	// we should not use the same id as the peer
	if(fID == mtod(packet, ppp_lcp_packet*)->id)
		fID -= 128;
	
	nak.SetID(request.ID());
	reject.SetID(request.ID());
	
	// parse each item
	ppp_configure_item *item;
	in_addr_t *requestedAddress, *wishedAddress = NULL;
	for(int32 index = 0; index < request.CountItems(); index++) {
		item = request.ItemAt(index);
		if(!item)
			continue;
		
		// addresses have special handling to reduce code size
		switch(item->type) {
			case IPCP_ADDRESSES:
				// abandoned by the standard
			case IPCP_ADDRESS:
				wishedAddress = &fPeerRequests.address;
			break;
			
			case IPCP_PRIMARY_DNS:
				wishedAddress = &fPeerRequests.primaryDNS;
			break;
			
			case IPCP_SECONDARY_DNS:
				wishedAddress = &fPeerRequests.secondaryDNS;
			break;
		}
		
		// now parse item
		switch(item->type) {
			case IPCP_ADDRESSES:
				// abandoned by the standard
			case IPCP_ADDRESS:
			case IPCP_PRIMARY_DNS:
			case IPCP_SECONDARY_DNS:
				if(item->length != 6) {
					// the packet is invalid
					m_freem(packet);
					NewState(PPP_INITIAL_STATE);
					ReportUpFailedEvent();
					return;
				}
				
				requestedAddress = (in_addr_t*) item->data;
				if(*wishedAddress == INADDR_ANY) {
					if(*requestedAddress == INADDR_ANY) {
						// we do not have an address for you
						m_freem(packet);
						NewState(PPP_INITIAL_STATE);
						ReportUpFailedEvent();
						return;
					}
				} else if(*requestedAddress != *wishedAddress) {
					// we do not want this address
					ip_item ipItem;
					ipItem.type = item->type;
					ipItem.length = 6;
					ipItem.address = *wishedAddress;
					nak.AddItem((ppp_configure_item*) &ipItem);
				}
			break;
			
//			case IPCP_COMPRESSION_PROTOCOL:
				// TODO: implement me!
//			break;
			
			default:
				reject.AddItem(item);
		}
	}
	
	// append additional values to the nak
	if(!request.ItemWithType(IPCP_ADDRESS) && fPeerRequests.address == INADDR_ANY) {
		// The peer did not provide us his address. Tell him to do so.
		ip_item ipItem;
		ipItem.type = IPCP_ADDRESS;
		ipItem.length = 6;
		ipItem.address = INADDR_ANY;
		nak.AddItem((ppp_configure_item*) &ipItem);
	}
	
	if(nak.CountItems() > 0) {
		RCRBadEvent(nak.ToMbuf(Interface().MRU(), Interface().PacketOverhead()), NULL);
		m_freem(packet);
	} else if(reject.CountItems() > 0) {
		RCRBadEvent(NULL, reject.ToMbuf(Interface().MRU(),
			Interface().PacketOverhead()));
		m_freem(packet);
	} else
		RCRGoodEvent(packet);
}


void
IPCP::RCRGoodEvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RCRGoodEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	switch(State()) {
		case PPP_INITIAL_STATE:
			NewState(PPP_ACK_SENT_STATE);
			InitializeRestartCount();
			locker.UnlockNow();
			SendConfigureRequest();
			SendConfigureAck(packet);
		break;
		
		case PPP_REQ_SENT_STATE:
			NewState(PPP_ACK_SENT_STATE);
		
		case PPP_ACK_SENT_STATE:
			locker.UnlockNow();
			SendConfigureAck(packet);
		break;
		
		case PPP_ACK_RCVD_STATE:
			NewState(PPP_OPENED_STATE);
			locker.UnlockNow();
			SendConfigureAck(packet);
			ReportUpEvent();
		break;
		
		case PPP_OPENED_STATE:
			NewState(PPP_ACK_SENT_STATE);
			locker.UnlockNow();
			SendConfigureRequest();
			SendConfigureAck(packet);
		break;
		
		default:
			m_freem(packet);
	}
}


void
IPCP::RCRBadEvent(struct mbuf *nak, struct mbuf *reject)
{
#if DEBUG
	dprintf("IPCP: RCRBadEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	switch(State()) {
		case PPP_OPENED_STATE:
			NewState(PPP_REQ_SENT_STATE);
			locker.UnlockNow();
			SendConfigureRequest();
		
		case PPP_ACK_SENT_STATE:
			if(State() == PPP_ACK_SENT_STATE)
				NewState(PPP_REQ_SENT_STATE);
					// OPENED_STATE might have set this already
		
		case PPP_INITIAL_STATE:
		case PPP_REQ_SENT_STATE:
		case PPP_ACK_RCVD_STATE:
			locker.UnlockNow();
			if(nak && ntohs(mtod(nak, ppp_lcp_packet*)->length) > 3)
				SendConfigureNak(nak);
			else if(reject && ntohs(mtod(reject, ppp_lcp_packet*)->length) > 3)
				SendConfigureNak(reject);
		return;
			// prevents the nak/reject from being m_freem()'d
		
		default:
			;
	}
	
	if(nak)
		m_freem(nak);
	if(reject)
		m_freem(reject);
}


void
IPCP::RCAEvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RCAEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	if(fRequestID != mtod(packet, ppp_lcp_packet*)->id) {
		// this packet is not a reply to our request
		
		// TODO: log this event
		m_freem(packet);
		return;
	}
	
	// parse this ack
	KPPPConfigurePacket ack(packet);
	ppp_configure_item *item;
	in_addr_t *requestedAddress, *wishedAddress = NULL, *configuredAddress = NULL;
	for(int32 index = 0; index < ack.CountItems(); index++) {
		item = ack.ItemAt(index);
		if(!item)
			continue;
		
		// addresses have special handling to reduce code size
		switch(item->type) {
			case IPCP_ADDRESSES:
				// abandoned by the standard
			case IPCP_ADDRESS:
				wishedAddress = &fLocalRequests.address;
				configuredAddress = &fLocalConfiguration.address;
			break;
			
			case IPCP_PRIMARY_DNS:
				wishedAddress = &fLocalRequests.primaryDNS;
				configuredAddress = &fLocalConfiguration.primaryDNS;
			break;
			
			case IPCP_SECONDARY_DNS:
				wishedAddress = &fLocalRequests.secondaryDNS;
				configuredAddress = &fLocalConfiguration.secondaryDNS;
			break;
		}
		
		// now parse item
		switch(item->type) {
			case IPCP_ADDRESSES:
				// abandoned by the standard
			case IPCP_ADDRESS:
			case IPCP_PRIMARY_DNS:
			case IPCP_SECONDARY_DNS:
				requestedAddress = (in_addr_t*) item->data;
				if((*wishedAddress == INADDR_ANY && *requestedAddress != INADDR_ANY)
						|| *wishedAddress == *requestedAddress)
					*configuredAddress = *requestedAddress;
			break;
			
//			case IPCP_COMPRESSION_PROTOCOL:
				// TODO: implement me
//			break;
			
			default:
				;
		}
	}
	
	// if address was not specified we should select the given one
	if(!ack.ItemWithType(IPCP_ADDRESS))
		fLocalConfiguration.address = fLocalRequests.address;
	
	
	switch(State()) {
		case PPP_INITIAL_STATE:
			IllegalEvent(PPP_RCA_EVENT);
		break;
		
		case PPP_REQ_SENT_STATE:
			NewState(PPP_ACK_RCVD_STATE);
			InitializeRestartCount();
		break;
		
		case PPP_ACK_RCVD_STATE:
			NewState(PPP_REQ_SENT_STATE);
			locker.UnlockNow();
			SendConfigureRequest();
		break;
		
		case PPP_ACK_SENT_STATE:
			NewState(PPP_OPENED_STATE);
			InitializeRestartCount();
			locker.UnlockNow();
			ReportUpEvent();
		break;
		
		case PPP_OPENED_STATE:
			NewState(PPP_REQ_SENT_STATE);
			locker.UnlockNow();
			SendConfigureRequest();
		break;
		
		default:
			;
	}
	
	m_freem(packet);
}


void
IPCP::RCNEvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RCNEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	if(fRequestID != mtod(packet, ppp_lcp_packet*)->id) {
		// this packet is not a reply to our request
		
		// TODO: log this event
		m_freem(packet);
		return;
	}
	
	// parse this nak/reject
	KPPPConfigurePacket nak_reject(packet);
	ppp_configure_item *item;
	in_addr_t *requestedAddress;
	if(nak_reject.Code() == PPP_CONFIGURE_NAK)
		for(int32 index = 0; index < nak_reject.CountItems(); index++) {
			item = nak_reject.ItemAt(index);
			if(!item)
				continue;
			
			switch(item->type) {
				case IPCP_ADDRESSES:
					// abandoned by the standard
				case IPCP_ADDRESS:
					if(item->length != 6)
						continue;
					
					requestedAddress = (in_addr_t*) item->data;
					if(fLocalRequests.address == INADDR_ANY
							&& *requestedAddress != INADDR_ANY)
						fLocalConfiguration.address = *requestedAddress;
							// this will be used in our next request
				break;
				
//				case IPCP_COMPRESSION_PROTOCOL:
					// TODO: implement me!
//				break;
				
				case IPCP_PRIMARY_DNS:
					if(item->length != 6)
						continue;
					
					requestedAddress = (in_addr_t*) item->data;
					if(fRequestPrimaryDNS
							&& fLocalRequests.primaryDNS == INADDR_ANY
							&& *requestedAddress != INADDR_ANY)
						fLocalConfiguration.primaryDNS = *requestedAddress;
							// this will be used in our next request
				break;
				
				case IPCP_SECONDARY_DNS:
					if(item->length != 6)
						continue;
					
					requestedAddress = (in_addr_t*) item->data;
					if(fRequestSecondaryDNS
							&& fLocalRequests.secondaryDNS == INADDR_ANY
							&& *requestedAddress != INADDR_ANY)
						fLocalConfiguration.secondaryDNS = *requestedAddress;
							// this will be used in our next request
				break;
				
				default:
					;
			}
		}
	else if(nak_reject.Code() == PPP_CONFIGURE_REJECT)
		for(int32 index = 0; index < nak_reject.CountItems(); index++) {
			item = nak_reject.ItemAt(index);
			if(!item)
				continue;
			
			switch(item->type) {
//				case IPCP_COMPRESSION_PROTOCOL:
					// TODO: implement me!
//				break;
				
				default:
					// DNS and addresses must be supported if we set them to auto
					m_freem(packet);
					NewState(PPP_INITIAL_STATE);
					ReportUpFailedEvent();
					return;
			}
		}
	
	switch(State()) {
		case PPP_INITIAL_STATE:
			IllegalEvent(PPP_RCN_EVENT);
		break;
		
		case PPP_REQ_SENT_STATE:
		case PPP_ACK_SENT_STATE:
			InitializeRestartCount();
		
		case PPP_ACK_RCVD_STATE:
		case PPP_OPENED_STATE:
			if(State() == PPP_ACK_RCVD_STATE || State() == PPP_OPENED_STATE)
				NewState(PPP_REQ_SENT_STATE);
			locker.UnlockNow();
			SendConfigureRequest();
		break;
		
		default:
			;
	}
	
	m_freem(packet);
}


void
IPCP::RTREvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RTREvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	// we should not use the same ID as the peer
	if(fID == mtod(packet, ppp_lcp_packet*)->id)
		fID -= 128;
	
	switch(State()) {
		case PPP_INITIAL_STATE:
			IllegalEvent(PPP_RTR_EVENT);
		break;
		
		case PPP_ACK_RCVD_STATE:
		case PPP_ACK_SENT_STATE:
			NewState(PPP_REQ_SENT_STATE);
		
		case PPP_CLOSING_STATE:
		case PPP_REQ_SENT_STATE:
			locker.UnlockNow();
			SendTerminateAck(packet);
		return;
			// do not m_freem() packet
		
		case PPP_OPENED_STATE:
			NewState(PPP_CLOSING_STATE);
			ZeroRestartCount();
			locker.UnlockNow();
			SendTerminateAck(packet);
		return;
			// do not m_freem() packet
		
		default:
			;
	}
	
	m_freem(packet);
}


void
IPCP::RTAEvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RTAEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	if(fTerminateID != mtod(packet, ppp_lcp_packet*)->id) {
		// this packet is not a reply to our request
		
		// TODO: log this event
		m_freem(packet);
		return;
	}
	
	switch(State()) {
		case PPP_INITIAL_STATE:
			IllegalEvent(PPP_RTA_EVENT);
		break;
		
		case PPP_CLOSING_STATE:
			NewState(PPP_INITIAL_STATE);
			locker.UnlockNow();
			ReportDownEvent();
		break;
		
		case PPP_ACK_RCVD_STATE:
			NewState(PPP_REQ_SENT_STATE);
		break;
		
		case PPP_OPENED_STATE:
			NewState(PPP_REQ_SENT_STATE);
			locker.UnlockNow();
			SendConfigureRequest();
		break;
		
		default:
			;
	}
	
	m_freem(packet);
}


void
IPCP::RUCEvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RUCEvent() state=%d\n", State());
#endif
	
	SendCodeReject(packet);
}


void
IPCP::RXJBadEvent(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: RXJBadEvent() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	
	switch(State()) {
		case PPP_INITIAL_STATE:
			IllegalEvent(PPP_RXJ_BAD_EVENT);
		break;
		
		case PPP_CLOSING_STATE:
			NewState(PPP_INITIAL_STATE);
			locker.UnlockNow();
			ReportDownEvent();
		break;
		
		case PPP_REQ_SENT_STATE:
		case PPP_ACK_RCVD_STATE:
		case PPP_ACK_SENT_STATE:
			NewState(PPP_INITIAL_STATE);
			locker.UnlockNow();
			ReportUpFailedEvent();
		break;
		
		case PPP_OPENED_STATE:
			NewState(PPP_CLOSING_STATE);
			InitializeRestartCount();
			locker.UnlockNow();
			SendTerminateRequest();
		break;
		
		default:
			;
	}
	
	m_freem(packet);
}


// actions
void
IPCP::IllegalEvent(ppp_event event)
{
	// TODO: update error statistics
	dprintf("IPCP: IllegalEvent(event=%d) state=%d\n", event, State());
}


void
IPCP::ReportUpFailedEvent()
{
	// reset configurations
	memset(&fLocalConfiguration, 0, sizeof(ipcp_configuration));
	memset(&fPeerConfiguration, 0, sizeof(ipcp_configuration));
	
	UpdateAddresses();
	
	UpFailedEvent();
}


void
IPCP::ReportUpEvent()
{
	UpdateAddresses();
	
	UpEvent();
}


void
IPCP::ReportDownEvent()
{
	// reset configurations
	memset(&fLocalConfiguration, 0, sizeof(ipcp_configuration));
	memset(&fPeerConfiguration, 0, sizeof(ipcp_configuration));
	
	UpdateAddresses();
	
	DownEvent();
}


void
IPCP::InitializeRestartCount()
{
	fRequestCounter = fMaxRequest;
	fTerminateCounter = fMaxTerminate;
	fNakCounter = fMaxNak;
}


void
IPCP::ZeroRestartCount()
{
	fRequestCounter = 0;
	fTerminateCounter = 0;
	fNakCounter = 0;
}


bool
IPCP::SendConfigureRequest()
{
#if DEBUG
	dprintf("IPCP: SendConfigureRequest() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	--fRequestCounter;
	fNextTimeout = system_time() + PPP_STATE_MACHINE_TIMEOUT;
	locker.UnlockNow();
	
	KPPPConfigurePacket request(PPP_CONFIGURE_REQUEST);
	request.SetID(NextID());
	fRequestID = request.ID();
	ip_item ipItem;
	ipItem.length = 6;
	
	// add address
	ipItem.type = IPCP_ADDRESS;
	if(fLocalRequests.address == INADDR_ANY)
		ipItem.address = fLocalConfiguration.address;
	else
		ipItem.address = fLocalRequests.address;
	request.AddItem((ppp_configure_item*) &ipItem);
	
#if DEBUG
	dprintf("IPCP: SCR: confaddr=%lX; reqaddr=%lX; addr=%lX\n",
		fLocalConfiguration.address, fLocalRequests.address,
		((ip_item*)request.ItemAt(0))->address);
#endif
	
	// add primary DNS (if needed)
	if(fRequestPrimaryDNS && fLocalRequests.primaryDNS == INADDR_ANY) {
		ipItem.type = IPCP_PRIMARY_DNS;
		ipItem.address = fLocalConfiguration.primaryDNS;
			// at first this is 0.0.0.0, but a nak might have set it to a correct value
		request.AddItem((ppp_configure_item*) &ipItem);
	}
	
	// add secondary DNS (if needed)
	if(fRequestSecondaryDNS && fLocalRequests.primaryDNS == INADDR_ANY) {
		ipItem.type = IPCP_SECONDARY_DNS;
		ipItem.address = fLocalConfiguration.secondaryDNS;
			// at first this is 0.0.0.0, but a nak might have set it to a correct value
		request.AddItem((ppp_configure_item*) &ipItem);
	}
	
	// TODO: add VJC support
	
	return Send(request.ToMbuf(Interface().MRU(),
		Interface().PacketOverhead())) == B_OK;
}


bool
IPCP::SendConfigureAck(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: SendConfigureAck() state=%d\n", State());
#endif
	
	if(!packet)
		return false;
	
	mtod(packet, ppp_lcp_packet*)->code = PPP_CONFIGURE_ACK;
	KPPPConfigurePacket ack(packet);
	
	// verify items
	ppp_configure_item *item;
	in_addr_t *requestedAddress, *wishedAddress = NULL, *configuredAddress = NULL;
	for(int32 index = 0; index < ack.CountItems(); index++) {
		item = ack.ItemAt(index);
		if(!item)
			continue;
		
		// addresses have special handling to reduce code size
		switch(item->type) {
			case IPCP_ADDRESSES:
				// abandoned by the standard
			case IPCP_ADDRESS:
				wishedAddress = &fPeerRequests.address;
				configuredAddress = &fPeerConfiguration.address;
			break;
			
			case IPCP_PRIMARY_DNS:
				wishedAddress = &fPeerRequests.primaryDNS;
				configuredAddress = &fPeerConfiguration.primaryDNS;
			break;
			
			case IPCP_SECONDARY_DNS:
				wishedAddress = &fPeerRequests.secondaryDNS;
				configuredAddress = &fPeerConfiguration.secondaryDNS;
			break;
		}
		
		// now parse item
		switch(item->type) {
			case IPCP_ADDRESSES:
				// abandoned by the standard
			case IPCP_ADDRESS:
			case IPCP_PRIMARY_DNS:
			case IPCP_SECONDARY_DNS:
				requestedAddress = (in_addr_t*) item->data;
				if((*wishedAddress == INADDR_ANY && *requestedAddress != INADDR_ANY)
						|| *wishedAddress == *requestedAddress)
					*configuredAddress = *requestedAddress;
			break;
			
//			case IPCP_COMPRESSION_PROTOCOL:
				// TODO: implement me!
//			break;
			
			default:
				;
		}
	}
	
	// if address was not specified we should select the given one
	if(!ack.ItemWithType(IPCP_ADDRESS))
		fPeerConfiguration.address = fPeerRequests.address;
	
	return Send(packet) == B_OK;
}


bool
IPCP::SendConfigureNak(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: SendConfigureNak() state=%d\n", State());
#endif
	
	if(!packet)
		return false;
	
	ppp_lcp_packet *nak = mtod(packet, ppp_lcp_packet*);
	if(nak->code == PPP_CONFIGURE_NAK) {
		if(fNakCounter == 0) {
			// We sent enough naks. Let's try a reject.
			nak->code = PPP_CONFIGURE_REJECT;
		} else
			--fNakCounter;
	}
	
	return Send(packet) == B_OK;
}


bool
IPCP::SendTerminateRequest()
{
#if DEBUG
	dprintf("IPCP: SendTerminateRequest() state=%d\n", State());
#endif
	
	LockerHelper locker(fLock);
	--fTerminateCounter;
	fNextTimeout = system_time() + PPP_STATE_MACHINE_TIMEOUT;
	locker.UnlockNow();
	
	struct mbuf *packet = m_gethdr(MT_DATA);
	if(!packet)
		return false;
	
	packet->m_pkthdr.len = packet->m_len = 4;
	
	// reserve some space for other protocols
	packet->m_data += Interface().PacketOverhead();
	
	ppp_lcp_packet *request = mtod(packet, ppp_lcp_packet*);
	request->code = PPP_TERMINATE_REQUEST;
	request->id = fTerminateID = NextID();
	request->length = htons(4);
	
	return Send(packet) == B_OK;
}


bool
IPCP::SendTerminateAck(struct mbuf *request = NULL)
{
#if DEBUG
	dprintf("IPCP: SendTerminateAck() state=%d\n", State());
#endif
	
	struct mbuf *reply = request;
	
	ppp_lcp_packet *ack;
	
	if(!reply) {
		reply = m_gethdr(MT_DATA);
		if(!reply)
			return false;
		
		reply->m_data += Interface().PacketOverhead();
		reply->m_pkthdr.len = reply->m_len = 4;
		
		ack = mtod(reply, ppp_lcp_packet*);
		ack->id = NextID();
	} else
		ack = mtod(reply, ppp_lcp_packet*);
	
	ack->code = PPP_TERMINATE_ACK;
	ack->length = htons(4);
	
	return Send(reply) == B_OK;
}


bool
IPCP::SendCodeReject(struct mbuf *packet)
{
#if DEBUG
	dprintf("IPCP: SendCodeReject() state=%d\n", State());
#endif
	
	if(!packet)
		return false;
	
	M_PREPEND(packet, 4);
		// add some space for the header
	
	// adjust packet if too big
	int32 adjust = Interface().MRU();
	if(packet->m_flags & M_PKTHDR) {
		adjust -= packet->m_pkthdr.len;
	} else
		adjust -= packet->m_len;
	
	if(adjust < 0)
		m_adj(packet, adjust);
	
	ppp_lcp_packet *reject = mtod(packet, ppp_lcp_packet*);
	reject->code = PPP_CODE_REJECT;
	reject->id = NextID();
	if(packet->m_flags & M_PKTHDR)
		reject->length = htons(packet->m_pkthdr.len);
	else
		reject->length = htons(packet->m_len);
	
	return Send(packet) == B_OK;
}
