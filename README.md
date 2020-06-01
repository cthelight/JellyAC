# JellyAC
Headless command line JellyFin client for audio.

## Description
This is a simple headless client for JellyFin. It simply turns any device in to a remote-controllable audio output that is controlled by the standard JellyFin interface.
Simply select your JellyAC client from the list on the web browser, mobile app, or whatever interface that supports remote control, and start playing music!

## Use
Currently there is no "install". Simply run make and the execute jac.out as follows:

./jac.out &lt;ip:port&gt; &lt;username&gt; &lt;password&gt;

NOTE: be sure to clone all submodules as well, prior to making.
## Dependancies
### Compile
    libcurl
    libwebsockets
### Run
    mplayer