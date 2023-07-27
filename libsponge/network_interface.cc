#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    uint32_t ip = next_hop.ipv4_numeric();
    auto p = arpmap.find(ip);
    if (p == arpmap.end()) {
        auto pp = _waitlist.find(ip);
        if (pp == _waitlist.end() || pp->second.empty())
            send_arp_request(ip);
        _waitlist[ip].push_back(dgram);
    } else {
        EthernetFrame ereq;
        ereq.payload() = dgram.serialize();
        ereq.header() = {p->second.first, _ethernet_address, EthernetHeader::TYPE_IPv4};
        _frames_out.push(ereq);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const EthernetHeader header = frame.header();
    if (header.dst != _ethernet_address && header.dst != ETHERNET_BROADCAST)
        return std::nullopt;
    if (header.type == header.TYPE_IPv4) {
        InternetDatagram datagram;
        datagram.parse(frame.payload());
        return datagram;
    } else if (header.type == header.TYPE_ARP) {
        ARPMessage arp;
        arp.parse(frame.payload());
        if (arp.opcode == arp.OPCODE_REPLY) {
            EthernetAddress eaddr = arp.sender_ethernet_address;
            uint32_t ipaddr = arp.sender_ip_address;
            arpmap[ipaddr] = {eaddr, _initial_max_timeout};
            vector<InternetDatagram> &v = _waitlist[ipaddr];
            for (auto &p : v) {
                EthernetFrame eIPV4;
                eIPV4.payload() = p.serialize();
                eIPV4.header() = {eaddr, _ethernet_address, header.TYPE_IPv4};
                _frames_out.push(eIPV4);
            }
            v.clear();
            reqmap.erase(ipaddr);

        } else if (arp.opcode == arp.OPCODE_REQUEST) {
            EthernetAddress eaddr = arp.sender_ethernet_address;
            uint32_t ipaddr = arp.sender_ip_address;
            arpmap[ipaddr] = {eaddr, _initial_max_timeout};

            if (arp.target_ip_address != _ip_address.ipv4_numeric())
                return std::nullopt;
            ARPMessage reply;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ethernet_address = eaddr;
            reply.target_ip_address = ipaddr;
            reply.opcode = arp.OPCODE_REPLY;

            EthernetFrame ereply;
            ereply.payload() = reply.serialize();
            ereply.header() = {header.src, _ethernet_address, header.TYPE_ARP};
            _frames_out.push(ereply);
        }
    }
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto p = reqmap.begin(); p != reqmap.end(); p++) {
        if (p->second <= ms_since_last_tick) {
            send_arp_request(p->first);
            p->second = ms_since_last_tick;
        } else
            p->second -= ms_since_last_tick;
    }

    for (auto p = arpmap.begin(); p != arpmap.end();) {
        if (p->second.second <= ms_since_last_tick) {
            p = arpmap.erase(p);
        } else {
            p->second.second -= ms_since_last_tick;
            p++;
        }
    }
}

void NetworkInterface::send_arp_request(uint32_t target_ip) {
    ARPMessage req;
    req.sender_ethernet_address = _ethernet_address;
    req.sender_ip_address = _ip_address.ipv4_numeric();
    req.target_ip_address = target_ip;
    req.opcode = ARPMessage::OPCODE_REQUEST;

    EthernetFrame ereq;
    ereq.payload() = req.serialize();
    ereq.header() = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
    _frames_out.push(ereq);
    reqmap[target_ip] = _initial_max_reqtimeout;
}
