
#include "tjsCommHead.h"
#include "Application.h"
#include "NativeEventQueue.h"


void NativeEventQueueImplement::HandlerDefault( NativeEvent& event ) {
}
void NativeEventQueueImplement::Allocate() {
	Application->addEventHandler(this);
}
void NativeEventQueueImplement::Deallocate() {
	Application->removeEventHandler(this);
}
void NativeEventQueueImplement::PostEvent( const NativeEvent& event ) {
	Application->postEvent( &event, this );
}

