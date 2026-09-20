#include "composer.h"

Composer::Composer(Adafruit_SH1106G& display):display_(display){}

void Composer::begin(){text_="";active_=true;}
void Composer::cancel(){text_="";active_=false;}

void Composer::append(char c){
  if(!active_ || text_.length()>=kMaxLength || c<32 || c>126)return;
  text_+=c;
}

void Composer::backspace(){
  if(active_ && text_.length())text_.remove(text_.length()-1);
}

bool Composer::submit(String& text){
  if(!active_ || !text_.length())return false;
  text=text_;
  text_="";
  active_=false;
  return true;
}

void Composer::drawWrappedText(){
  constexpr int charsPerLine=20;
  constexpr int visibleLines=4;
  // Include the insertion position, including a new row at exact line boundaries.
  const int totalLines=(int)(text_.length()/charsPerLine)+1;
  const int firstLine=max(0,totalLines-visibleLines);
  for(int row=0;row<visibleLines;++row){
    const int line=firstLine+row;
    const int start=line*charsPerLine;
    if(start>=(int)text_.length())break;
    display_.setCursor(2,14+row*10);
    display_.print(text_.substring(start,min(start+charsPerLine,(int)text_.length())));
  }
  int cursorIndex=text_.length()%charsPerLine;
  int cursorLine=min(visibleLines-1,totalLines-1-firstLine);
  display_.drawFastHLine(2+cursorIndex*6,22+cursorLine*10,5,SH110X_WHITE);
}

void Composer::render(){
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setTextColor(SH110X_WHITE);
  display_.setCursor(0,0);
  display_.print("NEW MESSAGE");
  display_.setCursor(104,0);
  display_.print(text_.length());
  display_.drawLine(0,9,127,9,SH110X_WHITE);
  drawWrappedText();
  display_.drawLine(0,55,127,55,SH110X_WHITE);
  display_.setCursor(0,57);
  display_.print("ENTER:SEND  ESC:BACK");
  display_.display();
}
