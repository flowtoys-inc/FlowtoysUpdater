#include <catch2/catch_test_macros.hpp>
#include "FlashOutcome.h"

using FlashOutcome::Outcome;

TEST_CASE("read-back matching the image confirms the write")
{
	CHECK(FlashOutcome::classifyReadBack(true, 522, 522) == Outcome::Verified);
}

TEST_CASE("read-back of a different version is a real failure")
{
	CHECK(FlashOutcome::classifyReadBack(true, 521, 522) == Outcome::Failed);
	CHECK(FlashOutcome::isFailure(Outcome::Failed));
}

TEST_CASE("no answer to the read-back is written-but-unverified, not a failure")
{
	CHECK(FlashOutcome::classifyReadBack(false, 0, 522) == Outcome::WrittenUnverified);
	CHECK(FlashOutcome::classifyReadBack(false, 521, 522) == Outcome::WrittenUnverified);
	CHECK_FALSE(FlashOutcome::isFailure(Outcome::WrittenUnverified));
	CHECK_FALSE(FlashOutcome::isFailure(Outcome::Verified));
}

TEST_CASE("an image without fw_rev in its meta cannot be checked, so it is not failed")
{
	CHECK(FlashOutcome::classifyReadBack(true, 522, 0) == Outcome::WrittenUnverified);
}

TEST_CASE("only the low 16 bits of fw_rev are compared (it is a u16 on the wire)")
{
	CHECK(FlashOutcome::classifyReadBack(true, 0x1020A, 0x020A) == Outcome::Verified);
}

TEST_CASE("end screen: verified Vision props keep the original message")
{
	CHECK(FlashOutcome::endScreenText(2, 0, false) == "update successful.\nready to flow ...");
}

TEST_CASE("end screen: verified Capsules are told to replug, since they never come back on USB")
{
	auto text = FlashOutcome::endScreenText(1, 0, true);
	CHECK(text.startsWith("update successful."));
	CHECK(text.contains("unplug the prop and plug it back in to finish"));
}

TEST_CASE("end screen: any unverified prop gets the written / replug-to-finish wording")
{
	for (bool anyCapsule : {false, true})
	{
		auto text = FlashOutcome::endScreenText(1, 1, anyCapsule);
		CHECK(text.startsWith("firmware written."));
		CHECK(text.contains("unplug the prop and plug it back in to finish"));
		CHECK_FALSE(text.contains("successful"));
	}
}
