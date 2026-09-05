#include "UndoManager.h"

//-----------------------------------------------------------------------------

UndoManager::~UndoManager ()
{
	// The UI may already be gone at teardown, commit without it
	onShow = nullptr;
	onHide = nullptr;

	flush ();
}
//-----------------------------------------------------------------------------

void UndoManager::arm ( Op op )
{
	flush ();

	current = std::move ( op );

	if ( onShow )
		onShow ( current->text );
}
//-----------------------------------------------------------------------------

void UndoManager::undo ()
{
	if ( ! current )
		return;

	// Taken out of the slot first, the callback may arm a new op
	auto	op = std::move ( *current );
	current.reset ();

	if ( op.undo )
		op.undo ();

	if ( onHide )
		onHide ();
}
//-----------------------------------------------------------------------------

void UndoManager::flush ()
{
	if ( ! current )
		return;

	// Taken out of the slot first, the callback may arm a new op
	auto	op = std::move ( *current );
	current.reset ();

	if ( op.commit )
		op.commit ();

	if ( onHide )
		onHide ();
}
//-----------------------------------------------------------------------------
