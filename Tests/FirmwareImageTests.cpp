#include <catch2/catch_test_macros.hpp>
#include "FirmwareImage.h"

static constexpr int packetLength = 60; // DATA_PACKET_MAX_LENGTH

// Builds an in-memory zip with the given entries (name -> content).
static juce::MemoryBlock buildZip(const std::vector<std::pair<juce::String, juce::String>>& entries)
{
	juce::ZipFile::Builder builder;
	for (auto& e : entries)
	{
		auto content = e.second.toRawUTF8();
		builder.addEntry(std::make_unique<juce::MemoryInputStream>(content, strlen(content), true),
		                 5, e.first, juce::Time());
	}

	juce::MemoryOutputStream out;
	double progress = 0;
	builder.writeToStream(out, &progress);
	return out.getMemoryBlock();
}

static std::unique_ptr<FirmwareImage> parseBlock(const juce::MemoryBlock& block)
{
	juce::MemoryInputStream stream(block, false);
	return FirmwareImage::parse(stream, packetLength);
}

static const juce::String validMeta =
	R"({"usb_vid": 61703, "usb_pid": 4096, "fw_rev": 522, "hw_rev": 1280, "fw_date": 1580000000, "git_rev": "abc1234", "fw_ident": "capsule"})";

TEST_CASE("valid .fwimg parses")
{
	auto img = parseBlock(buildZip({ { "meta", validMeta }, { "data", "some firmware bytes" } }));
	REQUIRE(img != nullptr);

	CHECK(img->vid == 61703);  // 0xF107
	CHECK(img->pid == 4096);   // 0x1000
	CHECK(img->hwRev == 1280); // 0x500 == rev E
	CHECK(img->versionString == "2.10"); // fw_rev 522 == (2<<8)|10
	CHECK(img->data.getSize() == strlen("some firmware bytes"));
	CHECK(img->totalBytesToSend == packetLength); // rounded up to one packet
}

TEST_CASE("totalBytesToSend rounds up to whole packets")
{
	juce::String data61(juce::String::repeatedString("x", 61));
	auto img = parseBlock(buildZip({ { "meta", validMeta }, { "data", data61 } }));
	REQUIRE(img != nullptr);
	CHECK(img->totalBytesToSend == packetLength * 2);

	juce::String data60(juce::String::repeatedString("x", 60));
	img = parseBlock(buildZip({ { "meta", validMeta }, { "data", data60 } }));
	REQUIRE(img != nullptr);
	CHECK(img->totalBytesToSend == packetLength);
}

TEST_CASE("zip missing the meta entry is rejected")
{
	CHECK(parseBlock(buildZip({ { "data", "some firmware bytes" } })) == nullptr);
}

TEST_CASE("zip missing the data entry is rejected")
{
	CHECK(parseBlock(buildZip({ { "meta", validMeta } })) == nullptr);
}

TEST_CASE("meta that is not a JSON object is rejected")
{
	CHECK(parseBlock(buildZip({ { "meta", "not json at all" }, { "data", "bytes" } })) == nullptr);
	CHECK(parseBlock(buildZip({ { "meta", "[1,2,3]" }, { "data", "bytes" } })) == nullptr);
}

TEST_CASE("non-zip and empty inputs are rejected")
{
	juce::MemoryBlock garbage("this is definitely not a zip file", 33);
	CHECK(parseBlock(garbage) == nullptr);

	juce::MemoryBlock empty;
	CHECK(parseBlock(empty) == nullptr);
}

TEST_CASE("empty meta object parses with defaults")
{
	auto img = parseBlock(buildZip({ { "meta", "{}" }, { "data", "bytes" } }));
	REQUIRE(img != nullptr);
	CHECK(img->vid == 0);
	CHECK(img->pid == 0);
	CHECK(img->hwRev == 0);
	CHECK(img->versionString == "0.0");
}

TEST_CASE("hardware revision names")
{
	CHECK(FirmwareImage::getHwRevNameforHwRev(0) == "notset");
	CHECK(FirmwareImage::getHwRevNameforHwRev(0x300) == "C");
	CHECK(FirmwareImage::getHwRevNameforHwRev(0x400) == "D");
	CHECK(FirmwareImage::getHwRevNameforHwRev(0x500) == "E");
	CHECK(FirmwareImage::getHwRevNameforHwRev(0x600) == "F");
	CHECK(FirmwareImage::getHwRevNameforHwRev(0x700) == "G");
	CHECK(FirmwareImage::getHwRevNameforHwRev(0x800) == "H");
	CHECK(FirmwareImage::getHwRevNameforHwRev(0x123) == "unknown");
}

TEST_CASE("hardware compatibility is exact match")
{
	CHECK(FirmwareImage::isHardwareRevCompatible(0x500, 0x500, false));
	CHECK_FALSE(FirmwareImage::isHardwareRevCompatible(0x500, 0x600, false));
	CHECK_FALSE(FirmwareImage::isHardwareRevCompatible(0x300, 0x400, false));
}

TEST_CASE("capsule revisions C and D accept each other's firmware")
{
	CHECK(FirmwareImage::isHardwareRevCompatible(0x300, 0x400, true));
	CHECK(FirmwareImage::isHardwareRevCompatible(0x400, 0x300, true));
	CHECK(FirmwareImage::isHardwareRevCompatible(0x300, 0x300, true));
	// The interchange is only C<->D
	CHECK_FALSE(FirmwareImage::isHardwareRevCompatible(0x300, 0x500, true));
	CHECK_FALSE(FirmwareImage::isHardwareRevCompatible(0x500, 0x400, true));
}
