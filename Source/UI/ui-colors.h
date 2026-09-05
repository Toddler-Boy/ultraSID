#pragma once

// The custom juce::Colour IDs, keep this header cheap, (almost) everybody
// includes it. Only the tokens expand here; Theme.cpp expands paths and
// defaults into its color table, so juce is not needed at this point.

// X ( enum token, "block/key" theme path, default colour )
#define COLOR_ROLES(X) \
	X(window,				"colors/window",			juce::Colour ( 0xff'070912 )) \
	X(text,					"colors/text",				juce::Colour ( 0xff'E4E9F4 )) \
	X(accent,				"colors/accent",			juce::Colours::deeppink) \
	X(accent2,				"colors/accent2",			juce::Colours::deeppink.withRotatedHue ( 300.0f / 360.0f )) \
	X(tagLiked,				"tags/liked",				juce::Colour ( 0xff'f23d5b )) \
	X(tagPioneers,			"tags/pioneers",			juce::Colour ( 0xff'a97dff )) \
	X(tagGems,				"tags/gems",				juce::Colour ( 0xff'77b9e8 )) \
	X(tagWinners,			"tags/winners",				juce::Colour ( 0xff'ffd432 )) \
	X(stilToggleTunesOnly,	"stil/toggle-tunes-only",	juce::Colour ( 0xff'00b3ff )) \
	X(stilToggleSTIL,		"stil/toggle-stil",			juce::Colour ( 0xff'00ff95 )) \
	X(stilToggleViz,		"stil/toggle-viz",			juce::Colour ( 0xff'b388ff )) \
	X(stilBoxTitle,			"stil-box/title",			juce::Colour ( 0xff'333355 )) \
	X(stilBoxComment,		"stil-box/comment",			juce::Colour ( 0xff'2f3235 )) \
	X(stilBoxQuote,			"stil-box/quote",			juce::Colour ( 0xff'2f3235 )) \
	X(stilBoxBug,			"stil-box/bug",				juce::Colour ( 0x99'ff0000 )) \
	X(stilBoxMono,			"stil-box/mono",			juce::Colour ( 0x99'ff0000 )) \
	X(stilLink,				"stil-box/link",			juce::Colour ( 0xff'44ddff )) \
	X(chipDivot,			"chips/divot",				juce::Colour ( 0x88'000000 )) \
	X(chipText,				"chips/text",				juce::Colour ( 0x66'f0f8ff )) \
	X(voiceOff,				"colors/voice-off",			juce::Colour ( 0xff'565e66 )) \
	X(voiceOn,				"colors/voice-on",			juce::Colour ( 0xff'66ff99 )) \
	X(filterOn,				"colors/filter-on",			juce::Colour ( 0xff'66ffff )) \
	X(voiceMuted,			"colors/voice-muted",		juce::Colour ( 0xff'ff3636 )) \
	X(digi,					"colors/digi",				juce::Colour ( 0xff'00ffc8 )) \
	X(fxReal,				"colors/fx-real",			juce::Colour ( 0xff'e4e4e7 )) \
	X(fxPure,				"colors/fx-pure",			juce::Colour ( 0xff'33ffee )) \
	X(fxMagic,				"colors/fx-magic",			juce::Colour ( 0xff'ffd432 )) \
	X(fxEpic,				"colors/fx-epic",			juce::Colour ( 0xff'ff00ff )) \
	X(fxMythic,				"colors/fx-mythic",			juce::Colour ( 0xff'ff4433 )) \
	X(eq_hot,				"eq/hot",					juce::Colour ( 0xff'ffb54c )) \
	X(eq_neutral,			"eq/neutral",				juce::Colour ( 0xff'66ff99 )) \
	X(eq_cold,				"eq/cold",					juce::Colour ( 0xff'4cb5ff )) \
	X(fftLeftLine,			"fft/left-line",			juce::Colour ( 0xff'4C4CBA )) \
	X(fftLeftFill,			"fft/left-fill",			juce::Colour ( 0xff'4C4CBA )) \
	X(fftRightLine,			"fft/right-line",			juce::Colour ( 0xff'd9d9d9 )) \
	X(fftRightFill,			"fft/right-fill",			juce::Colours::transparentBlack) \
	X(fftGrid,				"fft/grid",					juce::Colour ( 0xff'ff0000 )) \
	X(fftGridText,			"fft/grid-text",			juce::Colour ( 0xff'999999 )) \
	X(exportTokenTitle,		"export/token-title",		juce::Colour ( 0xff'4fc3f7 )) \
	X(exportTokenAuthor,	"export/token-author",		juce::Colour ( 0xff'ffb74d )) \
	X(exportTokenRelease,	"export/token-release",		juce::Colour ( 0xff'ce93d8 )) \
	X(exportTokenYear,		"export/token-year",		juce::Colour ( 0xff'fff176 )) \
	X(exportTokenNumber,	"export/token-number",		juce::Colour ( 0xff'81c784 )) \
	X(exportTokenQuality,	"export/token-quality",		juce::Colour ( 0xff'f48fb1 )) \
	X(statusOk,				"colors/status-ok",			juce::Colour ( 0xff'66ff99 )) \
	X(statusWarning,		"colors/status-warning",	juce::Colour ( 0xff'ffd432 )) \
	X(statusError,			"colors/status-error",		juce::Colour ( 0xff'ff3636 )) \
	X(statusUnknown,		"colors/status-unknown",	juce::Colour ( 0xff'5a6673 )) \
	X(statusInfo,			"colors/status-info",		juce::Colour ( 0xff'253e5f ))	/* used for export indicators on the main menu */ \
	X(logo,					"colors/logo",				juce::Colour ( 0xff'ffff00 )) \
	X(logoOutline,			"colors/logo-outline",		juce::Colour ( 0xff'ff0000 )) \
	X(keycapFill,			"keycap/fill",				juce::Colour ( 0x1f'ffffff )) \
	X(keycapOutline,		"keycap/outline",			juce::Colour ( 0x40'ffffff ))

namespace UI
{
	enum colors
	{
		// Custom ColourIds must not collide with JUCE's built-in IDs (which
		// live around 0x100xxxx), this base starts a private range above them
		colorsIdBase = 0x1008000 - 1,

		#define X(role, name, col) role,
		COLOR_ROLES ( X )
		#undef X

		// End of themed stuff
		count,

		bento,
		textMuted,
		accentBright,
	};
}
