#include <cassert>
#include <climits>
#include "composer.h"
#include "encoder.h"
#include "config.h"
#include "message_view.h"

int main() {
  Adafruit_SH1106G display;
  Composer composer(display);
  String submitted;
  assert(!composer.submit(submitted));
  composer.begin();
  assert(!composer.submit(submitted));
  for(int length=0; length<=160; ++length) {
    composer.render();
    assert(display.cursorX==2+(length%20)*6);
    assert(display.cursorY==22+min(length/20,3)*10);
    composer.append('a');
  }
  assert(composer.submit(submitted));
  assert(submitted.length()==160);
  assert(!composer.active());
  composer.begin();
  composer.append('a');
  composer.backspace();
  assert(!composer.submit(submitted));
  composer.append('\n');
  assert(!composer.submit(submitted));
  composer.cancel();
  assert(!composer.active());

  MessageStore store;
  for(uint32_t id=0;id<18;++id) store.add({id,"sender","body",0,true,true});
  assert(store.size()==16 && store.at(0)->id==2 && store.at(15)->id==17);
  assert(store.at(16)==nullptr && store.unreadCount()==16);
  MessageView view(display);
  fakeMillis=100;
  view.begin(store);
  view.moveSelection(-3,store);
  assert(view.selectedIndex()==12);
  view.moveSelection(INT_MIN,store);
  assert(view.selectedIndex()==0);
  view.moveSelection(INT_MAX,store);
  assert(view.selectedIndex()==15);
  fakeMillis=999;
  view.updateReadState(store);
  assert(store.at(15)->unread);
  // Returning from the composer starts a fresh visible dwell period.
  fakeMillis=5000;
  view.resetDwell();
  view.updateReadState(store);
  assert(store.at(15)->unread);
  fakeMillis=5900;
  view.updateReadState(store);
  assert(!store.at(15)->unread && store.unreadCount()==15);
  view.moveSelection(-1,store);
  fakeMillis=UINT32_MAX-400;
  view.resetDwell();
  fakeMillis=499;
  view.updateReadState(store);
  assert(!store.at(14)->unread);

  Encoder encoder;
  fakePins[PagerConfig::ENCODER_CLK]=HIGH;
  fakePins[PagerConfig::ENCODER_SW]=HIGH;
  fakeMillis=1000;
  encoder.begin();
  fakeMillis=1100;
  fakePins[PagerConfig::ENCODER_SW]=LOW;
  encoder.update();
  assert(!encoder.consumeClick());
  fakeMillis=1105;
  fakePins[PagerConfig::ENCODER_SW]=HIGH;
  encoder.update();
  fakeMillis=1110;
  fakePins[PagerConfig::ENCODER_SW]=LOW;
  encoder.update();
  fakeMillis=1144;
  encoder.update();
  assert(!encoder.consumeClick());
  fakeMillis=1145;
  encoder.update();
  assert(encoder.consumeClick());
  fakeMillis=1200;
  encoder.update();
  assert(!encoder.consumeClick());
}
