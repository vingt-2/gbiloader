# gbiloader
Tool to automate launching extrem's Game Boy Interface software.

_Copyright 2017 by Adam Zey_

This is a fork of Dolaunch by suloku, specifically the BrosexecConf tool. It's heavily modified to target GBI specifically.

This tool allowas you to automatically start GBI when powering on the gamecube, and to support command-line arguments while doing so. Currently most people who want to start GBI automatically either boot GBI directly, or set Swiss to autostart GBI, but neither of these options support passing commandline arguments to GBI.

Each version of GBI can have their own .CLI file associated with it, so you can have different settings for each version.

By default, it will load GBI-LL. Hold Y while it starts to get a menu to select which version of GBI to load.


### Requirements

gbiloader requires an SDML or SDGecko as well as some way of loading homebrew software.


### Instructions

- Place the "gbiloader" folder on your SD card. All three versions of GBI and a copy of Swiss are included, you can update them to newer versions as you see fit.

- Edit the INI file to set the default resolution to match your GBI setting. The INI is set to 240p NTSC by default, if in doubt, set it to "auto", which lets the GameCube decide.

- Edit the GBI CLI files to set the GBI settings to your taste. The included CLI files are set to 240p NTSC, with LL also set to 1:1 pixel aspect. ULL does not support CLI files at all (GBI limitation). If you use a SNES controller adapter, you may want to add an entry to set the control scheme.

- Boot gbiloader through whatever means. Burn it to a bootable DVD, load it via the SDML disc, boot it from a memory card, etc. You may need to rename the gbiloader DOL or GCI file in order to use your preferred method. Three variants of gbiloader are included: "gbiloader.dol" is the normal package, "boot.dol" is the same thing but compressed (much smaller but may load slower), and "boot.gci" is meant for loading on memory cards via save exploits.

- gbiloader will automatically boot the last thing it loaded (gbi-ll by default). Hold down the "Y" button before gbiloader starts to show a menu to select what it should boot. When you select the option, it will save your preference to your SD card (so the SD must be writable) for the next time, and then boot what you selected.

- gbiloader can load all three versions of GBI as well as Swiss. **BE WARNED THAT IF YOU USE SWISS TO AUTOBOOT GBILOADER, YOU CANNOT USE GBILOADER TO LOAD SWISS. IT WILL ENTER AN INFINITE LOOP UNTIL YOU ACCESS THE GBILOADER MENU TO STOP IT.** I'm considering including a custom build of Swiss that has auto-boot disabled in order to work around this limitation.


### More information

For more information on GBI configuration, see the DCP files in the official GBI release, and the official thread here: <https://www.gc-forever.com/forums/viewtopic.php?f=37&t=2782>

For the gbiloader source code and latest builds, visit <https://github.com/Guspaz/gbiloader>


### Change history

**r1**
- Initial release. Includes GBI 20170410 and Swiss r387