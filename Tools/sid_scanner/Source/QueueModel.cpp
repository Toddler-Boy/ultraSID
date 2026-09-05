#include "QueueModel.h"

#include "CustomLookAndFeel.h"
#include "sid_scanner.h"

//-----------------------------------------------------------------------------

QueueModel::QueueModel ( const ProcessingThread& threadIn, const std::vector<const ProcessingThread::QueueEntry*>* entryListIn )
	: processingThread ( threadIn ),
	  entryList ( entryListIn )
{
}
//-----------------------------------------------------------------------------

void QueueModel::setEntryList ( const std::vector<const ProcessingThread::QueueEntry*>* entryListIn )
{
	entryList = entryListIn;
}
//-----------------------------------------------------------------------------

void QueueModel::attachTo ( juce::TableListBox& tableIn )
{
	table = &tableIn;
	table->setModel ( this );

	// Visible only, not sortable, not resizable; File takes whatever width the
	// two fixed columns leave over
	constexpr auto	columnFlags = juce::TableHeaderComponent::visible;

	auto&	header = table->getHeader ();
	header.addColumn ( "File", fileColumn, 300, 100, -1, columnFlags );
	header.addColumn ( "Status", statusColumn, 56, 56, 56, columnFlags );
	header.addColumn ( "Len", lengthColumn, 40, 40, 40, columnFlags );
	header.addColumn ( "Progress", progressColumn, 100, 100, 100, columnFlags );
	header.setStretchToFitActive ( true );

	// Tighter rows than the default, which leaves unnecessary gaps (the font
	// size is unaffected, cells just get less vertical padding)
	table->setRowHeight ( juce::roundToInt ( float ( table->getRowHeight () ) * 0.8f ) );

	// Double-width scrollbar for easier grabbing, always visible so the tables
	// don't change width when the content starts to overflow
	if ( auto* viewport = table->getViewport () )
	{
		viewport->setScrollBarThickness ( table->getLookAndFeel ().getDefaultScrollbarWidth () * 2 );
		viewport->getVerticalScrollBar ().setAutoHide ( false );
	}
}
//-----------------------------------------------------------------------------

juce::LookAndFeel& QueueModel::getLookAndFeel () const
{
	return table != nullptr ? table->getLookAndFeel () : juce::LookAndFeel::getDefaultLookAndFeel ();
}
//-----------------------------------------------------------------------------

int QueueModel::getNumRows ()
{
	return entryList != nullptr ? static_cast<int> ( entryList->size () ) : processingThread.getNumQueueEntries ();
}
//-----------------------------------------------------------------------------

const ProcessingThread::QueueEntry* QueueModel::getEntry ( const int rowNumber ) const
{
	if ( entryList != nullptr )
		return juce::isPositiveAndBelow ( rowNumber, static_cast<int> ( entryList->size () ) ) ? ( *entryList )[ static_cast<size_t> ( rowNumber ) ] : nullptr;

	return processingThread.getQueueEntry ( rowNumber );
}
//-----------------------------------------------------------------------------

void QueueModel::paintRowBackground ( juce::Graphics& g, int /*rowNumber*/, int /*width*/, int /*height*/, const bool rowIsSelected )
{
	if ( rowIsSelected )
		g.fillAll ( getShade ( 0.2f ) );
}
//-----------------------------------------------------------------------------

void QueueModel::paintCell ( juce::Graphics& g, const int rowNumber, const int columnId, const int width, const int height, bool /*rowIsSelected*/ )
{
	const auto*	entry = getEntry ( rowNumber );
	if ( entry == nullptr )
		return;

	const auto	state = entry->state.load ();

	auto	textColour = getLookAndFeel ().findColour ( juce::ListBox::textColourId );
	if ( state == ProcessingThread::EntryState::failed )
		textColour = juce::Colours::orangered;
	else if ( state == ProcessingThread::EntryState::done )
		textColour = textColour.withAlpha ( 0.5f );

	g.setColour ( textColour );
	g.setFont ( juce::Font ( juce::FontOptions ( 13.0f ) ) );

	const auto	area = juce::Rectangle<int> ( 0, 0, width, height ).reduced ( 4, 0 );

	auto formatTime = [] ( const uint32_t ms )
	{
		const auto	totalSecs = int ( ms / 1000 );
		return juce::String::formatted ( "%d:%02d", totalSecs / 60, totalSecs % 60 );
	};

	switch ( columnId )
	{
		case fileColumn:
			g.drawText ( juce::String ( entry->name ) + " #" + juce::String ( entry->tuneNo ), area, juce::Justification::centredLeft, true );
			break;

		case statusColumn:
		{
			// Four detection dots, dimmed until the feature is detected; they light
			// up live while a tune renders: teal = filter, green = digis,
			// red = one-shot, yellow = delayed start
			const auto	features = entry->features.load ();

			const struct { juce::Colour colour; uint8_t flag; } dots[] = {
				{ juce::Colour ( 0xff66ffff ),	MeasureLoudness::featFilter },
				{ juce::Colour ( 0xff66ff99 ),	MeasureLoudness::featDigi },
				{ juce::Colour ( 0xffff3636 ),	MeasureLoudness::featOneShot },
				{ juce::Colour ( 0xffffd432 ),	MeasureLoudness::featDelayedStart },
			};

			constexpr auto	diameter = 8.0f;
			constexpr auto	gap = 4.0f;

			auto		x = ( float ( width ) - ( 4.0f * diameter + 3.0f * gap ) ) * 0.5f;
			const auto	y = ( float ( height ) - diameter ) * 0.5f;

			for ( const auto& dot : dots )
			{
				g.setColour ( dot.colour.withAlpha ( ( features & dot.flag ) != 0 ? 1.0f : 0.2f ) );
				g.fillEllipse ( x, y, diameter, diameter );
				x += diameter + gap;
			}
			break;
		}

		case lengthColumn:
			g.drawText ( formatTime ( entry->lengthMS ), area, juce::Justification::centredRight, false );
			break;

		case progressColumn:
		{
			if ( state == ProcessingThread::EntryState::pending )
			{
				g.drawText ( "queued", area, juce::Justification::centred, false );
				break;
			}

			if ( state == ProcessingThread::EntryState::failed )
			{
				g.drawText ( entry->error.empty () ? juce::String ( "failed" ) : juce::String ( entry->error ), area, juce::Justification::centred, true );
				break;
			}

			const auto	fraction = state == ProcessingThread::EntryState::done
				? 1.0f
				: ( entry->lengthMS ? std::min ( 1.0f, float ( entry->renderedMS.load () ) / float ( entry->lengthMS ) ) : 0.0f );

			// Clipping to the rounded outline and filling plain rects keeps the
			// corners intact even when the fill is narrower than the corner radius
			auto	barArea = juce::Rectangle<float> ( float ( width ), float ( height ) ).reduced ( 2.0f ).withTrimmedRight ( 4.0f );

			if ( barArea != progressBarPath.getBounds () )
			{
				progressBarPath.clear ();
				progressBarPath.addRoundedRectangle ( barArea, barArea.getHeight () / 2.0f );
			}

			{
				const juce::Graphics::ScopedSaveState	saveState ( g );

				g.reduceClipRegion ( progressBarPath );

				// Draw the filled portion of the bar, clipped to the rounded outline
				g.setColour ( getLookAndFeel ().findColour ( juce::ProgressBar::foregroundColourId ).withMultipliedAlpha ( state == ProcessingThread::EntryState::done ? 0.5f : 1.0f ) );
				g.fillRect ( barArea.removeFromLeft ( barArea.getWidth () * fraction ) );

				// Track pinned to a fixed palette shade, identical on any background
				if ( barArea.getWidth () > 0.0f )
				{
					g.setColour ( getShade ( 0.04f ) );
					g.fillRect ( barArea );
				}
			}

			g.setColour ( textColour );
			g.drawText ( juce::String ( int ( fraction * 100.0f ) ) + "%", area, juce::Justification::centred, false );
			break;
		}
	}
}
//-----------------------------------------------------------------------------
