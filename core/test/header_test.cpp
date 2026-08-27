/**
 * @file header_test.cpp
 * @brief Header accessor coverage: control byte fields, endpoint packing, null safety.
 */

#include <gtest/gtest.h>

#include "core/wirespaces_core.hpp"

namespace wirespaces::test {
namespace {

using wirespaces::ControlFields;
using wirespaces::Header;
using wirespaces::Namespace;
using wirespaces::kNamespaceCommon;
using wirespaces::kNamespaceUser0;
using wirespaces::kNamespaceUser1;
using wirespaces::kNamespaceUser2;
using wirespaces::kQoSBackground;
using wirespaces::kQoSCritical;
using wirespaces::kQoSHigh;
using wirespaces::kQoSNormal;
using wirespaces::kTransportSimple;

class HeaderQoSTest : public ::testing::TestWithParam<ws_qos_t> {};

TEST_P(HeaderQoSTest, RoundTripsQoS) {
    Header header{};
    ws_header_set_qos(&header, GetParam());
    EXPECT_EQ(ws_header_get_qos(&header), GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    AllQoS,
    HeaderQoSTest,
    ::testing::Values(kQoSCritical, kQoSHigh, kQoSNormal, kQoSBackground));

class HeaderExtensionsTest : public ::testing::TestWithParam<bool> {};

TEST_P(HeaderExtensionsTest, RoundTripsExtensionsFlag) {
    Header header{};
    ws_header_set_has_extensions(&header, GetParam());
    EXPECT_EQ(ws_header_get_has_extensions(&header), GetParam());
}

INSTANTIATE_TEST_SUITE_P(Extensions, HeaderExtensionsTest, ::testing::Bool());

TEST(HeaderControlFieldsTest, AppliesAllControlFields) {
    Header header{};
    const ControlFields fields{kQoSHigh, true, kTransportSimple};
    ws_header_set_control_fields(&header, fields);

    EXPECT_EQ(ws_header_get_qos(&header), kQoSHigh);
    EXPECT_TRUE(ws_header_get_has_extensions(&header));
    EXPECT_EQ(ws_header_get_transport_type(&header), kTransportSimple);
}

struct EndpointPackCase {
    Namespace namespace_id;
    uint16_t endpoint_id;
    uint16_t expected_packed_id;
};

class HeaderEndpointPackTest : public ::testing::TestWithParam<EndpointPackCase> {};

TEST_P(HeaderEndpointPackTest, PacksNamespaceAndEndpointId) {
    Header header{};
    ws_header_set_endpoint(&header, GetParam().namespace_id, GetParam().endpoint_id);

    EXPECT_EQ(ws_header_get_namespace(&header), GetParam().namespace_id);
    EXPECT_EQ(ws_header_get_endpoint_id(&header), GetParam().expected_packed_id);
}

INSTANTIATE_TEST_SUITE_P(
    EndpointPacking,
    HeaderEndpointPackTest,
    ::testing::Values(
        EndpointPackCase{kNamespaceUser0, 0x0001U, 0x0001U},
        EndpointPackCase{kNamespaceUser1, 0x0002U, 0x0002U},
        EndpointPackCase{kNamespaceUser2, 0x1234U, 0x1234U},
        EndpointPackCase{kNamespaceCommon, 0x3FFFU, 0x3FFFU},
        EndpointPackCase{kNamespaceUser0, 0x0000U, 0x0000U}));

TEST(HeaderEndpointPackTest, MasksOverflowEndpointIdBits) {
    Header header{};
    ws_header_set_endpoint(&header, kNamespaceUser0, 0xFFFFU);
    EXPECT_EQ(ws_header_get_endpoint_id(&header), 0x3FFFU);
}

TEST(HeaderNullSafetyTest, GettersReturnDefaultsForNullHeader) {
    EXPECT_EQ(ws_header_get_qos(nullptr), kQoSNormal);
    EXPECT_FALSE(ws_header_get_has_extensions(nullptr));
    EXPECT_EQ(ws_header_get_transport_type(nullptr), kTransportSimple);
    EXPECT_EQ(ws_header_get_endpoint_id(nullptr), 0U);
    EXPECT_EQ(ws_header_get_namespace(nullptr), kNamespaceUser0);
}

TEST(HeaderNullSafetyTest, SettersAcceptNullHeader) {
    ws_header_set_qos(nullptr, kQoSCritical);
    ws_header_set_has_extensions(nullptr, true);
    ws_header_set_transport_type(nullptr, kTransportSimple);
    ws_header_set_control_fields(nullptr, ControlFields{kQoSHigh, true, kTransportSimple});
    ws_header_set_endpoint(nullptr, kNamespaceCommon, 0x0001U);
    SUCCEED();
}

} // namespace
} // namespace wirespaces::test
