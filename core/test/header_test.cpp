/**
 * @file header_test.cpp
 * @brief Header control fields, strong identifiers, and endpoint packing.
 */

#include <gtest/gtest.h>

#include <runtime/core.hpp>

namespace wirespaces::test {
namespace {

class HeaderQoSTest : public ::testing::TestWithParam<QoS> {};

// Every QoS value round-trips through the packed control byte.
TEST_P(HeaderQoSTest, RoundTripsQoS) {
    Header header{};
    header.setQoS(GetParam());
    EXPECT_EQ(header.qos(), GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    AllQoS,
    HeaderQoSTest,
    ::testing::Values(QoS::kCritical, QoS::kHigh, QoS::kNormal, QoS::kBackground));

// The extension flag can be set and cleared without disturbing other fields.
TEST(HeaderTest, RoundTripsExtensionsFlag) {
    Header header{};
    header.setHasExtensions(true);
    EXPECT_TRUE(header.hasExtensions());
    header.setHasExtensions(false);
    EXPECT_FALSE(header.hasExtensions());
}

// Applying the combined control value updates all encoded subfields.
TEST(HeaderTest, AppliesAllControlFields) {
    Header header{};
    header.setControlFields(ControlFields{QoS::kHigh, true, TransportType::kSimple});
    EXPECT_EQ(header.qos(), QoS::kHigh);
    EXPECT_TRUE(header.hasExtensions());
    EXPECT_EQ(header.transportType(), TransportType::kSimple);
}

struct EndpointPackCase {
    Namespace namespace_id;
    uint16_t endpoint_id;
};

class EndpointAddressTest : public ::testing::TestWithParam<EndpointPackCase> {};

// Namespace and the 14-bit Endpoint ID round-trip through EndpointAddress.
TEST_P(EndpointAddressTest, PacksNamespaceAndEndpointId) {
    const EndpointPackCase value{GetParam()};
    const EndpointAddress address{EndpointAddress::from(value.namespace_id, value.endpoint_id)};
    EXPECT_EQ(address.namespaceId(), value.namespace_id);
    EXPECT_EQ(address.endpointId(), value.endpoint_id & 0x3FFFU);
}

INSTANTIATE_TEST_SUITE_P(
    EndpointPacking,
    EndpointAddressTest,
    ::testing::Values(
        EndpointPackCase{Namespace::kUser0, 0x0001U},
        EndpointPackCase{Namespace::kUser1, 0x0002U},
        EndpointPackCase{Namespace::kUser2, 0x1234U},
        EndpointPackCase{Namespace::kCommon, 0xFFFFU}));

} // namespace
} // namespace wirespaces::test
