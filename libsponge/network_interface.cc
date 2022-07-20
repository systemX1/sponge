#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), _curr_time(0) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    const auto &arp_table_iter = _arp_table.find(next_hop_ip);
    // found in arp table, the destination Ethernet address is already known, send the Datagram
    if (arp_table_iter != _arp_table.end()) {
        EthernetFrame frame;
        frame.header() = {
            arp_table_iter->second.eth_addr,
            _ethernet_address,
            EthernetHeader::TYPE_IPv4
        };
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    } else {    // not found, save and send arp
        _unsent_datagrams.emplace_back(next_hop, dgram);
        // avoid repeated ARP request
        if (_no_responded_frames.find(next_hop_ip) == _no_responded_frames.end()) {
            BroadcastArp(next_hop_ip);
        }
    }
}

void NetworkInterface::BroadcastArp(const uint32_t &ip) {
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ethernet_address = _ethernet_address;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
    arp.target_ethernet_address = {};
    arp.target_ip_address = ip;

    EthernetFrame frame;
    frame.header() = {
        ETHERNET_BROADCAST,
        _ethernet_address,
        EthernetHeader::TYPE_ARP
    };
    frame.payload() = arp.serialize();

    _frames_out.push(frame);
    _no_responded_frames[ip] = _curr_time;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetHeader header = frame.header();
    // ignore any frames not destined for the network interface (meaning, the Ethernet
    // destination is either the broadcast address or the interface’s own Ethernet address
    // stored in the ethernet address member variable)
    if (!isequal_ethernetAddress(header.dst, ETHERNET_BROADCAST) && !isequal_ethernetAddress(header.dst, _ethernet_address)) {
        return {};
    }
    switch (header.type) {
        case EthernetHeader::TYPE_IPv4: {
            InternetDatagram datagram;
            ParseResult res = datagram.parse(frame.payload());
            if (res == ParseResult::NoError) {
                return datagram;
            }
            return {};
        }
        case EthernetHeader::TYPE_ARP: {
            ARPMessage arp_recv;
            ParseResult res = arp_recv.parse(frame.payload());
            if (res != ParseResult::NoError) {
                return {};
            }
            EthernetAddress eth_addr = arp_recv.sender_ethernet_address;
            uint32_t ip_addr_src = arp_recv.sender_ip_address;
            uint32_t ip_addr_dst = arp_recv.target_ip_address;
            if ((arp_recv.opcode == ARPMessage::OPCODE_REQUEST) && (ip_addr_dst == _ip_address.ipv4_numeric())) {
                ResponseArpRequest(arp_recv);
            }

            // save arp entry to ARP table
            _arp_table[ip_addr_src] = {eth_addr, _curr_time};
            _no_responded_frames.erase(ip_addr_src);
            for (auto iter = _unsent_datagrams.begin(); iter != _unsent_datagrams.end(); ) {
                if (iter->first.ipv4_numeric() == ip_addr_src) {
                    send_datagram(iter->second, iter->first);
                    iter = _unsent_datagrams.erase(iter);
                } else
                    ++iter;
            }
            return {};
        }
        default:
            return {};
    }
}

void NetworkInterface::ResponseArpRequest(const ARPMessage &arp_msg) {
    ARPMessage arp_msg_send;
    arp_msg_send.opcode = ARPMessage::OPCODE_REPLY;
    arp_msg_send.sender_ethernet_address = _ethernet_address;
    arp_msg_send.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg_send.target_ethernet_address = arp_msg.sender_ethernet_address;
    arp_msg_send.target_ip_address = arp_msg.sender_ip_address;

    EthernetFrame frame;
    frame.header() = {
        arp_msg.sender_ethernet_address,
        _ethernet_address,
        EthernetHeader::TYPE_ARP
    };
    frame.payload() = arp_msg_send.serialize();

    _frames_out.push(frame);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _curr_time += ms_since_last_tick;
    // update ARP Table
    for (auto iter = _arp_table.begin(); iter != _arp_table.end(); ) {
        if (_curr_time - iter->second.time >= ARP_TABLE_ENTRY_TTL) {
            iter = _arp_table.erase(iter);
        } else {
            iter++;
        }
    }
    // update map _no_responded_frames
    for (auto &iter : _no_responded_frames) {
        if (_curr_time - iter.second >= ARP_REQUEST_TIMEOUT) {
            // resent the arp frame
            BroadcastArp(iter.first);
            iter.second = _curr_time;
        }
    }
}



