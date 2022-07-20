#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    RouteEntry entry = {
        route_prefix,
        prefix_length,
        next_hop,
        interface_num,
    };
    _routing_table.emplace_back(entry);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl <= 1)
        return ;
    dgram.header().ttl--;
    const uint32_t ip_addr_dst = dgram.header().dst;
    auto longest_prefix_match_entry = _routing_table.cend();

    for (auto iter = _routing_table.cbegin(); iter != _routing_table.cend(); iter++) {
        if (is_prefix_match(*iter, ip_addr_dst)
            && (longest_prefix_match_entry == _routing_table.end()
                || iter->prefix_length > longest_prefix_match_entry->prefix_length)) {
            longest_prefix_match_entry = iter;
        }
    }
    if (longest_prefix_match_entry == _routing_table.end()) {
        return ;
    }

    AsyncNetworkInterface &interface = _interfaces[longest_prefix_match_entry->interface_num];
    const optional<Address> next_hop = longest_prefix_match_entry->next_hop;
    if (next_hop.has_value())
        interface.send_datagram(dgram, next_hop.value());
    else
        interface.send_datagram(dgram, Address::from_ipv4_numeric(ip_addr_dst));
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}


bool Router::is_prefix_match(const RouteEntry& entry, const uint32_t &ip) {
    if (entry.prefix_length == 0)
        return true;
    uint8_t offset = 32 - entry.prefix_length;
    uint32_t mask = 0xffffffff << offset;
    return (entry.route_prefix & mask) == (ip & mask);
}

std::string Router::int2ip(uint32_t ip_num) {
    string ip = "";
    for (int i = 3; i >= 0; i--) {
        uint32_t tmp = (ip_num >> (i * 8) ) & 0xff;
        ip += std::to_string(tmp);
        if (i >= 1) {
            ip += ".";
        }
    }
    return ip;
}


//            fprintf( stderr, "\nLINE: %d, Matched, ip_addr_dst: %s iter: %s/%d, next_hop: %s \n",
//                    __LINE__, int2ip(dgram.header().dst).c_str(),
//                    int2ip(iter->route_prefix).c_str(), iter->prefix_length,
//                    iter->next_hop.has_value() ? iter->next_hop.value().ip().c_str() : "");

//    fprintf( stderr, "\nLINE: %d, ip_addr_src: %s, ip_addr_dst: %s, next_hop: %s \n",
//            __LINE__, int2ip(dgram.header().src).c_str(), int2ip(dgram.header().dst).c_str(),
//            longest_prefix_match_entry->next_hop.has_value() ? longest_prefix_match_entry->next_hop.value().ip().c_str() : "");