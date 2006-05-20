/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World  is free software; you can redistribute  it and/or modify it
 *  under the terms of the GNU General  Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or any later version.
 *
 *  The Mana  World is  distributed in  the hope  that it  will be  useful, but
 *  WITHOUT ANY WARRANTY; without even  the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should  have received a  copy of the  GNU General Public  License along
 *  with The Mana  World; if not, write to the  Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  $Id$
 */

#include "connectionhandler.h"

#include "chatchannelmanager.h"
#include "messagehandler.h"
#include "messagein.h"
#include "messageout.h"
#include "netcomputer.h"
#include "packet.h"

#include "utils/logger.h"

#ifdef SCRIPT_SUPPORT
#include "script.h"
#endif

/**
 * Convert a IP4 address into its string representation
 */
std::string
ip4ToString(unsigned int ip4addr)
{
    std::stringstream ss;
    ss << (ip4addr & 0x000000ff) << "."
       << ((ip4addr & 0x0000ff00) >> 8) << "."
       << ((ip4addr & 0x00ff0000) >> 16) << "."
       << ((ip4addr & 0xff000000) >> 24);
    return ss.str();
}

////////////////

ClientData::ClientData():
    inp(0)
{
}

bool
ConnectionHandler::startListen(enet_uint16 port)
{
    // Bind the server to the default localhost.
    address.host = ENET_HOST_ANY;
    address.port = port;

    LOG_INFO("Listening on port " << port << "...", 0);
    host = enet_host_create(&address    /* the address to bind the server host to */,
                            MAX_CLIENTS /* allow up to MAX_CLIENTS clients and/or outgoing connections */,
                            0           /* assume any amount of incoming bandwidth */,
                            0           /* assume any amount of outgoing bandwidth */);

    return host;
}

void
ConnectionHandler::stopListen()
{
    // - Disconnect all clients (close sockets)

    // TODO: probably there's a better way.
    ENetPeer *currentPeer;

    for (currentPeer = host->peers;
         currentPeer < &host->peers[host->peerCount];
         ++currentPeer)
    {
       if (currentPeer->state == ENET_PEER_STATE_CONNECTED)
       {
            enet_peer_disconnect(currentPeer, 0);
            enet_host_flush(host);
            enet_peer_reset(currentPeer);
       }
    }
    enet_host_destroy(host);
}

void
ConnectionHandler::process()
{
    ENetEvent event;
    // Process Enet events and do not block.
    while (enet_host_service(host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
            {
                LOG_INFO("A new client connected from " <<
                         ip4ToString(event.peer->address.host) << ":" <<
                         event.peer->address.port, 0);
                NetComputer *comp = computerConnected(event.peer);
                clients.push_back(comp);
                /*LOG_INFO(ltd->host->peerCount <<
                         " client(s) connected", 0);*/

                // Store any relevant client information here.
                event.peer->data = (void *)comp;
            } break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                LOG_INFO("A packet of length " << event.packet->dataLength <<
                         " was received from " << event.peer->address.host, 2);

                NetComputer *comp = (NetComputer *)event.peer->data;

#ifdef SCRIPT_SUPPORT
                // This could be good if you wanted to extend the
                // server protocol using a scripting language. This
                // could be attained by using allowing scripts to
                // "hook" certain messages.

                //script->message(buffer);
#endif

                // If the scripting subsystem didn't hook the message
                // it will be handled by the default message handler.

                // Convert the client IP address to string
                // representation
                std::string ipaddr = ip4ToString(event.peer->address.host);

                // Make sure that the packet is big enough (> short)
                if (event.packet->dataLength >= 2) {
                    Packet *packet = new Packet((char *)event.packet->data,
                                                event.packet->dataLength);
                    MessageIn msg(packet); // (MessageIn frees packet)

                    short messageId = msg.getId();

                    HandlerMap::iterator it = handlers.find(messageId);
                    if (it != handlers.end()) {
                        // send message to appropriate handler
                        it->second->receiveMessage(*comp, msg);
                    } else {
                        // bad message (no registered handler)
                        LOG_ERROR("Unhandled message (" << messageId
                                  << ") received from " << ipaddr, 0);
                    }
                } else {
                    LOG_ERROR("Message too short from " << ipaddr, 0);
                }

                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
            } break;

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                NetComputer *comp = (NetComputer *)event.peer->data;
                /*LOG_INFO(event.peer->address.host
                         << " disconected.", 0);*/
                // Reset the peer's client information.
                computerDisconnected(comp);
                clients.erase(std::find(clients.begin(), clients.end(), comp));
                event.peer->data = NULL;
            } break;

            default: break;
        }
    }
}

void ConnectionHandler::registerHandler(
        unsigned int msgId, MessageHandler *handler)
{
    handlers[msgId] = handler;
}

void ClientConnectionHandler::sendTo(tmwserv::BeingPtr beingPtr, MessageOut &msg)
{
    for (NetComputers::iterator i = clients.begin(), i_end = clients.end();
         i != i_end; ++i) {
        if (static_cast< ClientComputer * >(*i)->getCharacter().get() == beingPtr.get()) {
            (*i)->send(msg.getPacket());
            break;
        }
    }
}

void ClientConnectionHandler::sendTo(std::string name, MessageOut &msg)
{
    for (NetComputers::iterator i = clients.begin(), i_end = clients.end();
         i != i_end; ++i) {
        if (static_cast< ClientComputer * >(*i)->getCharacter()->getName() == name) {
            (*i)->send(msg.getPacket());
            break;
        }
    }
}

void ConnectionHandler::sendToEveryone(MessageOut &msg)
{
    for (NetComputers::iterator i = clients.begin(), i_end = clients.end();
         i != i_end; ++i) {
        (*i)->send(msg.getPacket());
    }
}

void ClientConnectionHandler::sendAround(tmwserv::BeingPtr beingPtr, MessageOut &msg)
{
    unsigned speakerMapId = beingPtr->getMapId();
    std::pair<unsigned, unsigned> speakerXY = beingPtr->getXY();
    for (NetComputers::iterator i = clients.begin(), i_end = clients.end();
         i != i_end; ++i) {
        // See if the other being is near enough, then send the message
        tmwserv::Being const *listener = static_cast< ClientComputer * >(*i)->getCharacter().get();
        if (listener->getMapId() != speakerMapId) continue;
        std::pair<unsigned, unsigned> listenerXY = listener->getXY();
        if (abs(listenerXY.first  - speakerXY.first ) > (int)AROUND_AREA_IN_TILES) continue;
        if (abs(listenerXY.second - speakerXY.second) > (int)AROUND_AREA_IN_TILES) continue;
        (*i)->send(msg.getPacket());
    }
}

void ClientConnectionHandler::sendInChannel(short channelId, MessageOut &msg)
{
    for (NetComputers::iterator i = clients.begin(), i_end = clients.end();
         i != i_end; ++i) {
        const std::vector<tmwserv::BeingPtr> beingList =
            chatChannelManager->getUserListInChannel(channelId);
        // If the being is in the channel, send it
        for (std::vector<tmwserv::BeingPtr>::const_iterator j = beingList.begin(), j_end = beingList.end();
             j != j_end; ++j) {
            if (static_cast< ClientComputer * >(*i)->getCharacter().get() == j->get())
            {
                (*i)->send(msg.getPacket());
            }
        }
    }
}

unsigned int ConnectionHandler::getClientNumber()
{
    return clients.size();
}

void ClientConnectionHandler::makeUsersLeaveChannel(const short channelId)
{
    MessageOut result;
    result.writeShort(SMSG_QUIT_CHANNEL_RESPONSE);
    result.writeByte(CHATCNL_OUT_OK);

    const std::vector<tmwserv::BeingPtr> beingList =
            chatChannelManager->getUserListInChannel(channelId);
    for (NetComputers::iterator i = clients.begin(), i_end = clients.end();
         i != i_end; ++i) {
        // If the being is in the channel, send it the 'leave now' packet
        for (std::vector<tmwserv::BeingPtr>::const_iterator j = beingList.begin(), j_end = beingList.end();
             j != j_end; ++j) {
            if (static_cast< ClientComputer * >(*i)->getCharacter().get() == j->get())
            {
                (*i)->send(result.getPacket());
            }
        }
    }
}

void ClientConnectionHandler::warnUsersAboutPlayerEventInChat(const short channelId,
                                                              const std::string& userName,
                                                              const char eventId)
{
    MessageOut result;
    result.writeShort(SMSG_UPDATE_CHANNEL_RESPONSE);
    result.writeByte(eventId);
    result.writeString(userName);

    const std::vector<tmwserv::BeingPtr> beingList =
            chatChannelManager->getUserListInChannel(channelId);
    for (NetComputers::iterator i = clients.begin(), i_end = clients.end();
         i != i_end; ++i) {
        // If the being is in the channel, send it the 'eventId' packet
        for (std::vector<tmwserv::BeingPtr>::const_iterator j = beingList.begin(), j_end = beingList.end();
             j != j_end; ++j) {
            if (static_cast< ClientComputer * >(*i)->getCharacter().get() == j->get() )
            {
                (*i)->send(result.getPacket());
            }
        }
    }
}

NetComputer *ClientConnectionHandler::computerConnected(ENetPeer *peer) {
    LOG_INFO("A client connected!", 0);
    return new ClientComputer(this, peer);
}

void ClientConnectionHandler::computerDisconnected(NetComputer *comp) {
    delete comp;
    LOG_INFO("A client disconnected!", 0);
}
