/**
 * @file deployment_runtime.hpp
 * @brief Host-only compiled deployment probes: complete trees, excluded listeners, unicast versus taps.
 */
#pragma once

#include <wirespaces/core/endpoint_queue.h>
#include <wirespaces/core/host.h>
#include <wirespaces/core/router.h>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace study {
using namespace wirespaces;
WS_PACKET_BUFFER_DEFINE(Packet, 8U);
constexpr EndpointAddress kEndpoint{42U};

/** @brief One isolated simulated image; global host identity is switched only between queued events. */
struct Node {
    HostInfo info;
    Router router;
    EndpointReceiverQueue<8U, 1U> receiver{};
    DispatchTableEntry entry{kEndpoint, &receiver};
    Dispatcher dispatcher{foundation::Span<const DispatchTableEntry>{&entry, 1U}};
    Node(HostInfo host, foundation::Span<const RouteTableEntry> routes, PacketForwarder& forwarder)
        : info{host}, router{routes, forwarder} {}
};

struct Connection {
    Node* node;
    std::string link;
    uint8_t ingress;
};

struct Event {
    Node* node;
    uint8_t ingress;
    Packet packet{};
};

/** @brief Queued physical-Link simulation; deliberately includes physically attached nonparticipants. */
struct Network {
    std::vector<Node*> nodes;
    std::vector<Connection> connections;
    std::deque<Event> events;
    std::map<std::string, unsigned> transmissions;

    void attach(Node& node, const char* link, uint8_t ingress) {
        connections.push_back({&node, link, ingress});
    }

    void transmit(uint8_t sender, const std::string& link, const PacketBuffer& packet) {
        ++transmissions[link];
        for (const auto& connection : connections) {
            if (connection.link == link && connection.node->info.id.value != sender) {
                events.push_back({connection.node, connection.ingress, {}});
                assert(events.back().packet.copyFrom(packet));
                // A receiving SHM adapter must stamp its own ingress even if the pooled/copied
                // buffer carries invalid or unrelated metadata from another host.
                events.back().packet.setIngressIndex(255U);
            }
        }
    }

    /** @brief Broadcast then directed delivery from each member; check exact fan-out and no duplicates. */
    void check(Node& origin, uint8_t wire, std::initializer_list<unsigned> members,
               std::initializer_list<const char*> links) {
        std::vector<uint8_t> destinations{kBroadcastHostValue};
        for (const auto member : members) {
            if (member != origin.info.id.value) { destinations.push_back(static_cast<uint8_t>(member)); break; }
        }
        const std::set<unsigned> member_set{members};
        const std::set<std::string> link_set{links.begin(), links.end()};
        for (const auto destination : destinations) {
            Packet packet{};
            assert(packet.initialize(3U, ControlFields::defaultControlFields()));
            packet.header().wire = WireNumber{wire};
            packet.header().source = origin.info.id;
            packet.header().destination = HostId{destination};
            packet.header().endpoint = kEndpoint;
            std::memcpy(packet.payload().data(), "abc", 3U);
            setLocalHostInfo(origin.info);
            assert(origin.router.forward(packet) == RouteResult::kAccepted);
            unsigned processed{0U};
            while (!events.empty()) {
                assert(++processed <= connections.size());
                Event event{events.front()};
                events.pop_front();
                setLocalHostInfo(event.node->info);
                const auto result{event.node->router.receive(event.packet, event.ingress, event.node->dispatcher)};
                assert(event.packet.ingressIndex() == event.ingress);
                const bool local{member_set.count(event.node->info.id.value) &&
                                 (destination == kBroadcastHostValue || destination == event.node->info.id.value)};
                assert(result.delivery == (local ? DispatchResult::kAccepted : DispatchResult::kNoEndpoint));
            }
            assert(transmissions.size() == link_set.size());
            for (const auto& sent : transmissions) {
                assert(link_set.count(sent.first) && sent.second == 1U);
            }
            for (Node* node : nodes) {
                Packet output{};
                const bool expected{node != &origin && member_set.count(node->info.id.value) &&
                                    (destination == kBroadcastHostValue || destination == node->info.id.value)};
                assert(node->receiver.dequeue(output) == expected);
                if (expected) {
                    assert(output.totalSize() == packet.totalSize());
                    assert(std::memcmp(output.headerAndPayload().data(), packet.headerAndPayload().data(), packet.totalSize()) == 0);
                    assert(output.ingressIndex() >= 1U && output.ingressIndex() <= 8U);
                    assert(!node->receiver.dequeue(output));
                }
            }
            transmissions.clear();
        }
    }
};

/** @brief Adapter whose lifetime and binding order are supplied by generated forwarding classes. */
class Link : public PacketLink {
public:
    Link(Network& network, uint8_t source, const char* link) : network_{network}, source_{source}, link_{link} {}
    LinkAdmission trySend(const PacketBuffer& packet) noexcept override {
        network_.transmit(source_, link_, packet);
        return LinkAdmission::kAccepted;
    }
private:
    Network& network_;
    uint8_t source_;
    std::string link_;
};
}  // namespace study
