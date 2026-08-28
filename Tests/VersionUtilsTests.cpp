#include <catch2/catch_test_macros.hpp>
#include "VersionUtils.h"

using VersionUtils::versionIsNewerThan;
using VersionUtils::compareVersionsDescending;

TEST_CASE("versionIsNewerThan orders dotted versions numerically")
{
	CHECK(versionIsNewerThan("1.1.8", "1.1.7"));
	CHECK_FALSE(versionIsNewerThan("1.1.7", "1.1.8"));
	CHECK(versionIsNewerThan("2.0.0", "1.9.9"));
	CHECK_FALSE(versionIsNewerThan("1.9.9", "2.0.0"));
}

TEST_CASE("equal versions are not newer")
{
	CHECK_FALSE(versionIsNewerThan("1.1.9", "1.1.9"));
	CHECK_FALSE(versionIsNewerThan("0.0.0", "0.0.0"));
}

TEST_CASE("shorter versions are zero-padded")
{
	CHECK_FALSE(versionIsNewerThan("1.2", "1.2.0"));
	CHECK_FALSE(versionIsNewerThan("1.2.0", "1.2"));
	CHECK(versionIsNewerThan("1.2.1", "1.2"));
	CHECK(versionIsNewerThan("1.2", "1.1.9"));
}

TEST_CASE("multi-digit segments compare as numbers, not as floats")
{
	// As floats, "1.10" parses to 1.1 and would sort below "1.7"/"1.9".
	CHECK(versionIsNewerThan("1.10", "1.9"));
	CHECK(versionIsNewerThan("1.10", "1.7"));
	CHECK_FALSE(versionIsNewerThan("1.9", "1.10"));

	// The live production case: the server serves flowOS 2.9 and 2.10.
	CHECK(versionIsNewerThan("2.10", "2.9"));
	CHECK(compareVersionsDescending("2.10", "2.9") == -1);
	CHECK(compareVersionsDescending("2.9", "2.10") == 1);
	CHECK(compareVersionsDescending("2.10", "2.10") == 0);
}

TEST_CASE("download filename follows the self-updater contract")
{
	CHECK(VersionUtils::downloadFileName("FlowtoysUpdater", "osx", "1.2.0", "zip")
	      == "FlowtoysUpdater-osx-1.2.0.zip");
	CHECK(VersionUtils::downloadFileName("FlowtoysUpdater", "win-x64", "1.2.0", "exe")
	      == "FlowtoysUpdater-win-x64-1.2.0.exe");
	CHECK(VersionUtils::downloadFileName("FlowtoysUpdater", "linux", "1.2.0", "zip")
	      == "FlowtoysUpdater-linux-1.2.0.zip");

	juce::String suffix = VersionUtils::platformOSSuffix();
	CHECK((suffix == "osx" || suffix == "win-x64" || suffix == "linux"));
#if JUCE_MAC
	CHECK(suffix == "osx");
#endif
}
