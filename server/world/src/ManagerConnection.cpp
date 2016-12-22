/**
 * @file server/world/src/ManagerConnection.cpp
 * @ingroup world
 *
 * @author HACKfrost
 *
 * @brief Manager to handle world connections to lobby and channel servers.
 *
 * This file is part of the World Server (world).
 *
 * Copyright (C) 2012-2016 COMP_hack Team <compomega@tutanota.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ManagerConnection.h"

// libcomp Includes
#include <Log.h>
#include <MessageConnectionClosed.h>
#include <MessageEncrypted.h>
#include <PacketCodes.h>

// world Includes
#include "WorldServer.h"

using namespace world;

std::list<libcomp::Message::MessageType> ManagerConnection::sSupportedTypes =
    { libcomp::Message::MessageType::MESSAGE_TYPE_CONNECTION };

ManagerConnection::ManagerConnection(std::weak_ptr<libcomp::BaseServer> server)
    : mServer(server)
{
}

ManagerConnection::~ManagerConnection()
{
}

std::list<libcomp::Message::MessageType>
ManagerConnection::GetSupportedTypes() const
{
    return sSupportedTypes;
}

bool ManagerConnection::ProcessMessage(const libcomp::Message::Message *pMessage)
{
    const libcomp::Message::ConnectionMessage *cMessage = dynamic_cast<
        const libcomp::Message::ConnectionMessage*>(pMessage);

    switch (cMessage->GetConnectionMessageType())
    {
        case libcomp::Message::ConnectionMessageType::CONNECTION_MESSAGE_ENCRYPTED:
            {
                const libcomp::Message::Encrypted *encrypted = dynamic_cast<
                    const libcomp::Message::Encrypted*>(cMessage);

                if(!LobbyConnected())
                {
                    auto connection = encrypted->GetConnection();

                    /// @todo: verify this is in fact the lobby

                    mLobbyConnection = std::dynamic_pointer_cast<libcomp::InternalConnection>(connection);
                }
                else
                {
                    //Nothing to do upon encrypting a channel connection (for now)
                }
        
                return true;
            }
            break;
        case libcomp::Message::ConnectionMessageType::CONNECTION_MESSAGE_CONNECTION_CLOSED:
            {
                const libcomp::Message::ConnectionClosed *closed = dynamic_cast<
                    const libcomp::Message::ConnectionClosed*>(cMessage);

                auto connection = closed->GetConnection();

                auto server = mServer.lock();
                server->RemoveConnection(connection);

                if(mLobbyConnection == connection)
                {
                    LOG_INFO(libcomp::String("Lobby connection closed. Shutting down."));
                    server->Shutdown();
                }
                else
                {
                    auto iConnection = std::dynamic_pointer_cast<libcomp::InternalConnection>(connection);
                    RemoveConnection(iConnection);
                }

                return true;
            }
            break;
        default:
            break;
    }
    
    return false;
}

std::shared_ptr<libcomp::InternalConnection> ManagerConnection::GetLobbyConnection()
{
    return mLobbyConnection;
}

bool ManagerConnection::LobbyConnected()
{
    return nullptr != mLobbyConnection;
}

void ManagerConnection::RemoveConnection(std::shared_ptr<libcomp::InternalConnection>& connection)
{
    objects::ChannelDescription channelDesc;
    auto server = std::dynamic_pointer_cast<WorldServer>(mServer.lock());
    if(server->GetChannelDescriptionByConnection(connection, channelDesc))
    {
        server->RemoveChannelDescription(connection);

        //Channel disconnected
        libcomp::Packet packet;
        packet.WritePacketCode(
            InternalPacketCode_t::PACKET_SET_CHANNEL_DESCRIPTION);
        packet.WriteU8(to_underlying(
            InternalPacketAction_t::PACKET_ACTION_REMOVE));
        channelDesc.SavePacket(packet);
        mLobbyConnection->SendPacket(packet);
    }
}