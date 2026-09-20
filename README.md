[![Windows Build](https://github.com/mrlindstrom/OpenKJ/actions/workflows/fork-windows-build.yml/badge.svg)](https://github.com/mrlindstrom/OpenKJ/actions/workflows/fork-windows-build.yml)
[![macOS Build](https://github.com/mrlindstrom/OpenKJ/actions/workflows/fork-macos-build.yml/badge.svg)](https://github.com/mrlindstrom/OpenKJ/actions/workflows/fork-macos-build.yml)

**This is a fork of [OpenKJ/OpenKJ](https://github.com/OpenKJ/OpenKJ).**  Builds from this
repository are published under [Releases](https://github.com/mrlindstrom/OpenKJ/releases) and are
not official OpenKJ builds.  They are unsigned, so Windows SmartScreen and macOS Gatekeeper will
warn about them.

Changes in this fork
====================

**QR code on the singer display**  
A QR code can be shown on the singer screen, pointing at whatever URL you like — typically your
song request page, so patrons can scan it and request songs from their phones.  The code, its
caption, its corner, its size and its colors are all configurable under
**Settings → Video → Show QR code on singer display**.  Changes take effect immediately, so the
position can be tuned with the singer window open.

**Faster karaoke database updates**  
Running Update on the karaoke database no longer competes with the background song-length scanner,
which previously made an update slower than clearing the database and rescanning from scratch.
Looking up songs, saving song lengths and detecting moved files are all much cheaper on large
libraries.

**Faster break music database updates**  
Break music updates now read tags only for files that aren't already in the database, instead of
re-reading every file on every update, and save them in a single batch.  More audio formats
(FLAC, WAV, M4A, WMA, Opus) are read with TagLib rather than GStreamer, which is considerably
faster, and filenames with accented or non-Latin characters now import correctly on Windows.

**Build and CI**  
The Windows build finds the MSVC GStreamer package automatically, and GitHub Actions workflows
build Windows and macOS installers and publish a release when a version tag is pushed.


**Downloads**  
If you are looking for installers for Windows or macOS, please visit the Downloads section at https://openkj.org

Linux users can grab OpenKJ stable versions from flathub: https://flathub.org/apps/details/org.openkj.OpenKJ

If you would like to install Linux versions of the unstable builds, please refer to the OpenKJ documentation wiki.

Documentation can be found at https://docs.openkj.org

If you need help with OpenKJ, you can reach out to support@openkj.org via email.

OpenKJ
======

Cross-platform open source karaoke show hosting software.

OpenKJ is a fully featured karaoke hosting program.
A few features:
* Save/track/load regular singers
* Key changer
* Tempo control
* EQ
* End of track silence detection (after last CDG draw command)
* Rotation ticker on the CDG display
* Option to use a custom background or display a rotating slide show on the CDG output dialog while idle
* Fades break music in and out automatically when karaoke tracks start/end
* Remote request server integration allowing singers to look up and submit songs via the web or mobile apps
* Configurable QR code on the singer display for pointing patrons at the request page (this fork)
* Automatic performance recording
* Autoplay karaoke mode
* Lots of other little things

It currently handles media+g zip files (zip files containing an mp3, wav, or ogg file and a cdg file) and paired mp3 and cdg files.  I'll be adding others in the future if anyone expresses interest.  It also can play non-cdg based video files (mkv, mp4, mpg, avi) for both break music and karaoke.

Database entries for the songs are based on the file naming scheme.  I've included the common ones I've come across which should cover 90% of what's out there. Custom patterns can be also defined in the program using regular expressions.



**Requirements to build OpenKJ:**

* Qt 5.x
* gstreamer 1.4 or above
* spdlog
* taglib

**Linux**

Build using cmake from the command line or in your IDE of choice

**Mac**

Building now works on OS X in Qt Creator using the native xcode compiler.  Use the latest stable version of the GStreamer SDK from http://gstreamer.freedesktop.org.


**Windows**

Building now works on Windows in Qt Creator using the msvc build system (both 32 and 64 bit).  Use the latest stable version of the GStreamer SDK from http://gstreamer.freedesktop.org.  You will likely need to modify the paths in the OpenKJ.pro file to match your devel environment.  Installers can be found at http://openkj.org/ if you just want to run the software and not build it yourself or help out with development.

