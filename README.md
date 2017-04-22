# gbiloader
Tool to automate launching extrem's Game Boy Interface software.

This is a fork of Dolaunch by suloku, specifically the BrosexecConf tool. It's heavily modified to target GBI specifically.

This tool allowas you to automatically start GBI when powering on the gamecube, and to support command-line arguments while doing so. Currently most people who want to start GBI automatically either boot GBI directly, or set Swiss to autostart GBI, but neither of these options support passing commandline arguments to GBI.

Each version of GBI can have their own .CLI file associated with it, so you can have different settings for each version.

By default, it will load GBI-LL. Hold Y while it starts to get a menu to select which version of GBI to load. The splash/loading screen will change accordingly. In the future, gbiloader will save this selection back to its config file so that your selection will become the new default.

More extensive documentation will be added later.
