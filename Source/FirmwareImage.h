/*
  ==============================================================================

    FirmwareImage.h

    .fwimg parsing and hardware-revision rules, extracted from FirmwareManager
    so they can be unit-tested against in-memory fixtures. Depends on juce_core
    only; no files are deleted here (the caller owns that recovery policy).

    A .fwimg is a ZIP with two entries:
      "data" - the raw firmware image streamed to the prop
      "meta" - JSON: usb_vid, usb_pid, fw_rev (u16, maj<<8|min), hw_rev,
               fw_date (unix seconds), git_rev, fw_ident

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>

struct FirmwareImage
{
	juce::MemoryBlock data;
	int totalBytesToSend = 0; // data size rounded up to a whole number of packets
	juce::var meta;
	juce::String versionString; // "maj.min" decoded from fw_rev
	float version = 0;          // legacy float form; do not use for ordering
	int hwRev = 0;
	int pid = 0;
	int vid = 0;

	static juce::String getHwRevNameforHwRev(int hwRev)
	{
		switch (hwRev)
		{
		case 0: return "notset";
		case 0x300: return "C";
		case 0x400: return "D";
		case 0x500: return "E";
		case 0x600: return "F";
		case 0x700: return "G";
		case 0x800: return "H";
		}

		return "unknown";
	}

	// Exact match, except Capsule hardware revisions C (0x300) and D (0x400)
	// accept each other's firmware.
	static bool isHardwareRevCompatible(int firmwareHwRev, int deviceHwRev, bool allowCapsuleCDInterchange)
	{
		if (allowCapsuleCDInterchange)
		{
			if (deviceHwRev == 0x400 && firmwareHwRev == 0x300) return true;
			if (deviceHwRev == 0x300 && firmwareHwRev == 0x400) return true;
		}

		return deviceHwRev == firmwareHwRev;
	}

	// Returns nullptr on any malformed input: not a zip, missing entries,
	// unreadable entries, or meta that is not a JSON object.
	static std::unique_ptr<FirmwareImage> parse(juce::InputStream& input, int dataPacketMaxLength)
	{
		juce::ZipFile zip(input);

		const juce::ZipFile::ZipEntry* metaEntry = zip.getEntry("meta");
		const juce::ZipFile::ZipEntry* dataEntry = zip.getEntry("data");
		if (metaEntry == nullptr || dataEntry == nullptr) return nullptr;

		std::unique_ptr<juce::InputStream> dataStream(zip.createStreamForEntry(*dataEntry));
		if (dataStream == nullptr) return nullptr;

		auto result = std::make_unique<FirmwareImage>();

		int numBytesToSend = (int)dataStream->getTotalLength();
		float numDataPackets = (float)(ceilf(numBytesToSend * 1.0f / dataPacketMaxLength));
		result->totalBytesToSend = (int)(numDataPackets * dataPacketMaxLength);
		dataStream->readIntoMemoryBlock(result->data);

		std::unique_ptr<juce::InputStream> metaStream(zip.createStreamForEntry(*metaEntry));
		if (metaStream == nullptr) return nullptr;

		juce::var fwMeta = juce::JSON::fromString(metaStream->readEntireStreamAsString());
		// Note: var::isObject() is true for JSON arrays too (JUCE's ArrayTag
		// sets isObject), so require an actual DynamicObject — the original
		// isObject() check let array metas through to a null-deref.
		if (fwMeta.getDynamicObject() == nullptr) return nullptr;

		result->meta = fwMeta;
		result->vid = (int)fwMeta.getProperty("usb_vid", 0);
		result->pid = (int)fwMeta.getProperty("usb_pid", 0);
		juce::uint16 fwRev = (juce::uint16)(int)fwMeta.getProperty("fw_rev", 0);
		result->hwRev = (int)fwMeta.getProperty("hw_rev", 0);
		result->versionString = juce::String(fwRev >> 8) + "." + juce::String(fwRev & 0xff);
		result->version = result->versionString.getFloatValue();

		return result;
	}
};
