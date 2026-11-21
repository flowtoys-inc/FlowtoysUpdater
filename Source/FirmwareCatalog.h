/*
  ==============================================================================

    FirmwareCatalog.h

    The firmware catalog the app mirrors: index.json (schema 1) on the web
    updater's S3 + CloudFront origin, over HTTPS. Kept free of network and GUI
    code so it can be unit-tested. Depends on juce_core only.

    Schema: brain/FIRMWARE-FORMAT.md. Each entry names a .fwimg file and gives
    its size and sha256, so a download (or a cached copy) is checked before it
    is parsed.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>

namespace FirmwareCatalog
{
inline const juce::String baseUrl = "https://updater.flowtoys.com/firmware/";
inline const juce::String indexUrl = baseUrl + "index.json";

struct Entry
{
	juce::String file;   // .fwimg file name, one path segment
	juce::int64 size;    // bytes
	juce::String sha256; // of the whole file, lowercase hex
};

struct ParseResult
{
	bool ok = false;
	juce::Array<Entry> entries;
	juce::String error;
};

// The name comes from the network and becomes a file in the cache folder
// (firmwareFolder.getChildFile(name)), so it must stay a plain file there.
inline bool isSafeFileName(const juce::String& name)
{
	if (name.isEmpty() || !name.endsWith(".fwimg")) return false;
	if (name.startsWithChar('.') || name != name.trim()) return false;
	if (name.containsAnyOf("/\\:")) return false; // ':' is a drive or stream separator on Windows

	for (auto c : name)
		if (c < 0x20 || c == 0x7f) return false;

	return true;
}

inline bool isLowerHexSha256(const juce::String& s)
{
	return s.length() == 64 && s.containsOnly("0123456789abcdef");
}

// One bad entry rejects the whole index, like the web updater's parseCatalog():
// a half-understood catalog must not shrink what the app offers.
inline ParseResult parseIndex(const juce::String& json)
{
	ParseResult r;
	juce::var data;
	if (juce::JSON::parse(json, data).failed() || !data.isObject())
	{
		r.error = "not a JSON object";
		return r;
	}

	if (!data.getProperty("schema", juce::var()).isInt() || (int)data.getProperty("schema", 0) != 1)
	{
		r.error = "unsupported schema";
		return r;
	}

	const juce::var list = data.getProperty("firmware", juce::var());
	if (!list.isArray())
	{
		r.error = "no firmware array";
		return r;
	}

	for (const auto& e : *list.getArray())
	{
		const juce::var file = e.getProperty("file", juce::var());
		const juce::var size = e.getProperty("size", juce::var());
		const juce::var sha = e.getProperty("sha256", juce::var());

		if (!file.isString() || !isSafeFileName(file.toString()))
		{
			r.error = "bad file name: " + file.toString();
			return r;
		}
		if (!(size.isInt() || size.isInt64()) || (juce::int64)size <= 0)
		{
			r.error = "bad size for " + file.toString();
			return r;
		}
		if (!sha.isString() || !isLowerHexSha256(sha.toString()))
		{
			r.error = "bad sha256 for " + file.toString();
			return r;
		}

		r.entries.add({file.toString(), (juce::int64)size, sha.toString()});
	}

	r.ok = true;
	return r;
}

// Same bytes as JavaScript's encodeURIComponent(), which the web updater
// uses for the same objects: UTF-8, everything but A-Z a-z 0-9 -_.!~*'() escaped.
inline juce::String fileUrl(const juce::String& file)
{
	const juce::String unreserved = "-_.!~*'()";
	juce::String out;
	for (const char* p = file.toRawUTF8(); *p != 0; ++p)
	{
		const auto c = (unsigned char)*p;
		if (juce::CharacterFunctions::isLetterOrDigit((char)c) && c < 0x80)
			out << (char)c;
		else if (unreserved.containsChar((juce::juce_wchar)c))
			out << (char)c;
		else
			out << "%" << juce::String::toHexString((int)c).toUpperCase().paddedLeft('0', 2);
	}
	return baseUrl + out;
}

inline bool matches(const Entry& e, juce::int64 fileSize, const juce::String& fileSha256)
{
	return fileSize == e.size && fileSha256 == e.sha256;
}
} // namespace FirmwareCatalog
