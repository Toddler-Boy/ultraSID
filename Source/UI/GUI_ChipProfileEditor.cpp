#include "GUI_ChipProfileEditor.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/FileUtils.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

// Finds the row with the given profile name (naive second-cell split; a quoted
// name containing commas would defeat it, none exist)
static int findRowByName ( const juce::StringArray& lines, const juce::String& name )
{
	for ( auto i = 1; i < lines.size (); ++i )
	{
		const auto	cellName = lines[ i ].fromFirstOccurrenceOf ( ",", false, false ).upToFirstOccurrenceOf ( ",", false, false ).trim ();

		if ( cellName.equalsIgnoreCase ( name ) )
			return i;
	}

	return -1;
}
//-----------------------------------------------------------------------------

// Inserts a cell at the given index into a CSV line, respecting quoted cells;
// a line ending before that index is left alone, its missing cells default
static juce::String insertCsvCell ( const juce::String& line, const int cellIdx, const juce::String& cell )
{
	auto	commas = 0;
	auto	inQuotes = false;

	for ( auto i = 0; i < line.length (); ++i )
	{
		const auto	c = line[ i ];

		if ( c == '"' )
			inQuotes = ! inQuotes;
		else if ( c == ',' && ! inQuotes && ++commas == cellIdx )
			return line.substring ( 0, i + 1 ) + cell + "," + line.substring ( i + 1 );
	}

	return line;
}
//-----------------------------------------------------------------------------

// Quote-aware start/length of cell cellIdx in a CSV line; false when the line
// ends before that cell
static bool csvCellRange ( const juce::String& line, const int cellIdx, int& start, int& length )
{
	auto	commas = 0;
	auto	inQuotes = false;
	start = 0;

	for ( auto i = 0; i < line.length (); ++i )
	{
		const auto	c = line[ i ];

		if ( c == '"' )
		{
			inQuotes = ! inQuotes;
		}
		else if ( c == ',' && ! inQuotes )
		{
			if ( commas == cellIdx )
			{
				length = i - start;
				return true;
			}

			++commas;
			start = i + 1;
		}
	}

	if ( commas != cellIdx )
		return false;

	length = line.length () - start;
	return true;
}
//-----------------------------------------------------------------------------

// Loads the csv sans trailing blank lines; the header seeds an empty file
static juce::StringArray loadCsvLines ( const juce::File& csv, const juce::String& headerIfEmpty = {} )
{
	juce::StringArray	lines;

	if ( csv.existsAsFile () )
		lines.addLines ( csv.loadFileAsString () );

	while ( ! lines.isEmpty () && lines[ lines.size () - 1 ].trim ().isEmpty () )
		lines.remove ( lines.size () - 1 );

	if ( lines.isEmpty () && headerIfEmpty.isNotEmpty () )
		lines.add ( headerIfEmpty );

	return lines;
}
//-----------------------------------------------------------------------------

GUI_ChipProfileEditor::SliderRow::SliderRow ( const juce::String& stringKey, const double min, const double max, const double increment, const double defaultValue )
	: label ( stringKey, 13.0f, 500 )
{
	setName ( stringKey.fromLastOccurrenceOf ( "/", false, false ) );
	slider.setName ( "slider" );

	slider.setRange ( min, max, increment );
	slider.setNumDecimalPlacesToDisplay ( 2 );
	slider.setDoubleClickReturnValue ( true, defaultValue );
	slider.setTextBoxStyle ( juce::Slider::TextBoxRight, false, 56, 20 );

	addAndMakeVisible ( label );
	addAndMakeVisible ( slider );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::SliderRow::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
								"UI/layouts/components/chip-editor-slider.json" } );
}
//-----------------------------------------------------------------------------

GUI_ChipProfileEditor::Panel::Panel ( GUI_ChipProfileEditor& _owner )
	: owner ( _owner )
{
	nameLabel.setName ( "name-label" );
	folderLabel.setName ( "folder-label" );
	nameEdit.setName ( "name" );
	folderEdit.setName ( "folder" );
	capLabel.setName ( "cap-label" );
	cwsLabel.setName ( "cws-label" );
	cwsUltraLabel.setName ( "cws-ultra-label" );
	loopLabel.setName ( "loop-label" );
	loopInfo.setName ( "loop-info" );

	auto	push = [ this ] { owner.pushFromControls (); };

	for ( auto row : { &flt0Dac, &fltGain, &fltSat, &resonance, &waveDC, &extInDC, &bias, &leakage } )
	{
		row->slider.onValueChange = push;
		addAndMakeVisible ( row );
	}

	capOld.onClick = push;
	cwsUltra.onClick = push;

	// Item ids are the ChipProfileSelector level enum plus one (0 is reserved)
	cwsLevel.addItem ( "weak", 1 );
	cwsLevel.addItem ( "average", 2 );
	cwsLevel.addItem ( "strong", 3 );
	cwsLevel.setSelectedId ( 2, juce::dontSendNotification );
	cwsLevel.onChange = push;

	loopSetStart.onClick = [ this ]
	{
		owner.loopStartMS = owner.player.getTimeMS ();

		if ( owner.loopEndMS <= owner.loopStartMS )
			owner.loopEndMS = 0;

		owner.updateLoopLabel ();
	};

	loopSetEnd.onClick = [ this ]
	{
		owner.loopEndMS = owner.player.getTimeMS ();
		owner.updateLoopLabel ();
	};

	loopClear.onClick = [ this ]
	{
		owner.loopStartMS = owner.loopEndMS = 0;
		owner.updateLoopLabel ();
	};

	saveUser.onClick = [ this ] { owner.saveToUserProfile (); };
	saveFactory.onClick = [ this ] { owner.saveToFactoryProfile (); };

	// Only a developer checkout has a factory file to write to
	saveFactory.setEnabled ( buildinfo::isDeveloperMode () );

	for ( auto c : std::initializer_list<juce::Component*> {	&nameLabel, &folderLabel, &nameEdit, &folderEdit,
																&capLabel, &capOld, &cwsLabel, &cwsLevel, &cwsUltraLabel, &cwsUltra,
																&loopLabel, &loopInfo, &loopSetStart, &loopSetEnd, &loopClear,
																&saveUser, &saveFactory,
																&line1, &line2, &line3, &line4, &line5 } )
		addAndMakeVisible ( c );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::Panel::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
								"UI/layouts/components/chip-editor.json" } );
}
//-----------------------------------------------------------------------------

GUI_ChipProfileEditor::GUI_ChipProfileEditor ( SIDPlayer& _player, const juce::File& userCsvFile, std::function<void ()> _onClosed )
	: juce::DocumentWindow ( "Chip profile", juce::Colours::black, juce::DocumentWindow::closeButton )
	, player ( _player )
	, userCsv ( userCsvFile )
	, onClosed ( std::move ( _onClosed ) )
{
	setUsingNativeTitleBar ( true );
	setBackgroundColour ( UI::getShade ( 0.1f ) );

	panel.setSize ( 470, 546 );
	setContentNonOwned ( &panel, true );

	setResizable ( false, false );
	setAlwaysOnTop ( true );
	centreWithSize ( getWidth (), getHeight () );
	setVisible ( true );

	updateLoopLabel ();
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::refresh ( const ChipSettings& s )
{
	current = s;

	panel.nameEdit.setText ( juce::String ( s.name ) );
	panel.folderEdit.setText ( juce::String ( s.folder ) );

	panel.capOld.setToggleState ( s.fltCapOld, juce::dontSendNotification );
	panel.flt0Dac.slider.setValue ( s.flt0Dac, juce::dontSendNotification );
	panel.fltGain.slider.setValue ( s.fltGain, juce::dontSendNotification );
	panel.fltSat.slider.setValue ( s.fltSaturation, juce::dontSendNotification );
	panel.resonance.slider.setValue ( s.fltResonance, juce::dontSendNotification );
	panel.waveDC.slider.setValue ( s.waveDC, juce::dontSendNotification );
	panel.extInDC.slider.setValue ( s.extInDC, juce::dontSendNotification );
	panel.bias.slider.setValue ( s.voiceBias, juce::dontSendNotification );
	panel.leakage.slider.setValue ( s.leakageRate, juce::dontSendNotification );
	panel.cwsLevel.setSelectedId ( std::clamp ( s.cwsLevel, 0, 2 ) + 1, juce::dontSendNotification );
	panel.cwsUltra.setToggleState ( s.cwsSawPulseUltra, juce::dontSendNotification );

	loopStartMS = loopEndMS = 0;
	updateLoopLabel ();

	updateTitle ();

	player.pushLiveProfile ( s );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::pushFromControls ()
{
	player.pushLiveProfile ( settingsFromControls () );
}
//-----------------------------------------------------------------------------

GUI_ChipProfileEditor::ChipSettings GUI_ChipProfileEditor::settingsFromControls () const
{
	auto	s = current;	// keeps approved status and the exceptions

	s.fltCapOld = panel.capOld.getToggleState ();
	s.flt0Dac = panel.flt0Dac.slider.getValue ();
	s.fltGain = panel.fltGain.slider.getValue ();
	s.fltSaturation = panel.fltSat.slider.getValue ();
	s.fltResonance = panel.resonance.slider.getValue ();
	s.waveDC = panel.waveDC.slider.getValue ();
	s.extInDC = panel.extInDC.slider.getValue ();
	s.voiceBias = panel.bias.slider.getValue ();
	s.leakageRate = panel.leakage.slider.getValue ();
	s.cwsLevel = panel.cwsLevel.getSelectedItemIndex ();
	s.cwsSawPulseUltra = panel.cwsUltra.getToggleState ();

	return s;
}
//-----------------------------------------------------------------------------

juce::String GUI_ChipProfileEditor::buildCsvRow () const
{
	const auto	s = settingsFromControls ();

	static const ChipSettings	defaults;

	auto fmt = [] ( const double v )
	{
		auto	str = juce::String ( v, 2 );

		if ( str.containsChar ( '.' ) )
			str = str.trimCharactersAtEnd ( "0" ).trimCharactersAtEnd ( "." );

		return str;
	};

	// Default values stay empty cells, the parser's defaults fill them back in
	auto num = [ &fmt ] ( const double v, const double defaultValue )
	{
		auto	str = fmt ( v );

		return str == fmt ( defaultValue ) ? juce::String () : str;
	};

	auto quoted = [] ( const juce::String& raw )
	{
		if ( raw.containsAnyOf ( ",\"" ) )
			return "\"" + raw.replace ( "\"", "\"\"" ) + "\"";

		return raw;
	};

	static const char* const	cwsNames[] = { "weak", "average", "strong" };

	juce::String	cws;

	if ( s.cwsLevel != defaults.cwsLevel || s.cwsSawPulseUltra )
	{
		cws = cwsNames[ s.cwsLevel ];
		if ( s.cwsSawPulseUltra )
			cws += "+";
	}

	const juce::StringArray	cells = {
		quoted ( juce::String ( s.folder ) ),
		quoted ( juce::String ( s.name ) ),
		s.isApproved ? "approved" : "",
		s.fltCapOld ? "old" : "",
		num ( s.flt0Dac, defaults.flt0Dac ),
		num ( s.fltGain, defaults.fltGain ),
		num ( s.fltSaturation, defaults.fltSaturation ),
		num ( s.fltResonance, defaults.fltResonance ),
		num ( s.waveDC, defaults.waveDC ),
		num ( s.extInDC, defaults.extInDC ),
		num ( s.voiceBias, defaults.voiceBias ),
		cws,
		num ( s.leakageRate, defaults.leakageRate ),
		quoted ( juce::String ( s.exceptionsCsv ) ),
	};

	return cells.joinIntoString ( "," );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::saveToUserProfile ()
{
	// Must match the factory file, both run through the same parser
	static const juce::String	header = "folder,name,status,fltCap,flt0Dac,fltGain,fltSat,resonance,waveDC,extInDC,bias,cwsLevel,leakage,exceptions";

	const auto	name = panel.nameEdit.getText ().trim ();

	if ( name.isEmpty () || userCsv == juce::File () )
		return;

	// The user-folder watcher picks the write up and hot-reloads the profiles
	upsertCsvRow ( userCsv, name, header );

	updateTitle ();
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::saveToFactoryProfile ()
{
	const auto	name = panel.nameEdit.getText ().trim ();
	const auto	factory = datasource::getDevFile ( "Databases/chip-profiles.csv" );

	if ( name.isEmpty () || ! factory.existsAsFile () )
		return;

	// The dev-root watcher reloads the profiles, replaying factory-then-user;
	// the now-redundant user row leaves with the same write cycle
	if ( upsertCsvRow ( factory, name, {} ) )
		removeUserProfileRow ( name );

	updateTitle ();
}
//-----------------------------------------------------------------------------

bool GUI_ChipProfileEditor::upsertCsvRow ( const juce::File& csv, const juce::String& name, const juce::String& headerIfEmpty )
{
	auto	lines = loadCsvLines ( csv, headerIfEmpty );

	// Older files hold the wave offset in a 'digi' column
	if ( ! lines.isEmpty () )
		lines.set ( 0, lines[ 0 ].replace ( "digi", "waveDC" ) );

	// Legacy 'fltBpw' (0 = full resonance) is today's 'resonance' (1 = full):
	// rename the header cell and invert the value in every data row
	if ( ! lines.isEmpty () && lines[ 0 ].contains ( "fltBpw" ) )
	{
		lines.set ( 0, lines[ 0 ].replace ( "fltBpw", "resonance" ) );

		for ( auto i = 1; i < lines.size (); ++i )
		{
			int	start = 0, length = 0;
			if ( ! csvCellRange ( lines[ i ], 7, start, length ) || length == 0 )
				continue;

			const auto	inverted = 1.0 - lines[ i ].substring ( start, start + length ).getDoubleValue ();
			const auto	cell = juce::String ( inverted, 2 ).trimCharactersAtEnd ( "0" ).trimCharactersAtEnd ( "." );

			lines.set ( i, lines[ i ].replaceSection ( start, length, cell ) );
		}
	}

	// Older files get the missing columns inserted behind 'waveDC' (header name
	// plus an empty cell per data row), so the trailing columns stay aligned
	struct NewColumn final { const char* name; int cellIdx; };

	for ( const auto& col : { NewColumn { "extInDC", 9 }, NewColumn { "bias", 10 } } )
		if ( ! lines.isEmpty () && ! lines[ 0 ].contains ( col.name ) )
			for ( auto i = 0; i < lines.size (); ++i )
				lines.set ( i, insertCsvCell ( lines[ i ], col.cellIdx, i == 0 ? col.name : "" ) );

	if ( const auto row = findRowByName ( lines, name ); row > 0 )
		lines.set ( row, buildCsvRow () );
	else
		lines.add ( buildCsvRow () );

	return fileutils::replaceFile ( csv, lines.joinIntoString ( "\n" ) + "\n" );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::removeUserProfileRow ( const juce::String& name )
{
	if ( ! userCsv.existsAsFile () )
		return;

	auto	lines = loadCsvLines ( userCsv );

	const auto	row = findRowByName ( lines, name );
	if ( row < 0 )
		return;

	lines.remove ( row );

	// An overlay holding no rows only shadows the factory file, delete it
	if ( lines.size () <= 1 )
		userCsv.deleteFile ();
	else
		fileutils::replaceFile ( userCsv, lines.joinIntoString ( "\n" ) + "\n" );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::updateTitle ()
{
	const auto	title = strings->get ( "chip-editor/title" ) + ": ";

	if ( current.name.empty () )
	{
		setName ( title + "-" );
		return;
	}

	// A user row shadows the factory one whole, so its presence decides
	juce::StringArray	lines;
	if ( userCsv.existsAsFile () )
		lines.addLines ( userCsv.loadFileAsString () );

	const auto	source = findRowByName ( lines, juce::String ( current.name ) ) > 0 ? " [user]" : " [factory]";

	setName ( title + juce::String ( current.name ) + source );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileEditor::updateLoopLabel ()
{
	if ( ! loopStartMS && ! loopEndMS )
	{
		panel.loopInfo.setText ( strings->get ( "chip-editor/no_loop" ) );
		return;
	}

	auto fmt = [] ( const uint32_t ms )
	{
		return juce::String::formatted ( "%u:%02u", ms / 60000u, ( ms / 1000u ) % 60u );
	};

	panel.loopInfo.setText ( fmt ( loopStartMS ) + " - " + fmt ( loopEndMS ) );
}
//-----------------------------------------------------------------------------
