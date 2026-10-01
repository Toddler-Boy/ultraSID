#include <map>

#include "AssetTools.h"

#include "Database/Database.h"
#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/ImageUtils.h"
#include "ultra-shared/Video/PNG_Loader.h"
#include "ultra-shared/Video/VIC2_Render.h"

#include "ScreenshotLookup.h"

//-----------------------------------------------------------------------------

// Makes the git repository root the working directory, false outside a repository
static bool enterRepository ()
{
	auto	cwd = juce::File::getCurrentWorkingDirectory ();
	while ( ! cwd.getChildFile ( ".git" ).isDirectory () )
	{
		const auto	parent = cwd.getParentDirectory ();
		if ( parent == cwd || ! parent.isDirectory () )
			return false;

		cwd = parent;
	}

	cwd.setAsCurrentWorkingDirectory ();
	return true;
}
//-----------------------------------------------------------------------------

// Local files in the developer tree (an ignored folder, a fresh drop) are not in git
static bool isTracked ( const juce::File& file )
{
	if ( ! enterRepository () )
		return false;

	auto	cp = juce::ChildProcess ();
	if ( ! cp.start ( juce::StringArray { "git", "ls-files", "--error-unmatch", file.getFullPathName () } ) || ! cp.waitForProcessToFinish ( 10'000 ) )
		return false;

	return cp.getExitCode () == 0;
}
//-----------------------------------------------------------------------------

static void executeCommand ( const juce::String& command, const juce::String& root, const juce::StringArray& files )
{
	if ( command == "mv" && files[ 0 ] == files[ 1 ] )
		return;

	if ( command == "rm" && files[ 0 ].isEmpty () )
		return;

	if ( ! enterRepository () )
		return;

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

// A picture larger than the screen keeps its picture area: each border margin goes where it holds
// nothing but the border color, which moves into the hint. Returns the hint digit, -1 = none
static int8_t cropPictureArea ( const juce::File& file, const juce::MemoryBlock& mb, pngloader::image& img )
{
	auto sourceColor = [ &img ] ( const int x, const int y )
	{
		const auto	i = size_t ( y ) * size_t ( img.width ) + size_t ( x );
		return img.paletted ? img.palette[ img.indices[ i ] ] : img.pixels[ i ] & 0xFFFFFF;
	};

	const auto	border = sourceColor ( 0, 0 );

	auto	minX = img.width;
	auto	maxX = -1;
	auto	minY = img.height;
	auto	maxY = -1;

	for ( auto y = 0; y < img.height; ++y )
		for ( auto x = 0; x < img.width; ++x )
			if ( sourceColor ( x, y ) != border )
			{
				minX = std::min ( minX, x );	maxX = std::max ( maxX, x );
				minY = std::min ( minY, y );	maxY = std::max ( maxY, y );
			}

	if ( maxX < 0 )
		return -1;

	// The picture area of one direction when only border color lies outside it, else all of it;
	// exports put it a line or so off center, the nearest window to the standard margin wins
	auto window = [] ( const int size, const int margin, const int screen, const int lo, const int hi ) -> std::pair<int, int>
	{
		const auto	length = size - 2 * margin;
		const auto	low = std::max ( 0, hi - ( length - 1 ) );
		const auto	high = std::min ( lo, size - length );

		if ( length < screen || low > high )
			return { 0, size };

		return { std::clamp ( margin, low, high ), length };
	};

	const auto [ x0, w ] = window ( img.width, VIC2_Render::unscaledBorderSizeX, VIC2_Render::innerUnscaledWidth, minX, maxX );
	const auto [ y0, h ] = window ( img.height, VIC2_Render::unscaledBorderSizeY, VIC2_Render::innerUnscaledHeight, minY, maxY );

	if ( w == img.width && h == img.height )
		return -1;

	// The renderer shows the picture area's top-left corner, border color, at the window's corner
	VIC2_Render	vic2 ( false );
	if ( ! vic2.loadImage ( file.getFullPathName ().toRawUTF8 (), mb.getData (), mb.getSize () ) )
		return -1;

	const auto	windowX = img.width == VIC2_Render::outerUnscaledWidth ? 0 : VIC2_Render::unscaledBorderSizeX;
	const auto	borderIndex = juce::Image::BitmapData ( vic2.getCRT (), juce::Image::BitmapData::readOnly ).getLinePointer ( VIC2_Render::unscaledBorderSizeY )[ windowX ];

	auto crop = [ &img, x0, y0, w, h ] ( auto& buf )
	{
		if ( buf.empty () )
			return;

		std::remove_reference_t<decltype ( buf )>	area ( size_t ( w ) * size_t ( h ) );

		for ( auto y = 0; y < h; ++y )
			std::copy_n ( buf.begin () + size_t ( y + y0 ) * size_t ( img.width ) + size_t ( x0 ), w, area.begin () + size_t ( y ) * size_t ( w ) );

		buf = std::move ( area );
	};

	crop ( img.indices );
	crop ( img.pixels );

	img.width = w;
	img.height = h;

	const auto	encoded = pngloader::encode ( img );
	if ( encoded.empty () || ! file.replaceWithData ( encoded.data (), encoded.size () ) )
		return -1;

	return borderIndex != vic2::black ? int8_t ( borderIndex ) : int8_t ( -1 );
}
//-----------------------------------------------------------------------------

// The tune's video standard travels as a filename hint so the picture shows
// right on its own, without the tune to look it up from
static bool tuneIsNTSC ( const std::string& tuneFilename )
{
	const auto	ent = db::findDatabaseEntry ( tuneFilename );
	return ent && ent->isNTSC ();
}
//-----------------------------------------------------------------------------

// A framed picture saved as displayed: the screen centered like the renderer does, a one-color
// border cropped to 320x200. Returns that border's hint digit, -1 = no hint (none, or black)
static int8_t normalizePicture ( const juce::File& file )
{
	constexpr auto	width = VIC2_Render::outerUnscaledWidth;
	constexpr auto	height = VIC2_Render::outerUnscaledHeight;
	constexpr auto	left = VIC2_Render::unscaledBorderSizeX;
	constexpr auto	top = VIC2_Render::unscaledBorderSizeY;

	juce::MemoryBlock	mb;
	if ( ! file.loadFileAsData ( mb ) )
		return -1;

	auto	img = pngloader::decode ( mb.getData (), mb.getSize () );

	if ( ! ( img.width == width && img.height == height ) )
	{
		const auto	larger = img.width >= width && img.height >= height;
		return larger && ! imageutils::hintFromFilename ( file.getFileName () ).interlaced ? cropPictureArea ( file, mb, img ) : int8_t ( -1 );
	}

	// The renderer finds the screen and the border's VIC color
	VIC2_Render	vic2 ( false );
	if ( ! vic2.loadImage ( file.getFullPathName ().toRawUTF8 (), mb.getData (), mb.getSize () ) )
		return -1;

	const auto	shift = vic2.getScreenShift ();
	const auto	borderIndex = *juce::Image::BitmapData ( vic2.getCRT (), juce::Image::BitmapData::readOnly ).getLinePointer ( 0 );

	// The same move on the source, palette untouched; lines moved in repeat the edge
	auto move = [ shift ] ( auto& buf )
	{
		if ( buf.empty () || shift.isOrigin () )
			return;

		const auto	src = buf;
		for ( auto y = 0; y < height; ++y )
			for ( auto x = 0; x < width; ++x )
				buf[ size_t ( y * width + x ) ] = src[ size_t ( std::clamp ( y - shift.y, 0, height - 1 ) * width + std::clamp ( x - shift.x, 0, width - 1 ) ) ];
	};

	move ( img.indices );
	move ( img.pixels );

	auto sourceColor = [ &img ] ( const int x, const int y )
	{
		const auto	i = size_t ( y ) * size_t ( img.width ) + size_t ( x );
		return img.paletted ? img.palette[ img.indices[ i ] ] : img.pixels[ i ] & 0xFFFFFF;
	};

	auto	uniform = true;
	for ( auto y = 0; y < height && uniform; ++y )
		for ( auto x = 0; x < width && uniform; ++x )
			if ( ( x < left || x >= left + VIC2_Render::innerUnscaledWidth || y < top || y >= top + VIC2_Render::innerUnscaledHeight )
				 && sourceColor ( x, y ) != sourceColor ( 0, 0 ) )
				uniform = false;

	if ( ! uniform && shift.isOrigin () )
		return -1;

	if ( uniform )
	{
		auto crop = [] ( auto& buf )
		{
			if ( buf.empty () )
				return;

			std::remove_reference_t<decltype ( buf )>	inner ( size_t ( VIC2_Render::innerUnscaledWidth ) * VIC2_Render::innerUnscaledHeight );

			for ( auto y = 0; y < VIC2_Render::innerUnscaledHeight; ++y )
				std::copy_n ( buf.begin () + ( y + top ) * width + left, VIC2_Render::innerUnscaledWidth,
							  inner.begin () + size_t ( y ) * VIC2_Render::innerUnscaledWidth );

			buf = std::move ( inner );
		};

		crop ( img.indices );
		crop ( img.pixels );

		img.width = VIC2_Render::innerUnscaledWidth;
		img.height = VIC2_Render::innerUnscaledHeight;
	}

	// Only replace the file once the new version encoded successfully
	const auto	encoded = pngloader::encode ( img );
	if ( encoded.empty () || ! file.replaceWithData ( encoded.data (), encoded.size () ) )
		return -1;

	return uniform && borderIndex != vic2::black ? int8_t ( borderIndex ) : int8_t ( -1 );
}
//-----------------------------------------------------------------------------

void assettools::addScreenshots ( const juce::File& dataRoot, const std::string& tuneFilename, const juce::StringArray& filenames )
{
	if ( filenames.isEmpty () || tuneFilename.empty () )
		return;

	// Saved as displayed before the optimizer runs, a uniform border shrinks to a filename hint
	std::map<juce::String, int8_t>	borderHints;

	for ( const auto& f : filenames )
		borderHints[ f ] = normalizePicture ( juce::File ( f ) );

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

	const auto	isNTSC = tuneIsNTSC ( tuneFilename );

	for ( const auto& f : filenames )
 	{
		auto	srcFile = juce::File ( f );
		if ( ! srcFile.hasFileExtension ( ".png" ) || ! srcFile.existsAsFile () )
			continue;

		const auto	srcNumber = srcFile.getFileName ().fromLastOccurrenceOf ( "_", true, false );

		// Expecting an underscore, 2-digit number, and then ".png" = 7 characters
		if ( srcNumber.length () != 7 )
			continue;

		// A cropped border and the tune's video standard travel as filename hints
		auto	hint = imageutils::imageHint { dstName + srcNumber.upToLastOccurrenceOf ( ".", false, false ),
											   "." + srcNumber.fromLastOccurrenceOf ( ".", false, false ) };
		hint.forceNTSC = isNTSC;

		if ( const auto it = borderHints.find ( f ); it != borderHints.end () )
			hint.borderColor = it->second;

		auto	dstFile = dst.getChildFile ( imageutils::filenameFromHint ( hint ) );

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
		const auto	src = datasource::getDevFile ( "Screenshots/" + artName );
		const auto	dst = datasource::getDevFile ( "Screenshots/" + newName );

		if ( isTracked ( src ) )
			executeCommand ( "mv", "/Screenshots/", { src.getFullPathName (), dst.getFullPathName () } );
		else if ( ! src.moveFileTo ( dst ) )
			Z_ERR ( "Could not rename " << src.getFullPathName () );

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

void assettools::toggleLoadingScreen ( const std::string& artName )
{
	changeHint ( artName, [] ( imageutils::imageHint& hint ) { hint.loadingScreen = ! hint.loadingScreen; } );
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
	{
		const auto	file = datasource::getDevFile ( "Screenshots/" + artName );

		if ( isTracked ( file ) )
			executeCommand ( "rm", "/Screenshots/", { file.getFullPathName () } );
		else if ( ! file.deleteFile () )
			Z_ERR ( "Could not delete " << file.getFullPathName () );
	}
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

// A picture into the user tree under name, saved as displayed like on import; a uniform
// border shrinks to the border-color hint, black is the default and gets no hint
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

	if ( const auto border = normalizePicture ( target ); border >= 0 )
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

	auto	hint = imageutils::imageHint { dstDir + dstName + "_" + number, ".png" };
	hint.forceNTSC = tuneIsNTSC ( tuneKey );

	writeUserPicture ( imageutils::filenameFromHint ( hint ), mb );
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
