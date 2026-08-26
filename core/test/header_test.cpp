/**
 * @file header_test.cpp
 * @brief Header accessor coverage: control byte fields, endpoint packing, null safety.
 *
 * Equivalence classes:
 * - QoS: all enum values
 * - Extensions: true / false
 * - Transport: TRANSPORT_SIMPLE
 * - Endpoint namespace: all WsNamespace values
 * - Endpoint id: min, typical, max 14-bit, overflow bits masked on set
 * - Null header pointers: getters return defaults, setters are no-ops
 */

#include <gtest/gtest.h>

#include "header.h"

namespace wirespaces::test {
namespace {

class HeaderQoSTest : public ::testing::TestWithParam<QoSCode> {};

// Verifies each QoS code round-trips through set/get accessors.
TEST_P(HeaderQoSTest, RoundTripsQoS) {
    Header header{};
    header_set_qos(&header, GetParam());
    EXPECT_EQ(header_get_qos(&header), GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    AllQoS,
    HeaderQoSTest,
    ::testing::Values(QOS_CRITICAL, QOS_HIGH, QOS_NORMAL, QOS_BACKGROUND));

class HeaderExtensionsTest : public ::testing::TestWithParam<bool> {};

// Verifies the extensions flag round-trips through set/get accessors.
TEST_P(HeaderExtensionsTest, RoundTripsExtensionsFlag) {
    Header header{};
    header_set_has_extensions(&header, GetParam());
    EXPECT_EQ(header_get_has_extensions(&header), GetParam());
}

INSTANTIATE_TEST_SUITE_P(Extensions, HeaderExtensionsTest, ::testing::Bool());

// Verifies control_fields setter applies QoS, extensions, and transport together.
TEST(HeaderControlFieldsTest, AppliesAllControlFields) {
    Header header{};
    const ControlFields fields{QOS_HIGH, true, TRANSPORT_SIMPLE};
    header_set_control_fields(&header, fields);

    EXPECT_EQ(header_get_qos(&header), QOS_HIGH);
    EXPECT_TRUE(header_get_has_extensions(&header));
    EXPECT_EQ(header_get_transport_type(&header), TRANSPORT_SIMPLE);
}

struct EndpointPackCase {
    WsNamespace namespace_id;
    uint16_t endpoint_id;
    uint16_t expected_packed_id;
};

class HeaderEndpointPackTest : public ::testing::TestWithParam<EndpointPackCase> {};

// Verifies namespace and endpoint id pack into the 16-bit endpoint field.
TEST_P(HeaderEndpointPackTest, PacksNamespaceAndEndpointId) {
    Header header{};
    header_set_endpoint(&header, GetParam().namespace_id, GetParam().endpoint_id);

    EXPECT_EQ(header_get_namespace(&header), GetParam().namespace_id);
    EXPECT_EQ(header_get_endpoint_id(&header), GetParam().expected_packed_id);
}

INSTANTIATE_TEST_SUITE_P(
    EndpointPacking,
    HeaderEndpointPackTest,
    ::testing::Values(
        EndpointPackCase{WS_NAMESPACE_USER0, 0x0001U, 0x0001U},
        EndpointPackCase{WS_NAMESPACE_USER1, 0x0002U, 0x0002U},
        EndpointPackCase{WS_NAMESPACE_USER2, 0x1234U, 0x1234U},
        EndpointPackCase{WS_NAMESPACE_COMMON, 0x3FFFU, 0x3FFFU},
        EndpointPackCase{WS_NAMESPACE_USER0, 0x0000U, 0x0000U}));

// Verifies endpoint id bits above 14 bits are masked on set.
TEST(HeaderEndpointPackTest, MasksOverflowEndpointIdBits) {
    Header header{};
    header_set_endpoint(&header, WS_NAMESPACE_USER0, 0xFFFFU);
    EXPECT_EQ(header_get_endpoint_id(&header), 0x3FFFU);
}

// Verifies null header getters return documented default values.
TEST(HeaderNullSafetyTest, GettersReturnDefaultsForNullHeader) {
    EXPECT_EQ(header_get_qos(nullptr), QOS_NORMAL);
    EXPECT_FALSE(header_get_has_extensions(nullptr));
    EXPECT_EQ(header_get_transport_type(nullptr), TRANSPORT_SIMPLE);
    EXPECT_EQ(header_get_endpoint_id(nullptr), 0U);
    EXPECT_EQ(header_get_namespace(nullptr), WS_NAMESPACE_USER0);
}

// Verifies null header setters are no-ops (do not crash).
TEST(HeaderNullSafetyTest, SettersAcceptNullHeader) {
    header_set_qos(nullptr, QOS_CRITICAL);
    header_set_has_extensions(nullptr, true);
    header_set_transport_type(nullptr, TRANSPORT_SIMPLE);
    header_set_control_fields(nullptr, ControlFields{QOS_HIGH, true, TRANSPORT_SIMPLE});
    header_set_endpoint(nullptr, WS_NAMESPACE_COMMON, 0x0001U);
    SUCCEED();
}

} // namespace
} // namespace wirespaces::test
