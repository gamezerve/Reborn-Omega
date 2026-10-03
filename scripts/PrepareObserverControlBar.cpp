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
 place("InputBlockRight","UPPERLEFT: 1642 887, BOTTOMRIGHT: 1871 1071");
 // Reborn: Keep selected-unit information at the right edge instead of retail observer's center slot.
 place("WinUnitSelected","UPPERLEFT: 1642 887, BOTTOMRIGHT: 1871 1071");
 // Reborn: Hover searches enter the HUD parent first, so its bounds must contain the relocated portrait and upgrades.
 place("RightHUD","UPPERLEFT: 1642 887, BOTTOMRIGHT: 1871 1071");
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
