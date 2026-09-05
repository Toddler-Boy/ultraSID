#include <map>

#include "AssetTools.h"

#include "ultra-shared/Helpers/ImageUtils.h"
#include "ultra-shared/Video/PNG_Loader.h"
#include "ultra-shared/Video/VIC2_Render.h"

//-----------------------------------------------------------------------------

static void executeCommand ( const juce::String& command, const juce::String& root, const juce::StringArray& files )
{
	if ( command == "mv" && files[ 0 ] == files[ 1 ] )
		return;

	if ( command == "rm" && files[ 0 ].isEmpty () )
		return;

	// Find git repository root
	{
		auto	cwd = juce::File::getCurrentWorkingDirectory ();
		while ( ! cwd.getChildFile ( ".git" ).isDirectory () )
		{
			const auto	parent = cwd.getParentDirectory ();
			if ( parent == cwd || ! parent.isDirectory () )
				return;

			cwd = parent;
		}

		cwd.setAsCurrentWorkingDirectory ();
	}

	// Build command
	auto	cmd = "git " + command;

	for ( const auto& f : files )
		cmd += " " + ( "Data" + f.replaceCharacter ('\\', '/').fromFirstOccurrenceOf ( root, true, false) ).quoted ();

	// Execute command
	{
		auto	cp = juce::ChildProcess ();
		cp.start ( cmd );
		if ( ! cp.waitForProcessToFinish ( 10'000 ) )
		{
			Z_ERR ( "Command timed out: " + cmd );
			return;
		}

		if ( cp.getExitCode () )
			Z_ERR ( "Command failed: " + cmd + "\nOutput: " + cp.readAllProcessOutput () );
	}
}
//-----------------------------------------------------------------------------

// A uniform single-color border carries no pixel information: the image gets
// cropped to the inner 320x200 and the border color moves into the filename
// hint. Returns the hint digit, -1 = no hint (untouched, or black = default)
static int8_t cropUniformBorder ( const juce::File& file )
{
	juce::MemoryBlock	mb;
	if ( ! file.loadFileAsData ( mb ) )
		return -1;

	auto	img = pngloader::decode ( mb.getData (), mb.getSize () );

	if ( ! ( img.width == VIC2_Render::outerUnscaledWidth && img.height == VIC2_Render::outerUnscaledHeight ) )
		return -1;

	// Convert, then find the border color and the bounding box of everything
	// that differs from it. Index space is enough: distinct source colors
	// always convert to distinct indices
	VIC2_Render	vic2 ( false );
	if ( ! vic2.loadImage ( file.getFullPathName ().toRawUTF8 (), mb.getData (), mb.getSize () ) )
		return -1;

	auto	borderIndex = uint8_t ( 0 );
	auto	xOfs = 0;
	auto	yOfs = 0;

	{
		const auto	bmp = juce::Image::BitmapData ( vic2.getCRT (), juce::Image::BitmapData::readOnly );

		borderIndex = *bmp.getLinePointer ( 0 );

		auto	minX = VIC2_Render::outerUnscaledWidth;
		auto	maxX = -1;
		auto	minY = VIC2_Render::outerUnscaledHeight;
		auto	maxY = -1;

		for ( auto y = 0; y < VIC2_Render::outerUnscaledHeight; ++y )
		{
			const auto*	line = bmp.getLinePointer ( y );

			for ( auto x = 0; x < VIC2_Render::outerUnscaledWidth; ++x )
			{
				if ( line[ x ] == borderIndex )
					continue;

				minX = std::min ( minX, x );	maxX = std::max ( maxX, x );
				minY = std::min ( minY, y );	maxY = std::max ( maxY, y );
			}
		}

		// A single-color image crops at the standard window
		if ( maxX < 0 )
		{
			minX = maxX = VIC2_Render::unscaledBorderSizeX;
			minY = maxY = VIC2_Render::unscaledBorderSizeY;
		}

		// The screen window must cover the box with only border color outside;
		// no such window means artwork in the border, which stays full size
		const auto	xLow = std::max ( 0, maxX - ( VIC2_Render::innerUnscaledWidth - 1 ) );
		const auto	xHigh = std::min ( minX, VIC2_Render::outerUnscaledWidth - VIC2_Render::innerUnscaledWidth );
		const auto	yLow = std::max ( 0, maxY - ( VIC2_Render::innerUnscaledHeight - 1 ) );
		const auto	yHigh = std::min ( minY, VIC2_Render::outerUnscaledHeight - VIC2_Render::innerUnscaledHeight );

		if ( xLow > xHigh || yLow > yHigh )
			return -1;

		// When the box does not pin the window, prefer the centered standard
		// (VICE and the app's own renders); off-center captures pin themselves
		xOfs = std::clamp ( int ( VIC2_Render::unscaledBorderSizeX ), xLow, xHigh );
		yOfs = std::clamp ( int ( VIC2_Render::unscaledBorderSizeY ), yLow, yHigh );
	}

	// Crop the decoded source down to the screen window, palette untouched
	{
		auto crop = [ xOfs, yOfs ] ( auto& buf )
		{
			if ( buf.empty () )
				return;

			std::remove_reference_t<decltype ( buf )>	inner ( size_t ( VIC2_Render::innerUnscaledWidth ) * VIC2_Render::innerUnscaledHeight );

			for ( auto y = 0; y < VIC2_Render::innerUnscaledHeight; ++y )
				std::copy_n ( buf.begin () + ( y + yOfs ) * VIC2_Render::outerUnscaledWidth + xOfs,
							  VIC2_Render::innerUnscaledWidth,
							  inner.begin () + size_t ( y ) * VIC2_Render::innerUnscaledWidth );

			buf = std::move ( inner );
		};

		crop ( img.indices );
		crop ( img.pixels );

		img.width = VIC2_Render::innerUnscaledWidth;
		img.height = VIC2_Render::innerUnscaledHeight;
	}

	// Only replace the file once the cropped version encoded successfully
	const auto	encoded = pngloader::encode ( img );
	if ( encoded.empty () || ! file.replaceWithData ( encoded.data (), encoded.size () ) )
		return -1;

	return borderIndex == vic2::black ? int8_t ( -1 ) : int8_t ( borderIndex );
}
//-----------------------------------------------------------------------------

void assettools::addScreenshots ( const juce::File& dataRoot, const std::string& tuneFilename, const juce::StringArray& filenames )
{
	if ( filenames.isEmpty () || tuneFilename.empty () )
		return;

	// A uniform border shrinks to a filename hint before the optimizer runs
	std::map<juce::String, int8_t>	borderHints;

	for ( const auto& f : filenames )
		borderHints[ f ] = cropUniformBorder ( juce::File ( f ) );

	// Use oxipng to optimize screenshots
	for ( const auto& f : filenames )
	{
		const auto	cmd = "oxipng -o max -Z -s " + f.quoted ();

		auto	cp = juce::ChildProcess ();
		cp.start ( cmd );
		cp.waitForProcessToFinish ( -1 );
	}

	const auto	dstDir = juce::String ( tuneFilename ).fromFirstOccurrenceOf ( "/", false, false ).upToLastOccurrenceOf ( "/", true, false );
	const auto	dstName = juce::String ( tuneFilename ).fromLastOccurrenceOf ( "/", false, false ).upToLastOccurrenceOf ( ".", false, false ).toLowerCase ();

	auto	dst = dataRoot.getChildFile ( "Screenshots/" + dstDir );
	dst.createDirectory ();

	for ( const auto& f : filenames )
 	{
		auto	srcFile = juce::File ( f );
		if ( ! srcFile.hasFileExtension ( ".png" ) || ! srcFile.existsAsFile () )
			continue;

		const auto	srcNumber = srcFile.getFileName ().fromLastOccurrenceOf ( "_", true, false );

		// Expecting an underscore, 2-digit number, and then ".png" = 7 characters
		if ( srcNumber.length () != 7 )
			continue;

		auto	dstFileName = dstName + srcNumber;

		// A cropped border travels as the border-color hint
		if ( const auto it = borderHints.find ( f ); it != borderHints.end () && it->second >= 0 )
			dstFileName = imageutils::filenameFromHint ( { dstName + srcNumber.upToLastOccurrenceOf ( ".", false, false ),
														   "." + srcNumber.fromLastOccurrenceOf ( ".", false, false ),
														   it->second } );

		auto	dstFile = dst.getChildFile ( dstFileName );

 		srcFile.moveFileTo ( dstFile );

		// Use git to add new file to repository
		executeCommand ( "add", "/Screenshots/", { dstFile.getFullPathName () });
	}
}
//-----------------------------------------------------------------------------

void assettools::setBorderColor ( const juce::File& imageFile, const int index )
{
	auto	newName = imageFile.getFullPathName ();

	auto	hint = imageutils::hintFromFilename ( newName );
	hint.borderColor = int8_t ( index );
	newName = imageutils::filenameFromHint ( hint );

	executeCommand ( "mv", "/Screenshots/", { imageFile.getFullPathName (), newName } );
}
//-----------------------------------------------------------------------------

void assettools::toggleFirstLuma ( const juce::File& imageFile )
{
	auto	newName = imageFile.getFullPathName ();

	auto	hint = imageutils::hintFromFilename ( newName );
	hint.firstLuma = ! hint.firstLuma;
	newName = imageutils::filenameFromHint ( hint );

	executeCommand ( "mv", "/Screenshots/", { imageFile.getFullPathName (), newName } );
}
//-----------------------------------------------------------------------------

void assettools::toggleFirstLumaAll ( const juce::File& dataRoot, const std::vector<std::string>& artwork )
{
	for ( const auto& art : artwork )
	{
		auto	file = dataRoot.getChildFile ( "Screenshots/" ).getChildFile ( art );
		auto	newName = file.getFullPathName ();

		auto	hint = imageutils::hintFromFilename ( newName );
		hint.firstLuma = ! hint.firstLuma;
		newName = imageutils::filenameFromHint ( hint );

		executeCommand ( "mv", "/Screenshots/", { file.getFullPathName (), newName } );
	}
}
//-----------------------------------------------------------------------------

void assettools::toggleThumbnail ( const juce::File& imageFile )
{
	auto	newName = imageFile.getFullPathName ();

	auto	hint = imageutils::hintFromFilename ( newName );
	hint.isGameScreen = ! hint.isGameScreen;
	newName = imageutils::filenameFromHint ( hint );

	executeCommand ( "mv", "/Screenshots/", { imageFile.getFullPathName (), newName } );
}
//-----------------------------------------------------------------------------

void assettools::deleteImage ( const juce::File& imageFile )
{
	executeCommand ( "rm", "/Screenshots/", { imageFile.getFullPathName () } );
}
//-----------------------------------------------------------------------------
