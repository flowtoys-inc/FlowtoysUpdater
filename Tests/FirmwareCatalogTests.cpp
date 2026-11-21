#include <catch2/catch_test_macros.hpp>
#include "FirmwareCatalog.h"

using juce::String;

namespace
{
String entryJson(const String& file, const String& extra = {})
{
	return "{\"file\": " + juce::JSON::toString(file) + ", \"size\": 66230, \"sha256\": \"" + String::repeatedString("ab", 32) + "\"" + extra + "}";
}

String indexJson(const String& entries, const String& schema = "1")
{
	return "{\"schema\": " + schema + ", \"generated\": \"2026-11-19T19:43:06.696Z\", \"firmware\": [" + entries + "]}";
}
} // namespace

TEST_CASE("the catalog is read over HTTPS from the web updater's bucket")
{
	CHECK(FirmwareCatalog::indexUrl == "https://updater.flowtoys.com/firmware/index.json");
	CHECK(FirmwareCatalog::indexUrl.startsWith("https://"));
}

TEST_CASE("a schema 1 index yields its entries in order")
{
	auto r = FirmwareCatalog::parseIndex(indexJson(entryJson("flowOS 2.9-capsule-d.fwimg") + "," + entryJson("flowOS 2.10-club (long handle, pirouette, grip)-e.fwimg")));
	REQUIRE(r.ok);
	REQUIRE(r.entries.size() == 2);
	CHECK(r.entries[0].file == "flowOS 2.9-capsule-d.fwimg");
	CHECK(r.entries[0].size == 66230);
	CHECK(r.entries[0].sha256 == String::repeatedString("ab", 32));
	CHECK(r.entries[1].file == "flowOS 2.10-club (long handle, pirouette, grip)-e.fwimg");
}

TEST_CASE("unknown keys are ignored")
{
	auto r = FirmwareCatalog::parseIndex(indexJson(entryJson("a.fwimg", ", \"pid\": 4097, \"fw_ident\": \"flowOS/club\", \"future\": [1]")));
	REQUIRE(r.ok);
	CHECK(r.entries.size() == 1);
}

TEST_CASE("an index that isn't schema 1 is rejected whole")
{
	CHECK_FALSE(FirmwareCatalog::parseIndex(indexJson(entryJson("a.fwimg"), "2")).ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex("{\"firmware\": []}").ok);
}

TEST_CASE("a captive portal page or garbage is rejected")
{
	CHECK_FALSE(FirmwareCatalog::parseIndex("<html>Sign in to Wi-Fi</html>").ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex("").ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex("[1,2]").ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex("{\"schema\": 1, \"firmware\": {}}").ok);
}

TEST_CASE("an entry missing its file, size or a well-formed sha256 rejects the index")
{
	const String sha = String::repeatedString("ab", 32);
	CHECK_FALSE(FirmwareCatalog::parseIndex(indexJson("{\"size\": 1, \"sha256\": \"" + sha + "\"}")).ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex(indexJson("{\"file\": \"a.fwimg\", \"sha256\": \"" + sha + "\"}")).ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex(indexJson("{\"file\": \"a.fwimg\", \"size\": 0, \"sha256\": \"" + sha + "\"}")).ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex(indexJson("{\"file\": \"a.fwimg\", \"size\": 1, \"sha256\": \"abc\"}")).ok);
	CHECK_FALSE(FirmwareCatalog::parseIndex(indexJson("{\"file\": \"a.fwimg\", \"size\": 1, \"sha256\": \"" + String::repeatedString("AB", 32) + "\"}")).ok);
}

TEST_CASE("names that could escape the cache folder are rejected")
{
	CHECK(FirmwareCatalog::isSafeFileName("flowOS 2.10-club (short handle, delphin)-e.fwimg"));
	CHECK_FALSE(FirmwareCatalog::isSafeFileName(""));
	CHECK_FALSE(FirmwareCatalog::isSafeFileName("../evil.fwimg"));
	CHECK_FALSE(FirmwareCatalog::isSafeFileName("sub/a.fwimg"));
	CHECK_FALSE(FirmwareCatalog::isSafeFileName("sub\\a.fwimg"));
	CHECK_FALSE(FirmwareCatalog::isSafeFileName(".hidden.fwimg"));
	CHECK_FALSE(FirmwareCatalog::isSafeFileName("a.zip"));
	CHECK_FALSE(FirmwareCatalog::isSafeFileName(String("a") + juce::juce_wchar(10) + ".fwimg"));

	CHECK_FALSE(FirmwareCatalog::parseIndex(indexJson(entryJson("../evil.fwimg"))).ok);
}

TEST_CASE("file URLs are encoded like encodeURIComponent, as the web updater does")
{
	CHECK(FirmwareCatalog::fileUrl("flowOS 2.10-club (short handle, delphin)-e.fwimg") == "https://updater.flowtoys.com/firmware/flowOS%202.10-club%20(short%20handle%2C%20delphin)-e.fwimg");
	CHECK(FirmwareCatalog::fileUrl("a+b&c#d.fwimg") == "https://updater.flowtoys.com/firmware/a%2Bb%26c%23d.fwimg");
	CHECK(FirmwareCatalog::fileUrl(String::fromUTF8("\xc3\xa9.fwimg")) == "https://updater.flowtoys.com/firmware/%C3%A9.fwimg");
}

TEST_CASE("a file matches its entry only when size and sha256 both match")
{
	FirmwareCatalog::Entry e{"a.fwimg", 3, String::repeatedString("ab", 32)};
	CHECK(FirmwareCatalog::matches(e, 3, String::repeatedString("ab", 32)));
	CHECK_FALSE(FirmwareCatalog::matches(e, 4, String::repeatedString("ab", 32)));
	CHECK_FALSE(FirmwareCatalog::matches(e, 3, String::repeatedString("cd", 32)));
}
