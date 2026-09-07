/**
 * @file branched_runtime.cpp
 * @brief Generated tree integration: multi-hop, bus fan-out, ingress rejection and endpoint delivery.
 */
#include "Root.h"
#include "Gateway.h"
#include "LeafA.h"
#include "LeafB.h"
#include "LeafC.h"
#include "Observer.h"

#include <wirespaces/core/endpoint_queue.h>

#include <array>
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>

namespace {
using namespace wirespaces;
WS_PACKET_BUFFER_DEFINE(Packet, 8U);
constexpr EndpointAddress kEndpoint{42U};

/** @brief One sequentially simulated host, with a real Router, Dispatcher and endpoint queue. */
struct Node {
    HostInfo info;
    Router router;
    EndpointReceiverQueue<8U, 2U> receiver{};
    DispatchTableEntry entry{kEndpoint, &receiver};
    Dispatcher dispatcher{foundation::Span<const DispatchTableEntry>{&entry, 1U}};

    Node(HostInfo host, foundation::Span<const RouteTableEntry> routes, PacketForwarder& forwarder)
        : info{host}, router{routes, forwarder} {}

    IngressResult receive(Packet& packet, uint8_t ingress) {
        setLocalHostInfo(info);
        return router.receive(packet, ingress, dispatcher);
    }
};

struct Event {
    Node* destination;
    uint8_t ingress;
    Packet packet{};
};

/** @brief Physical bus, including unselected listeners; adapters enqueue independently owned packets. */
struct Bus {
    std::deque<Event>& events;
    std::vector<std::pair<Node*, uint8_t>> attachments{};
    unsigned transmissions{0U};

    void transmit(HostId source, const PacketBuffer& packet) {
        ++transmissions;
        for (const auto& attachment : attachments) {
            if (attachment.first->info.id != source) {
                events.push_back(Event{attachment.first, attachment.second, {}});
                // Intentionally preserve the previous host's tag like an in-memory Link.
                // The receiving driver must always replace it with its own local index.
                assert(events.back().packet.copyFrom(packet));
            }
        }
    }
};

/** @brief Application-owned Link binding used by the generated forwarding classes. */
class Link final : public PacketLink {
public:
    Link(Bus& bus, HostId source) : bus_{bus}, source_{source} {}
    LinkAdmission trySend(const PacketBuffer& packet) noexcept override {
        bus_.transmit(source_, packet);
        return LinkAdmission::kAccepted;
    }
private:
    Bus& bus_;
    HostId source_;
};

/** @brief Real generated bindings and exact physical connectivity from examples/branched.yaml. */
struct Network {
    std::deque<Event> events{};
    Bus uplink{events}, bus{events}, tail{events}, bypass{events};
    Link root_uplink{uplink, Root::kRootHost}, root_bypass{bypass, Root::kRootHost};
    Link gateway_uplink{uplink, Root::kGatewayHost}, gateway_bus{bus, Root::kGatewayHost};
    Link gateway_tail{tail, Root::kGatewayHost}, gateway_bypass{bypass, Root::kGatewayHost};
    Link a_bus{bus, Root::kLeafAHost}, b_bus{bus, Root::kLeafBHost};
    Link c_tail{tail, Root::kLeafCHost}, observer_bus{bus, Root::kObserverHost};
    Root::Forwarder root_forwarder{root_bypass, root_uplink};
    Gateway::Forwarder gateway_forwarder{gateway_bus, gateway_bypass, gateway_tail, gateway_uplink};
    LeafA::Forwarder a_forwarder{a_bus};
    LeafB::Forwarder b_forwarder{b_bus};
    LeafC::Forwarder c_forwarder{c_tail};
    Observer::Forwarder observer_forwarder{observer_bus};
    Node root{Root::kRootHostInfo, Root::routes(), root_forwarder};
    Node gateway{Gateway::kGatewayHostInfo, Gateway::routes(), gateway_forwarder};
    Node a{LeafA::kLeafAHostInfo, LeafA::routes(), a_forwarder};
    Node b{LeafB::kLeafBHostInfo, LeafB::routes(), b_forwarder};
    Node c{LeafC::kLeafCHostInfo, LeafC::routes(), c_forwarder};
    Node observer{Observer::kObserverHostInfo, Observer::routes(), observer_forwarder};
    std::array<Node*, 6> nodes{&root, &gateway, &a, &b, &c, &observer};

    Network() {
        uplink.attachments = {{&root, Root::kUplinkIngressIndex}, {&gateway, Gateway::kUplinkIngressIndex}};
        bus.attachments = {{&gateway, Gateway::kBusIngressIndex}, {&a, LeafA::kBusIngressIndex},
                           {&b, LeafB::kBusIngressIndex}, {&observer, Observer::kBusIngressIndex}};
        tail.attachments = {{&gateway, Gateway::kTailIngressIndex}, {&c, LeafC::kTailIngressIndex}};
        bypass.attachments = {{&root, Root::kBypassIngressIndex}, {&gateway, Gateway::kBypassIngressIndex}};
    }

    void drain() {
        unsigned processed{0U};
        while (!events.empty()) {
            assert(++processed < 20U);  // Detect reflections or repeated propagation promptly.
            Event event{events.front()};
            events.pop_front();
            const IngressResult result{event.destination->receive(event.packet, event.ingress)};
            assert(event.packet.ingressIndex() == event.ingress);
            if (event.destination == &observer) {
                assert(result.routing == RouteResult::kNoRoute);
                assert(result.delivery == DispatchResult::kNoEndpoint);
            } else {
                assert(result.routing == RouteResult::kAccepted || result.routing == RouteResult::kNoEgress);
                const bool member{event.destination->info.isMemberOf(Root::kTest)};
                const bool addressed{event.packet.header().destination.isBroadcast() ||
                                     event.packet.header().destination == event.destination->info.id};
                assert(result.delivery == (member && addressed ? DispatchResult::kAccepted : DispatchResult::kNoEndpoint));
            }
        }
    }

    // Test each direction; one physical transmission per selected Link, none on Bypass.
    void send(Node& origin, HostId destination) {
        Packet packet{};
        assert(packet.initialize(3U, ControlFields{QoS::kNormal, false, TransportType::kSimple}));
        packet.header().wire = Root::kTest;
        packet.header().source = origin.info.id;
        packet.header().destination = destination;
        packet.header().endpoint = kEndpoint;
        std::memcpy(packet.payload().data(), "abc", 3U);
        setLocalHostInfo(origin.info);
        assert(origin.router.forward(packet) == RouteResult::kAccepted);
        drain();
        assert(uplink.transmissions == 1U && bus.transmissions == 1U && tail.transmissions == 1U);
        assert(bypass.transmissions == 0U);
        for (Node* node : nodes) {
            Packet received{};
            const bool expected{node != &origin && node->info.isMemberOf(Root::kTest) &&
                                (destination.isBroadcast() || destination == node->info.id)};
            assert(node->receiver.dequeue(received) == expected);
            if (expected) {
                assert(received.ingressIndex() != 0U);
                assert(received.totalSize() == packet.totalSize());
                assert(std::memcmp(received.headerAndPayload().data(), packet.headerAndPayload().data(),
                                   packet.totalSize()) == 0);
                assert(!node->receiver.dequeue(received));
            }
        }
        uplink.transmissions = bus.transmissions = tail.transmissions = 0U;
    }
};
}  // namespace

int main() {
    Network network{};
    // Broadcasts and all directed source/destination pairs across both branching forms.
    for (Node* source : {&network.root, &network.a, &network.b, &network.c}) {
        network.send(*source, HostId{0xFFU});
        for (Node* destination : {&network.root, &network.a, &network.b, &network.c}) {
            if (source != destination) {
                network.send(*source, destination->info.id);
            }
        }
    }
    // A physical but unselected ingress cannot propagate or deliver, even at a member.
    Packet packet{};
    packet.header().wire = Root::kTest;
    packet.header().endpoint = kEndpoint;
    packet.header().destination = HostId{0xFFU};
    auto result{network.root.receive(packet, Root::kBypassIngressIndex)};
    assert(result.routing == RouteResult::kInvalidIngress && result.delivery == DispatchResult::kNoEndpoint);
    result = network.gateway.receive(packet, Gateway::kBypassIngressIndex);
    assert(result.routing == RouteResult::kInvalidIngress && result.delivery == DispatchResult::kNoEndpoint);
    assert(network.events.empty());
    Packet output{};
    assert(!network.root.receiver.dequeue(output));
    assert(!network.gateway.receiver.dequeue(output));
}
