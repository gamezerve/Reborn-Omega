// Reborn: Prepare the shipped observer skin offline without altering existing player ControlBarPro layouts.
#define main PreparePlayerControlBarsMain
#include "PrepareCustomControlBars.cpp"
#undef main
#include <functional>
#include <cstring>

//-------------------------------------------------------------------------------------------------
/** Reborn: Parse one canonical control-bar tree for a pointer-stable observer appearance. */
//-------------------------------------------------------------------------------------------------
CustomWndNode tree(const std::string& text) {
 auto ts=statements(text); size_t p=0; while(p<ts.size()&&ts[p]!="WINDOW")++p;
 return readNode(ts,p);
}
//-------------------------------------------------------------------------------------------------
/** Reborn: Record drawing flags only; never overwrite gameplay visibility, enabled state or input flags. */
//-------------------------------------------------------------------------------------------------
void drawingFlags(std::ostream& out,const CustomWndNode& n) {
 const auto f=n.fields.find("NAME"), s=n.fields.find("STATUS");
 if(f!=n.fields.end()&&s!=n.fields.end()) {
  const auto a=f->second.find('"'),b=f->second.find_last_of('"');
  out<<"S "<<f->second.substr(a+1,b-a-1)<<" "<<(s->second.find("IMAGE")!=std::string::npos)<<" "<<(s->second.find("SEE_THRU")!=std::string::npos)<<"\n";
 }
 for(const auto& c:n.children)drawingFlags(out,c);
}
//-------------------------------------------------------------------------------------------------
/** Reborn: Namespace only the atlas overridden by the observer package, leaving normal USA art untouched. */
//-------------------------------------------------------------------------------------------------
std::string renameObserver(const std::string& text,const std::map<std::string,std::string>& names) {
 const std::regex token("[A-Za-z0-9_][A-Za-z0-9_.-]*"); std::string result;size_t last=0;
 for(std::sregex_iterator i(text.begin(),text.end(),token),end;i!=end;++i) {
  result+=text.substr(last,i->position()-last);auto f=names.find(lookupKey(i->str().c_str()));
  result+=f==names.end()?i->str():f->second;last=i->position()+i->length();
 }
 return result+text.substr(last);
}
//-------------------------------------------------------------------------------------------------
/** Reborn: Build observer WNDs/manifests and isolated art, preserving every original mod control identity. */
//-------------------------------------------------------------------------------------------------
int main(int argc,char**argv) {
 try {
 if(argc!=3)throw std::runtime_error("Usage: PrepareObserverControlBar <repository-root> <staging-output-directory>");
 const std::string repo=argv[1], dest=argv[2],shared=repo+"/build/shared/";
 Files files;size_t total=0;
 for(const char* name:{"200_ControlBarObsBaseZH.big","200_ControlBarObsEnglishZH.big"}) {
  auto s=readText(shared+"RebornOmegaData/CustomControlBar/ControlBarObsEnglishZH_v1.5/"+name);
  readBig(Bytes(s.begin(),s.end()),files,total);
 }
 std::map<std::string,std::string> images;
 const std::string atlas=readText(shared+"Data/INI/MappedImages/TextureSize_512/SAControlBar512.INI");
 const std::regex mapped("MappedImage[ \\t]+([^ \\t\\r\\n]+)",std::regex::icase);
 for(std::sregex_iterator i(atlas.begin(),atlas.end(),mapped),end;i!=end;++i)images[lookupKey((*i)[1].str().c_str())]="RebornCBO_"+(*i)[1].str();
 images["sacontrolbar512_001.tga"]="RebornCBO_SAControlBar512_001.tga";
 output(dest+"/Art/Textures/RebornCBO_SAControlBar512_001.dds",files.at("art/textures/sacontrolbar512_001.dds"));
 auto central=readText(shared+"Data/INI/MappedImages/HandCreated/HandCreatedMappedImages.INI");
 const std::string begin="; Reborn: Begin built-in Observer Control Bar mapped images",end="; Reborn: End built-in Observer Control Bar mapped images";
 auto at=central.find(begin);if(at!=std::string::npos){auto stop=central.find(end,at);if(stop==std::string::npos)throw std::runtime_error("Unterminated observer images");central.erase(at,stop+end.size()-at);}
 output(dest+"/Data/INI/MappedImages/HandCreated/HandCreatedMappedImages.INI",central+"\n"+begin+"\n"+renameObserver(atlas,images)+"\n"+end+"\n");
 const auto& wnd=files.at("window/controlbar.wnd");
 auto retail=tree(renameObserver(std::string(wnd.begin(),wnd.end()),images));
 auto normalText=readText(shared+"Window/ControlBar.wnd");auto normalTree=tree(normalText);
 auto observer=normalTree;skinNode(observer,retail);
 // Reborn: Observer children extend beyond retail's narrow root; preserve clickable parents without old player walls.
 auto place=[&](const char* name,const char* bounds){auto n=findNode(observer,name);if(n)n->fields["SCREENRECT"]=std::string("SCREENRECT = ")+bounds+", CREATIONRESOLUTION: 1920 1080";};
 place("ControlBarParent","UPPERLEFT: 0 716, BOTTOMRIGHT: 1920 1080");
 place("CenterBackground","UPPERLEFT: 368 716, BOTTOMRIGHT: 1920 1080");
 place("InputBlockLeft","UPPERLEFT: 4 716, BOTTOMRIGHT: 364 1078");
 place("InputBlockMain","UPPERLEFT: 368 870, BOTTOMRIGHT: 690 1076");
 place("InputBlockRight","UPPERLEFT: 1634 879, BOTTOMRIGHT: 1879 1079");
 // Reborn: Keep selected-unit information at the right edge instead of retail observer's center slot.
 place("WinUnitSelected","UPPERLEFT: 1642 887, BOTTOMRIGHT: 1871 1071");
 // Reborn: Expand the clickable parent around the eight-pixel observer metal frame.
 place("RightHUD","UPPERLEFT: 1634 879, BOTTOMRIGHT: 1879 1079");
 // Reborn: Compact observer rows need scaled small fonts rather than the normal bar's two-line headings.
 for(const char* name:{"StaticTextObsBuildings","StaticTextObsUnits","StaticTextObsUnitsLost","StaticTextObsUnitsKilled","StaticTextNumberOfUnitsLost","StaticTextNumberOfUnitsKilled","StaticTextNumberOfBuildings","StaticTextNumberOfUnits"}) {
  auto n=findNode(observer,name);if(n){n->fields["FONT"]="FONT = NAME: \"Arial\", SIZE: 8, BOLD: 0";n->fields["HEADERTEMPLATE"]="HEADERTEMPLATE = \"LabelSmall\"";}
 }
 if(auto n=findNode(observer,"StaticTextPlayerName")) {
  n->fields["FONT"]="FONT = NAME: \"Arial\", SIZE: 10, BOLD: 0";
  n->fields["HEADERTEMPLATE"]="HEADERTEMPLATE = \"LabelRegular\"";
 }
 // Reborn: Synchronize atlas/color rendering with the source skin, but retain all mod callbacks and controls.
 std::function<void(CustomWndNode&)> flags=[&](CustomWndNode& n){auto src=findNode(retail,nodeName(n));if(src){auto& s=n.fields["STATUS"];for(const char* flag:{"IMAGE","SEE_THRU"}){const auto p=s.find(std::string("+")+flag);if(p!=std::string::npos)s.erase(p,strlen(flag)+1);if(src->fields["STATUS"].find(flag)!=std::string::npos)s+=std::string("+")+flag;}}for(auto& c:n.children)flags(c);};
 flags(observer);layoutUpgradeCameos(observer);
 // Reborn: Retail hides these labels and uses tooltips on player buttons; retain useful visible names beside them.
 for(int i=0;i<8;++i){auto n=findNode(observer,"StaticTextPlayer"+std::to_string(i));if(n){n->fields["SCREENRECT"]="SCREENRECT = UPPERLEFT: 408 "+std::to_string(718+i*39)+", BOTTOMRIGHT: 684 "+std::to_string(754+i*39)+", CREATIONRESOLUTION: 1920 1080";auto& s=n->fields["STATUS"];auto p=s.find("+SEE_THRU");if(p!=std::string::npos)s.erase(p,9);}}
 // Reborn: Fit the existing controls into the original Pro observer shell.
 place("LeftHUD","UPPERLEFT: 5 753, BOTTOMRIGHT: 327 1075");
 place("InputBlockLeft","UPPERLEFT: 0 740, BOTTOMRIGHT: 394 1080");
 place("RightHUD","UPPERLEFT: 1642 887, BOTTOMRIGHT: 1871 1071");
 place("InputBlockRight","UPPERLEFT: 1625 870, BOTTOMRIGHT: 1920 1080");
 place("BackgroundMarker","UPPERLEFT: 0 0, BOTTOMRIGHT: 1920 1080");
 place("ObserverPlayerListWindow","UPPERLEFT: 332 740, BOTTOMRIGHT: 684 1010");
 place("ButtonPlayer0","UPPERLEFT: 334 755, BOTTOMRIGHT: 379 785");
 place("StaticTextPlayer0","UPPERLEFT: 400 755, BOTTOMRIGHT: 684 785");
 place("ButtonPlayer1","UPPERLEFT: 334 787, BOTTOMRIGHT: 379 817");
 place("StaticTextPlayer1","UPPERLEFT: 400 787, BOTTOMRIGHT: 684 817");
 place("ButtonPlayer2","UPPERLEFT: 334 819, BOTTOMRIGHT: 379 849");
 place("StaticTextPlayer2","UPPERLEFT: 400 819, BOTTOMRIGHT: 684 849");
 place("ButtonPlayer3","UPPERLEFT: 334 851, BOTTOMRIGHT: 379 881");
 place("StaticTextPlayer3","UPPERLEFT: 400 851, BOTTOMRIGHT: 684 881");
 place("ButtonPlayer4","UPPERLEFT: 334 883, BOTTOMRIGHT: 379 913");
 place("StaticTextPlayer4","UPPERLEFT: 400 883, BOTTOMRIGHT: 684 913");
 place("ButtonPlayer5","UPPERLEFT: 334 915, BOTTOMRIGHT: 379 945");
 place("StaticTextPlayer5","UPPERLEFT: 400 915, BOTTOMRIGHT: 684 945");
 place("ButtonPlayer6","UPPERLEFT: 334 947, BOTTOMRIGHT: 379 977");
 place("StaticTextPlayer6","UPPERLEFT: 400 947, BOTTOMRIGHT: 684 977");
 place("ButtonPlayer7","UPPERLEFT: 334 979, BOTTOMRIGHT: 379 1009");
 place("StaticTextPlayer7","UPPERLEFT: 400 979, BOTTOMRIGHT: 684 1009");
 // Reborn: Parent bounds contain every player button; stats and experience align with the Pro artwork.
 place("CenterBackground","UPPERLEFT: 332 716, BOTTOMRIGHT: 1920 1080");
 place("GeneralsExp","UPPERLEFT: 1880 923, BOTTOMRIGHT: 1906 1063");
 place("ExpBarForeground","UPPERLEFT: 1875 887, BOTTOMRIGHT: 1911 1071");
 // Reborn: The experience foreground is part of the original Pro observer panel.
 // Reborn: SEE_THRU suppresses drawing; match the normal Pro foreground over the progress fill.
 if(auto exp=findNode(observer,"ExpBarForeground")) {
  auto& status=exp->fields["STATUS"];
  status=std::regex_replace(status,std::regex("\\+SEE_THRU"),"");
  status+="+IMAGE";
 }
 // Reborn: Move the entire player card 32 reference pixels beyond the observer sidebar.
 for(const char* name:{"WinFlag","ButtonCancel","PowerWindow","StaticTextPlayerName","StaticTextObsBuildings",
     "StaticTextObsUnits","StaticTextObsUnitsLost","StaticTextObsUnitsKilled",
     "StaticTextNumberOfBuildings","StaticTextNumberOfUnits","StaticTextNumberOfUnitsLost",
     "StaticTextNumberOfUnitsKilled"}) {
  auto node=findNode(observer,name);
  if(!node)throw std::runtime_error(std::string("Missing observer card control ")+name);
  const auto bounds=rect(*node);
  place(name,("UPPERLEFT: "+std::to_string(bounds[0]+32)+" "+std::to_string(bounds[1])+
      ", BOTTOMRIGHT: "+std::to_string(bounds[2]+32)+" "+std::to_string(bounds[3])).c_str());
 }
 // Reborn: Align Back with the black backing drawn two pixels outside the player card.
 place("ButtonCancel","UPPERLEFT: 398 1030, BOTTOMRIGHT: 488 1076");
 std::ostringstream obs;manifestNode(obs,observer);
 output(dest+"/Window/CustomControlBar/Observer/ControlBarObserverCustom.wnd","FILE_VERSION = 2;\nSTARTLAYOUTBLOCK\nLAYOUTINIT = [None];\nLAYOUTUPDATE = [None];\nLAYOUTSHUTDOWN = [None];\nENDLAYOUTBLOCK\n"+writeNode(observer));
 output(dest+"/Data/INI/CustomControlBar/ObserverAppearance.txt",obs.str());
 const auto& schemeBytes=files.at("data/ini/controlbarscheme.ini");std::string schemes(schemeBytes.begin(),schemeBytes.end());
 const size_t first=schemes.find("ControlBarScheme Observer8x6"),last=schemes.find("ControlBarScheme ",first+1);
 std::string scheme=renameObserver(schemes.substr(first,last-first),images);
 // Reborn: Observer art is window-drawn; discard the inherited player shell rather than drawing a stale faction bar.
 auto ip=scheme.find("  ImagePart");auto ipEnd=scheme.find("  End",ip);scheme.erase(ip,ipEnd+5-ip);
 // Reborn: Keep the mod's observer idle-worker action beside, rather than on top of, the return button.
 scheme=std::regex_replace(scheme,std::regex("WorkerUL X:[0-9]+ Y:[0-9]+"),"WorkerUL X:502 Y:1030");
 scheme=std::regex_replace(scheme,std::regex("WorkerLR X:[0-9]+ Y:[0-9]+"),"WorkerLR X:538 Y:1066");
 // Reborn: Keep the original Pro observer button strip aligned with its command-bar atlas.
 scheme=std::regex_replace(scheme,std::regex("OptionsUL X:[0-9]+ Y:[0-9]+"),"OptionsUL X:334 Y:1011");
 scheme=std::regex_replace(scheme,std::regex("OptionsLR X:[0-9]+ Y:[0-9]+"),"OptionsLR X:379 Y:1041");
 scheme=std::regex_replace(scheme,std::regex("ChatUL X:[0-9]+ Y:[0-9]+"),"ChatUL X:334 Y:1043");
 scheme=std::regex_replace(scheme,std::regex("ChatLR X:[0-9]+ Y:[0-9]+"),"ChatLR X:379 Y:1073");
 for(const auto& button:std::vector<std::pair<std::string,std::string>>{{"Options","Options"},{"Buddy","Chat"}})
  for(const auto& state:std::vector<std::pair<std::string,std::string>>{{"Enable",""},{"Hightlited","H"},{"Pushed","P"},{"Disabled","I"}})
   scheme=std::regex_replace(scheme,std::regex("("+button.first+"Button"+state.first+"[ \t]+)RebornCBO_[A-Za-z0-9_]+"),"$1RebornCBP_1080_ObserverPro"+button.second+state.second);
 // Reborn: Restore the visible Pro experience-bar foreground.
 scheme=std::regex_replace(scheme,std::regex("ExpBarForegroundImage [A-Za-z0-9_]+"),"ExpBarForegroundImage RebornCBP_1080_AmericaProExpBar");
 // Reborn: Shift the observer power meter with the player card.
 scheme=std::regex_replace(scheme,std::regex("PowerBarUL X:[0-9]+ Y:[0-9]+"),"PowerBarUL X:400 Y:854");
 scheme=std::regex_replace(scheme,std::regex("PowerBarLR X:[0-9]+ Y:[0-9]+"),"PowerBarLR X:720 Y:866");
 output(dest+"/Data/INI/CustomControlBar/ObserverScheme.ini",scheme);
 // Reborn: Derive flag snapshots from current files, preserving all player layout edits.
 auto addFlags=[&](const std::string& text,const CustomWndNode& n){std::istringstream in(text);std::ostringstream out;std::string line;while(std::getline(in,line)){if(line.compare(0,2,"S ")!=0)out<<line<<"\n";}drawingFlags(out,n);return out.str();};
 output(dest+"/Data/INI/CustomControlBar/NormalAppearance.txt",addFlags(readText(shared+"Data/INI/CustomControlBar/NormalAppearance.txt"),normalTree));
 for(const char* res:{"1280x720","1600x900","1920x1080","2560x1440","3840x2160"}) {
  std::string p=std::string("Data/INI/CustomControlBar/")+res+"/Appearance.txt";
  output(dest+"/"+p,addFlags(readText(shared+p),tree(readText(shared+"Window/CustomControlBar/"+res+"/ControlBarCustom.wnd"))));
 }
 if(namesOf(normalText)!=namesOf(writeNode(observer)))throw std::runtime_error("Observer identities changed");
 std::cout<<"PASS: Observer controls retain all mod identities; isolated atlas and reversible flag manifests ready\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}return 0;
}
