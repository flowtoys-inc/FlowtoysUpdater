#include <catch2/catch_test_macros.hpp>
#include "PropConstants.h"

TEST_CASE("product IDs map to prop types")
{
	CHECK(propTypeForProductID(0x1000) == CAPSULE);
	CHECK(propTypeForProductID(0x1001) == CLUB);
}

TEST_CASE("unknown product IDs fall back to Capsule (historical behavior)")
{
	CHECK(propTypeForProductID(0) == CAPSULE);
	CHECK(propTypeForProductID(0x1002) == CAPSULE);
	CHECK(propTypeForProductID(-1) == CAPSULE);
}

TEST_CASE("constants table stays in sync")
{
	CHECK(productIds[CAPSULE] == 0x1000);
	CHECK(productIds[CLUB] == 0x1001);
	CHECK(flowtoysVID == 0xF107);
	CHECK(DATA_PACKET_MAX_LENGTH == 60);
}
