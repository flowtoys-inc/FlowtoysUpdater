/*
  ==============================================================================

    VersionUtils.h

    Pure version-string helpers, extracted from AppUpdater so they can be
    unit-tested without the GUI/singleton machinery. Deliberately depends on
    juce_core only.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>

namespace VersionUtils
{
	// Dotted numeric comparison ("1.2.3"), shorter side zero-padded.
	// Returns true when versionToCheck is strictly newer than referenceVersion.
	inline bool versionIsNewerThan(const juce::String& versionToCheck, const juce::String& referenceVersion)
	{
		juce::StringArray fileVersionSplit;
		fileVersionSplit.addTokens(versionToCheck, juce::StringRef("."), juce::StringRef("\""));

		juce::StringArray minVersionSplit;
		minVersionSplit.addTokens(referenceVersion, juce::StringRef("."), juce::StringRef("\""));

		int maxVersionNumbers = juce::jmax<int>(fileVersionSplit.size(), minVersionSplit.size());
		while (fileVersionSplit.size() < maxVersionNumbers) fileVersionSplit.add("0");
		while (minVersionSplit.size() < maxVersionNumbers) minVersionSplit.add("0");

		for (int i = 0; i < maxVersionNumbers; i++)
		{
			int fV = fileVersionSplit[i].getIntValue();
			int minV = minVersionSplit[i].getIntValue();
			if (fV > minV) return true;
			else if (fV < minV) return false;
		}

		return false; // equal
	}

	// Ordering for "latest first" sorts: -1 if a is newer than b, 1 if older, 0 if equal.
	// Replaces the old float-based comparison, where "1.10" parsed as 1.1 and
	// sorted below "1.7" (live issue: the server serves flowOS 2.9 and 2.10).
	inline int compareVersionsDescending(const juce::String& a, const juce::String& b)
	{
		if (versionIsNewerThan(a, b)) return -1;
		if (versionIsNewerThan(b, a)) return 1;
		return 0;
	}

	// The self-updater's artifact naming contract. Server-side update.json and
	// the release workflow both depend on this exact shape:
	//   <prefix>-<osSuffix>-<version>.<extension>
	inline juce::String downloadFileName(const juce::String& filePrefix, const juce::String& osSuffix,
	                                     const juce::String& version, const juce::String& extension)
	{
		return filePrefix + "-" + osSuffix + "-" + version + "." + extension;
	}

	inline juce::String platformOSSuffix()
	{
#if JUCE_WINDOWS
		return "win-x64";
#elif JUCE_MAC
		return "osx";
#else
		return "linux";
#endif
	}
}
