/*************************************************************** -*- c++ -*-
 *       Copyright (c) 2003,2004 by Marcel Wiesweg                         *
 *       (autogenerated code (c) Klaus Schmidinger)
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <vdr/plugin.h>
#include <vdr/keys.h>
#include <vdr/config.h>

#include <getopt.h>
#include <iostream>

using namespace std;

#include "menu.h"
#include "txtrecv.h"
#include "setup.h"
#include "legacystorage.h"
#include "packedstorage.h"

#if defined(APIVERSNUM) && APIVERSNUM < 10739
#error "VDR-1.7.39 API version or greater is required!"
#endif

#define NUMELEMENTS(x) (sizeof(x) / sizeof(x[0]))

static const char *VERSION        = "1.0.6.1";
static const char *DESCRIPTION    = trNOOP("Displays teletext on the OSD");
static const char *MAINMENUENTRY  = trNOOP("Teletext");

int m_debugmask = 0;

class cPluginTeletextosd : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  cTxtStatus *txtStatus;
  bool startReceiver;
  bool storeTopText;
  Storage *storage;
  int maxStorage;
  void initTexts();
  Storage::StorageSystem storageSystem;
public:
  cPluginTeletextosd(void);
  virtual ~cPluginTeletextosd();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void);
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
};

class cTeletextSetupPage;
class ActionEdit {
   public:
      void Init(cTeletextSetupPage*, int, cMenuEditIntItem  *, cMenuEditStraItem *);
      cMenuEditStraItem *action;
      cMenuEditIntItem  *number;
      bool visible;
   };

struct ActionKeyName {
   const char *internalName;
   const char *userName;
};

class cTeletextSetupPage : public cMenuSetupPage {
friend class ActionEdit;
private:
   TeletextSetup temp;
   int tempPageNumber[LastActionKey];
   int tempConfiguredClrBackground; //must be a signed int
protected:
   virtual void Store(void);
   ActionEdit ActionEdits[LastActionKey];
   virtual eOSState ProcessKey(eKeys Key);
public:
   cTeletextSetupPage(void);
   static const ActionKeyName *actionKeyNames;
   static const char **modes;
   //~cTeletextSetupPage(void);
   //void SetItemVisible(cOsdItem *Item, bool visible, bool callDisplay=false);
};

const ActionKeyName *cTeletextSetupPage::actionKeyNames = 0;
const char **cTeletextSetupPage::modes = 0;

/*class MenuEditActionItem : public cMenuEditStraItem {
public:
   MenuEditActionItem(cTeletextSetupPage *parentMenu, cMenuEditIntItem *pageNumberMenuItem,
                           const char *Name, int *Value, int NumStrings, const char * const *Strings);
protected:
   virtual eOSState ProcessKey(eKeys Key);
   cTeletextSetupPage *parent;
   cMenuEditIntItem *pageNumberItem;
};*/


cPluginTeletextosd::cPluginTeletextosd(void)
  : txtStatus(0), startReceiver(true), storage(NULL), maxStorage(-1)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginTeletextosd::~cPluginTeletextosd()
{
   // Clean up after yourself!
}

const char *cPluginTeletextosd::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return "  -d        --directory=DIR    The directory where the temporary\n"
         "                               files will be stored.\n"
         "                               (default: /var/cache/vdr/vtx)\n"
         "                               Ensure that the directory exists and is writable.\n"
         "  -n        --max-cache=NUM    Maximum size in megabytes of cache used\n"
         "                               to store the pages on the harddisk.\n"
         "                               (default: a calculated value below 50 MB)\n"
         "  -s        --cache-system=SYS Set the cache system to be used.\n"
         "                               Choose \"legacy\" for the traditional\n"
         "                               one-file-per-page system.\n"
         "                               Default is \"packed\" for the \n"
         "                               one-file-for-a-few-pages system.\n"
         "  -t,       --toptext          Store top text pages at cache. (unviewable pages)\n"
         "  -D|--debugmask <int|hexint>  Enable debugmask\n";
}

bool cPluginTeletextosd::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
   static struct option long_options[] = {
       { "directory",    required_argument,       NULL, 'd' },
       { "max-cache",    required_argument,       NULL, 'n' },
       { "cache-system", required_argument,       NULL, 's' },
       { "toptext",      no_argument,             NULL, 't' },
       { "debugmask",    required_argument,       NULL, 'D' },
       { NULL }
       };

   int c;
   while ((c = getopt_long(argc, argv, "s:d:n:tD:", long_options, NULL)) != -1) {
        switch (c) {
          case 's':
                    if (!optarg)
                       break;
                    if (strcasecmp(optarg, "legacy")==0)
                       storageSystem = Storage::StorageSystemLegacy;
                    else if (strcasecmp(optarg, "packed")==0)
                       storageSystem = Storage::StorageSystemPacked;
                    break;
          case 'd': Storage::setRootDir(optarg);
                    break;
          case 'n': if (isnumber(optarg)) {
                       int n = atoi(optarg);
                       maxStorage=n;
                    }
                    break;
          case 't': storeTopText=true;
                    break;
          case 'D':
            if ((strlen(optarg) > 2) && (strncasecmp(optarg, "0x", 2) == 0)) {
               // hex conversion
               if (sscanf(optarg + 2, "%x", &m_debugmask) == 0) {
                  esyslog("osdteletext: can't parse hexadecimal debug mask (skip): %s", optarg);
               };
            } else {
				   m_debugmask = atoi(optarg);
            };
			   dsyslog("osdteletext: enable debug mask: %d (0x%02x)", m_debugmask, m_debugmask);
            break;
        }
   }
   return true;
}

bool cPluginTeletextosd::Start(void)
{
   // Start any background activities the plugin shall perform.
   //Clean any files which might be remaining from the last session,
   //perhaps due to a crash they have not been deleted.
   switch (storageSystem) {
      case Storage::StorageSystemLegacy:
         storage =  new LegacyStorage(maxStorage);
      case Storage::StorageSystemPacked:
      default:
        storage = new PackedStorage(maxStorage);
   }

   initTexts();
   if (startReceiver)
      txtStatus=new cTxtStatus(storeTopText, storage);
   if (ttSetup.OSDheightPct<10)  ttSetup.OSDheightPct=10;
   if (ttSetup.OSDwidthPct<10)   ttSetup.OSDwidthPct=10;
   if (ttSetup.OSDtopPct  > 90) ttSetup.OSDtopPct  = 0; // failsafe
   if (ttSetup.OSDleftPct > 90) ttSetup.OSDleftPct = 0; // failsafe
   if (ttSetup.OSDtopPct  <  0) ttSetup.OSDtopPct  = 0; // failsafe
   if (ttSetup.OSDleftPct <  0) ttSetup.OSDleftPct = 0; // failsafe
   if (abs(ttSetup.txtVoffset) > 10) ttSetup.txtVoffset = 0; // failsafe

   return true;
}

void cPluginTeletextosd::Stop(void)
{
   DELETENULL(txtStatus);
   if (storage) {
      storage->cleanUp();
      DELETENULL(storage);
   }
}

void cPluginTeletextosd::initTexts() {
   if (cTeletextSetupPage::actionKeyNames)
      return;
   //TODO: rewrite this in the xy[0].cd="fg"; form
   static const ActionKeyName st_actionKeyNames[] =
   {
      { "Action_kRed",      trVDR("Key$Red") },
      { "Action_kGreen",    trVDR("Key$Green") },
      { "Action_kYellow",   trVDR("Key$Yellow") },
      { "Action_kBlue",     trVDR("Key$Blue") },
      { "Action_kPlay",     trVDR("Key$Play") },
      { "Action_kStop",     trVDR("Key$Stop") },
      { "Action_kFastFwd",  trVDR("Key$FastFwd") },
      { "Action_kFastRew",  trVDR("Key$FastRew") }
   };

   cTeletextSetupPage::actionKeyNames = st_actionKeyNames;

   static const char *st_modes[] =
   {
      tr("Zoom"),
      tr("Half page"),
      tr("Change channel"),
      tr("Switch background"),
      //tr("Suspend receiving"),
      tr("Jump to...")
   };

   cTeletextSetupPage::modes = st_modes;
}

void cPluginTeletextosd::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

const char *cPluginTeletextosd::MainMenuEntry(void)
{
   return ttSetup.HideMainMenu ? 0 : tr(MAINMENUENTRY);
}

cOsdObject *cPluginTeletextosd::MainMenuAction(void)
{
   // Perform the action when selected from the main VDR menu.
   return new TeletextBrowser(txtStatus,storage);
}

cMenuSetupPage *cPluginTeletextosd::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cTeletextSetupPage;
}


bool cPluginTeletextosd::SetupParse(const char *Name, const char *Value)
{
  initTexts();
  // Parse your own setup parameters and store their values.
  //Stretch=true;
  if (!strcasecmp(Name, "configuredClrBackground")) ttSetup.configuredClrBackground=( ((unsigned int)atoi(Value)) << 24);
  else if (!strcasecmp(Name, "showClock")) ttSetup.showClock=atoi(Value);
     //currently not used
  else if (!strcasecmp(Name, "suspendReceiving")) ttSetup.suspendReceiving=atoi(Value);
  else if (!strcasecmp(Name, "autoUpdatePage")) ttSetup.autoUpdatePage=atoi(Value);
  else if (!strcasecmp(Name, "OSDheightPct")) ttSetup.OSDheightPct=atoi(Value);
  else if (!strcasecmp(Name, "OSDwidthPct")) ttSetup.OSDwidthPct=atoi(Value);
  else if (!strcasecmp(Name, "OSDtopPct")) ttSetup.OSDtopPct=atoi(Value);
  else if (!strcasecmp(Name, "OSDleftPct")) ttSetup.OSDleftPct=atoi(Value);
  else if (!strcasecmp(Name, "inactivityTimeout")) /*ttSetup.inactivityTimeout=atoi(Value)*/;
  else if (!strcasecmp(Name, "HideMainMenu")) ttSetup.HideMainMenu=atoi(Value);
  else if (!strcasecmp(Name, "txtFontName")) ttSetup.txtFontName=strdup(Value);
  else if (!strcasecmp(Name, "txtG0Block")) ttSetup.txtG0Block=atoi(Value);
  else if (!strcasecmp(Name, "txtG2Block")) ttSetup.txtG2Block=atoi(Value);
  else if (!strcasecmp(Name, "txtVoffset")) ttSetup.txtVoffset=atoi(Value);
  else if (!strcasecmp(Name, "colorMode4bpp")) ttSetup.colorMode4bpp=atoi(Value);
  else if (!strcasecmp(Name, "lineMode24")) ttSetup.lineMode24=atoi(Value);
  // ignore obsolete options
#define DSYSLOG_IGNORE_OPTION dsyslog("osdteletext: ignore obsolete option in setup.conf: osdteletext.%s", Name);
  else if (!strcasecmp(Name, "OSDHAlign"  )) { DSYSLOG_IGNORE_OPTION } // < 1.0.0
  else if (!strcasecmp(Name, "OSDVAlign"  )) { DSYSLOG_IGNORE_OPTION } // < 1.0.0
  else if (!strcasecmp(Name, "OSDheight"  )) { DSYSLOG_IGNORE_OPTION } // < 1.0.0
  else if (!strcasecmp(Name, "OSDwidth"   )) { DSYSLOG_IGNORE_OPTION } // < 1.0.0
  else if (!strcasecmp(Name, "OSDhcentPct")) { DSYSLOG_IGNORE_OPTION } // 1.0.0 - 1.0.4
  else if (!strcasecmp(Name, "OSDvcentPct")) { DSYSLOG_IGNORE_OPTION } // 1.0.0 - 1.0.4
  else {
     for (int i=0;i<LastActionKey;i++) {
        if (!strcasecmp(Name, cTeletextSetupPage::actionKeyNames[i].internalName)) {
           ttSetup.mapKeyToAction[i]=(eTeletextAction)atoi(Value);

           //for migration to 0.4
           if (ttSetup.mapKeyToAction[i]<100 && ttSetup.mapKeyToAction[i]>=LastAction)
              ttSetup.mapKeyToAction[i]=LastAction-1;

           return true;
        }
     }

     //for migration to 0.4
     char act[8];
     strncpy(act, Name, 7);
     if (!strcasecmp(act, "Action_"))
        return true;

     return false;
  }
  return true;
}

void cTeletextSetupPage::Store(void) {
   //copy table
   for (int i=0;i<LastActionKey;i++) {
      if (temp.mapKeyToAction[i] >= LastAction) //jump to page selected
         ttSetup.mapKeyToAction[i]=(eTeletextAction)tempPageNumber[i];
      else //one of the other modes selected
         ttSetup.mapKeyToAction[i]=temp.mapKeyToAction[i];
   }
   ttSetup.configuredClrBackground=( ((unsigned int)tempConfiguredClrBackground) << 24);
   ttSetup.showClock=temp.showClock;
   ttSetup.suspendReceiving=temp.suspendReceiving;
   ttSetup.autoUpdatePage=temp.autoUpdatePage;
   ttSetup.OSDheightPct=temp.OSDheightPct;
   ttSetup.OSDwidthPct=temp.OSDwidthPct;
   ttSetup.OSDtopPct=temp.OSDtopPct;
   ttSetup.OSDleftPct=temp.OSDleftPct;
   ttSetup.HideMainMenu=temp.HideMainMenu;
   ttSetup.txtFontName=temp.txtFontNames[temp.txtFontIndex];
   ttSetup.txtG0Block=temp.txtG0Block;
   ttSetup.txtG2Block=temp.txtG2Block;
   ttSetup.txtVoffset=temp.txtVoffset;
   ttSetup.colorMode4bpp=temp.colorMode4bpp;
   ttSetup.lineMode24=temp.lineMode24;
   //ttSetup.inactivityTimeout=temp.inactivityTimeout;

   for (int i=0;i<LastActionKey;i++) {
      SetupStore(actionKeyNames[i].internalName, ttSetup.mapKeyToAction[i]);
   }
   SetupStore("configuredClrBackground", (int)(ttSetup.configuredClrBackground >> 24));
   SetupStore("showClock", ttSetup.showClock);
      //currently not used
   //SetupStore("suspendReceiving", ttSetup.suspendReceiving);
   SetupStore("autoUpdatePage", ttSetup.autoUpdatePage);
   SetupStore("OSDheightPct", ttSetup.OSDheightPct);
   SetupStore("OSDwidthPct", ttSetup.OSDwidthPct);
   SetupStore("OSDtopPct", ttSetup.OSDtopPct);
   SetupStore("OSDleftPct", ttSetup.OSDleftPct);
   SetupStore("HideMainMenu", ttSetup.HideMainMenu);
   SetupStore("txtFontName", ttSetup.txtFontName);
   SetupStore("txtG0Block", ttSetup.txtG0Block);
   SetupStore("txtG2Block", ttSetup.txtG2Block);
   SetupStore("txtVoffset", ttSetup.txtVoffset);
   SetupStore("colorMode4bpp", ttSetup.colorMode4bpp);
   SetupStore("lineMode24", ttSetup.lineMode24);
   //SetupStore("inactivityTimeout", ttSetup.inactivityTimeout);
}


cTeletextSetupPage::cTeletextSetupPage(void) {
   cString buf;
   cOsdItem *item;

   temp.txtBlock[0]  = tr("Latin 1");
   temp.txtBlock[1]  = tr("Latin 2");
   temp.txtBlock[2]  = tr("Latin 3");
   temp.txtBlock[3]  = tr("Latin 4");
   temp.txtBlock[4]  = tr("Cyrillic");
   temp.txtBlock[5]  = tr("Reserved");
   temp.txtBlock[6]  = tr("Greek");
   temp.txtBlock[7]  = tr("Reserved");
   temp.txtBlock[8]  = tr("Arabic");
   temp.txtBlock[9]  = tr("Reserved");
   temp.txtBlock[10] = tr("Hebrew");

   //init tables
   for (int i=0;i<LastActionKey;i++) {
      if (ttSetup.mapKeyToAction[i] >= LastAction) {//jump to page selected
         temp.mapKeyToAction[i]=LastAction; //to display the last string
         tempPageNumber[i]=ttSetup.mapKeyToAction[i];
      } else { //one of the other modes selected
         temp.mapKeyToAction[i]=ttSetup.mapKeyToAction[i];
         tempPageNumber[i]=100;
      }
   }
   tempConfiguredClrBackground=(ttSetup.configuredClrBackground >> 24);
   temp.showClock=ttSetup.showClock;
   temp.suspendReceiving=ttSetup.suspendReceiving;
   temp.autoUpdatePage=ttSetup.autoUpdatePage;
   temp.OSDheightPct=ttSetup.OSDheightPct;
   temp.OSDwidthPct=ttSetup.OSDwidthPct;
   temp.OSDtopPct=ttSetup.OSDtopPct;
   temp.OSDleftPct=ttSetup.OSDleftPct;
   temp.HideMainMenu=ttSetup.HideMainMenu;
   temp.txtFontName=ttSetup.txtFontName;
   temp.txtG0Block=ttSetup.txtG0Block;
   temp.txtG2Block=ttSetup.txtG2Block;
   temp.txtVoffset=ttSetup.txtVoffset;
   temp.colorMode4bpp=ttSetup.colorMode4bpp;
   temp.lineMode24=ttSetup.lineMode24;
   //temp.inactivityTimeout=ttSetup.inactivityTimeout;

   cFont::GetAvailableFontNames(&temp.txtFontNames, true);
   temp.txtFontIndex = temp.txtFontNames.Find(ttSetup.txtFontName);
   if (temp.txtFontIndex < 0) {
       temp.txtFontIndex = 0;
   }

   Add(new cMenuEditIntItem(tr("Background transparency"), &tempConfiguredClrBackground, 0, 255));

   Add(new cMenuEditBoolItem(tr("Show clock"), &temp.showClock ));

   //Add(new cMenuEditBoolItem(tr("Setup$Suspend receiving"), &temp.suspendReceiving ));

   Add(new cMenuEditBoolItem(tr("Auto-update pages"), &temp.autoUpdatePage ));
   Add(new cMenuEditIntItem(tr("OSD left (%)"), &temp.OSDleftPct, 0, 90));
   Add(new cMenuEditIntItem(tr("OSD top (%)"), &temp.OSDtopPct, 0, 90));
   Add(new cMenuEditIntItem(tr("OSD width (%)"), &temp.OSDwidthPct, 10, 100));
   Add(new cMenuEditIntItem(tr("OSD height (%)"), &temp.OSDheightPct, 10, 100));
   Add(new cMenuEditBoolItem(tr("Hide mainmenu entry"), &temp.HideMainMenu));
   Add(new cMenuEditStraItem(tr("Text Font"), &temp.txtFontIndex, temp.txtFontNames.Size(), &temp.txtFontNames[0]));
   Add(new cMenuEditStraItem(tr("G0 code block"), &temp.txtG0Block, NUMELEMENTS(temp.txtBlock), temp.txtBlock));
   Add(new cMenuEditStraItem(tr("G2 code block"), &temp.txtG2Block, NUMELEMENTS(temp.txtBlock), temp.txtBlock));
   Add(new cMenuEditIntItem(tr("Text Vertical Offset"), &temp.txtVoffset, -10, 10));
   Add(new cMenuEditBoolItem(tr("16-Color Mode"), &temp.colorMode4bpp));
   Add(new cMenuEditBoolItem(tr("24-Line Mode"), &temp.lineMode24));

   //Using same string as VDR's setup menu
   //Add(new cMenuEditIntItem(tr("Setup.Miscellaneous$Min. user inactivity (min)"), &temp.inactivityTimeout));

   buf = cString::sprintf("%s:", tr("Key bindings"));
   item = new cOsdItem(*buf);
   item->SetSelectable(false);
   Add(item);

   for (int i=0;i<LastActionKey;i++) {
      ActionEdits[i].Init(this, i, new cMenuEditIntItem(tr("  Page number"), &tempPageNumber[i], 100, 899),
         new cMenuEditStraItem(actionKeyNames[i].userName, (int*)&temp.mapKeyToAction[i], LastAction+1, modes) );
   }
}

eOSState cTeletextSetupPage::ProcessKey(eKeys Key) {
   eOSState state = cMenuSetupPage::ProcessKey(Key);
   if (Key != kRight && Key!=kLeft)
      return state;
   cOsdItem *item = Get(Current());
   for (int i=0;i<LastActionKey;i++) {
      if (ActionEdits[i].action==item) { //we have a key left/right and one of our items as current
         //eOSState state = item->ProcessKey(Key);
         //if (state != osUnknown) { //really should not return osUnknown I think
            if (temp.mapKeyToAction[i] == LastAction && !ActionEdits[i].visible) {
               //need to make it visible
               if (i+1<LastActionKey)
                  //does not work for i==LastAction-1
                  Ins( ActionEdits[i].number, false, ActionEdits[i+1].action);
               else
                  Add( ActionEdits[i].number, false );

               ActionEdits[i].visible=true;
               Display();
            } else if (temp.mapKeyToAction[i] != LastAction && ActionEdits[i].visible) {
               //need to hide it
               cList<cOsdItem>::Del(ActionEdits[i].number, false);
               ActionEdits[i].visible=false;
               Display();
            }
            break;
            //return state;
         //}
     }
   }

   return state;
   //return cMenuSetupPage::ProcessKey(Key);
}


void ActionEdit::Init(cTeletextSetupPage* s, int num, cMenuEditIntItem  *p, cMenuEditStraItem * a) {
   action=a;
   number=p;
   s->Add(action);
   if (s->temp.mapKeyToAction[num] == LastAction) {
      s->Add(number);
      visible=true;
   } else
      visible=false;
}




VDRPLUGINCREATOR(cPluginTeletextosd); // Don't touch this!

// vim: ts=3 sw=3 et
