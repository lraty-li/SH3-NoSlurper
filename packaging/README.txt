SH3 No Slurper

INSTALLATION
============

1. Make sure Silent Hill 3 has a compatible 32-bit ASI loader installed.
   Ultimate ASI Loader is supported:
   https://github.com/ThirteenAG/Ultimate-ASI-Loader

2. Copy the "scripts" folder from this archive into the Silent Hill 3
   game directory.

   The final file should be:

       <SH3_DIR>\scripts\NoSlurper.asi

3. Start the game normally.

A NoSlurper.log file will be written next to the ASI when the mod loads.


UNINSTALLATION
==============

Delete:

    <SH3_DIR>\scripts\NoSlurper.asi


NOTES
=====

- The mod targets Slurper enemy kinds 0x20A and 0x20B.
- It preserves the original Slurper variant/model and transitions the
  instantiated enemy into the game's native dead state.
- Other enemy types are not modified.
- No save files or game archives are modified on disk.

Source:
https://github.com/lraty-li/SH3-NoSlurper
