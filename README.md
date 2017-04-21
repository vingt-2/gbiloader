# gbiloader
Tool to automate launching extrem's Game Boy Interface software.

This is a fork of Dolaunch by suloku, specifically the BrosexecConf tool. It's heavily modified to target GBI specifically.

The goal is to allow you to automatically start GBI when powering on the gamecube, but at the same time support commandline arguments for GBI. Currently most people who want to do this either boot GBI directly, or set Swiss to autostart GBI, but neither of these options support passing commandline arguments to GBI.

The design goal is to, by default, simply launch GBI immediately from an SD card using a .cli file for GBI config. A splash screen will be presented while GBI loads, and you will be able to hold down the "L" button while gbiloader starts to view a menu to select between the three editions of GBI (GBI, GBI-LL, and GBI-ULL), each with their own corresponding .cli file. gbiloader should remember your previous selection, so that the next time you boot the gamecube, it should start the same edition of GBI as last time.