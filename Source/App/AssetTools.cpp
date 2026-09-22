#include <map>

#include "AssetTools.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/ImageUtils.h"
#include "ultra-shared/Video/PNG_Loader.h"
#include "ultra-shared/Video/VIC2_Render.h"

#include "ScreenshotLookup.h"

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

// Factory files move through git only where the naked repository is the data
static bool isDeveloperTree ()
{
	return buildinfo::isDeveloperMode () && ! datasource::isPak ();
}
//-----------------------------------------------------------------------------

// The file under its new name, whichever tree it lives in
static void renameScreenshot ( const std::string& artName, const juce::String& newName )
{
	if ( newName == juce::String ( artName ) )
		return;

	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	if ( lookup->isUserFile ( artName ) )
	{
		lookup->getUserFile ( artName ).moveFileTo ( lookup->getUserFile ( newName.toStdString () ) );
		return;
	}

	if ( isDeveloperTree () )
	{
		executeCommand ( "mv", "/Screenshots/", { datasource::getDevFile ( "Screenshots/" + artName ).getFullPathName (),
												  datasource::getDevFile ( "Screenshots/" + newName ).getFullPathName () } );
		return;
	}

	// A user copy under the new name shadows the factory file
	const auto	mb = datasource::loadData ( "Screenshots/" + artName );
	const auto	target = lookup->getUserFile ( newName.toStdString () );

	if ( mb.isEmpty () || target == juce::File () )
		return;

	target.getParentDirectory ().createDirectory ();

	if ( ! target.replaceWithData ( mb.getData (), mb.getSize () ) )
		Z_ERR ( "Could not write " << target.getFullPathName () );
}
//-----------------------------------------------------------------------------

template <typename F>
static void changeHint ( const std::string& artName, F change )
{
	auto	hint = imageutils::hintFromFilename ( artName );
	change ( hint );

	renameScreenshot ( artName, imageutils::filenameFromHint ( hint ) );
}
//-----------------------------------------------------------------------------

void assettools::setBorderColor ( const std::string& artName, const int index )
{
	changeHint ( artName, [ index ] ( imageutils::imageHint& hint ) { hint.borderColor = int8_t ( index ); } );
}
//-----------------------------------------------------------------------------

void assettools::toggleFirstLuma ( const std::string& artName )
{
	changeHint ( artName, [] ( imageutils::imageHint& hint ) { hint.firstLuma = ! hint.firstLuma; } );
}
//-----------------------------------------------------------------------------

void assettools::toggleFirstLumaAll ( const std::vector<std::string>& artwork )
{
	for ( const auto& art : artwork )
		toggleFirstLuma ( art );
}
//-----------------------------------------------------------------------------

void assettools::setScreenKind ( const std::string& artName, const int kind )
{
	changeHint ( artName, [ kind ] ( imageutils::imageHint& hint ) { hint.kind = imageutils::screenKind ( std::clamp ( kind, 0, 3 ) ); } );
}
//-----------------------------------------------------------------------------

void assettools::cycleScreenKind ( const std::string& artName )
{
	changeHint ( artName, [] ( imageutils::imageHint& hint ) { hint.kind = imageutils::screenKind ( ( int ( hint.kind ) + 1 ) % 4 ); } );
}
//-----------------------------------------------------------------------------

void assettools::toggleNTSC ( const std::string& artName )
{
	changeHint ( artName, [] ( imageutils::imageHint& hint ) { hint.forceNTSC = ! hint.forceNTSC; } );
}
//-----------------------------------------------------------------------------

bool assettools::canDelete ( const std::string& artName )
{
	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	return lookup->isUserFile ( artName ) || isDeveloperTree ();
}
//-----------------------------------------------------------------------------

void assettools::deleteImage ( const std::string& artName )
{
	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	if ( lookup->isUserFile ( artName ) )
	{
		lookup->getUserFile ( artName ).deleteFile ();
		return;
	}

	if ( isDeveloperTree () )
		executeCommand ( "rm", "/Screenshots/", { datasource::getDevFile ( "Screenshots/" + artName ).getFullPathName () } );
}
//-----------------------------------------------------------------------------

// The bytes of a shown picture: a dropped file by its path, otherwise a name
// in the tree
static juce::MemoryBlock pictureBytes ( const juce::String& picture )
{
	if ( juce::File::isAbsolutePath ( picture ) )
	{
		juce::MemoryBlock	mb;
		juce::File ( picture ).loadFileAsData ( mb );
		return mb;
	}

	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	return lookup->loadData ( picture.toStdString () );
}
//-----------------------------------------------------------------------------

// A picture into the user tree under name. A uniform border shrinks to the
// border-color hint, as on import; black is the default and gets no hint
static void writeUserPicture ( const juce::String& name, const juce::MemoryBlock& mb )
{
	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	const auto	target = lookup->getUserFile ( name.toStdString () );
	if ( target == juce::File () )
		return;

	target.getParentDirectory ().createDirectory ();

	if ( ! target.replaceWithData ( mb.getData (), mb.getSize () ) )
	{
		Z_ERR ( "Could not write " << target.getFullPathName () );
		return;
	}

	if ( const auto border = cropUniformBorder ( target ); border >= 0 )
	{
		auto	hint = imageutils::hintFromFilename ( name );
		hint.borderColor = border;

		target.moveFileTo ( lookup->getUserFile ( imageutils::filenameFromHint ( hint ).toStdString () ) );
	}
}
//-----------------------------------------------------------------------------

void assettools::keepForTune ( const juce::String& picture, const std::string& tuneKey )
{
	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	const auto	mb = pictureBytes ( picture );
	if ( tuneKey.empty () || mb.isEmpty () )
		return;

	// "$HVSC$/GAMES/A/B.sid" -> "GAMES/A/b_NN.png", the developer import's naming
	const auto	dstDir = juce::String ( tuneKey ).fromFirstOccurrenceOf ( "/", false, false ).upToLastOccurrenceOf ( "/", true, false );
	const auto	dstName = juce::String ( tuneKey ).fromLastOccurrenceOf ( "/", false, false ).upToLastOccurrenceOf ( ".", false, false ).toLowerCase ();
	const auto	number = juce::String ( lookup->getLastNumber ( tuneKey ) + 1 ).paddedLeft ( '0', 2 );

	const auto	plainName = dstDir + dstName + "_" + number + ".png";

	writeUserPicture ( plainName, mb );
}
//-----------------------------------------------------------------------------

void assettools::saveToFolder ( const juce::String& picture, const std::string& folder )
{
	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	const auto	mb = pictureBytes ( picture );
	if ( mb.isEmpty () )
		return;

	const auto	leaf = picture.replaceCharacter ( '\\', '/' ).fromLastOccurrenceOf ( "/", false, false );
	const auto	name = ( folder.empty () ? juce::String () : juce::String ( folder ) + "/" ) + leaf;

	if ( const auto target = lookup->getUserFile ( name.toStdString () ); target.existsAsFile () )
	{
		Z_ERR ( "Not overwriting " << target.getFullPathName () );
		return;
	}

	writeUserPicture ( name, mb );
}
//-----------------------------------------------------------------------------
