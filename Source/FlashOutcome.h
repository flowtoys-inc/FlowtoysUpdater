/*
  ==============================================================================

    FlashOutcome.h

    How a finished flash is judged, kept free of HID and GUI code so it can be
    unit-tested without a prop. Depends on juce_core only.

    After ProgramDone and before AppReset(App), Prop::run sends
    GetVersion(App) to the bootloader. The bootloader reads the version block
    from the app area of flash, so the answer reflects the image just written.
    That read-back is the only confirmation a Capsule 2.0 ever gives: its app
    firmware has no USB, so it never reappears after the reset.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>

namespace FlashOutcome
{
enum class Outcome
{
	Verified,          // read-back matches the image
	WrittenUnverified, // write finished, but the read-back couldn't confirm it
	Failed             // read-back reports a different firmware than the image
};

// answered:      the device replied to GetVersion(App)
// readBackFwRev: fw_rev from that reply, (major << 8) | minor
// imageFwRev:    fw_rev from the image's meta, 0 when the meta has none
inline Outcome classifyReadBack(bool answered, int readBackFwRev, int imageFwRev)
{
	// No answer is not evidence of a bad write: the write itself completed
	// (ProgramDone). Never turn it into a failure or a re-flash prompt.
	if (!answered) return Outcome::WrittenUnverified;

	// Nothing to compare against.
	if (imageFwRev == 0) return Outcome::WrittenUnverified;

	// fw_rev is a u16 on the wire and in the meta.
	return (readBackFwRev & 0xffff) == (imageFwRev & 0xffff) ? Outcome::Verified : Outcome::Failed;
}

inline bool isFailure(Outcome o)
{
	return o == Outcome::Failed;
}

inline juce::String toString(Outcome o)
{
	switch (o)
	{
		case Outcome::Verified: return "verified";
		case Outcome::WrittenUnverified: return "written, unverified";
		case Outcome::Failed: return "failed";
	}

	return "unknown";
}

// End-screen text once every prop finished without a failure.
// anyCapsule: a Capsule was among the flashed props. It won't reappear on USB
// after the reset, so the user is told to replug it.
inline juce::String endScreenText(int numVerified, int numUnverified, bool anyCapsule)
{
	juce::ignoreUnused(numVerified);
	const juce::String replug = "unplug the prop and plug it back in to finish.";

	if (numUnverified > 0)
		return "firmware written.\n" + replug + "\nif it doesn't light up as usual, update it again.";

	if (anyCapsule) return "update successful.\n" + replug;

	return "update successful.\nready to flow ...";
}
} // namespace FlashOutcome
