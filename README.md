# mpd-spotify-display
ESP32-S3 TFT Song display for MPD and Spotify

# Introduction

This is a small project to create a simple display to work with Mopidy server and the Mopidy-MPD plugin.
It allows the display of the current and next track in the playlist via the MPD server.

# The Purpose

As Mopidy server was made to be headless, there is no display attached to it. Even though a web interface is used to interact and see which track is playing, it is very inefficient at a glance to see what is playing and the song that is coming up next. This display aims to provide that information in a very quick glance just to show the current and next track.

Occassionally, the system that runs Mopidy server also becomes and AirPlay server via `shairport`. Spotify is then used to play music to the same audio system. This same display also provides the currently playing track from Spotify so as to help identify the track.

# Components

## Hardware used

- Lilygo T-DisplayS3 ESP32-S3 with TFT 1.9" display.
	- https://www.lilygo.cc/

## Software used

- Mopidy
- Mopidy-MPD
- Mopidy-Local
- Pidi
- Spotipy
- ImageMagick
- Python Unidecode Server Service
- Spotify Server Service

## General Software Process and Considerations

1. ESP32 connects to wifi and queries for MPD server
1. If MPD server is available, connect to it. Otherwise assume Spotify playback
1. Check for state of playback and obtain the various track information
1. Decode the information so that only ascii text is used
1. Downloads the artwork

### Artwork download

#### MPD

Artwork is made avialable via the `pidi` project by pirate audio. That fetches the artwork for the currently playing track from MPD and caches it. ImageMagick is used to convert the image into a smaller size meant for the ESP32-3 1.9" display. The image is served over the Mopidy-Local server. To have the artwork automatically converted on the server side, `fswatch` is used together with the `convert` command.

```
# /mnt/INTERNAL/cache/bum/current.jpg is the path defined when starting pidi

/usr/bin/fswatch --event Updated /mnt/INTERNAL/cache/bum/current.jpg | /usr/bin/xargs -n 1 /home/pi/local/bin/update_current_art.sh
```

`update_current_art.sh` is a simple script that creates the link to Mopidy-Local and also converts the image.
A softlink is used to provide the image from the `pidi` cached location to the actual Mopidy-Local web service location.


```
# note that there is a soft-link that goes from /var/lib/mopidy/local/images/current.jpg -> /mnt/INTERNAL/cache/bum/current_tn.jpg
convert -resize 107x107 /mnt/INTERNAL/cache/bum/current.jpg /mnt/INTERNAL/cache/bum/current_tn.jpg

```
These are all added into the start/stop script that handles the mopidy process.


#### Spotify

Artwork information is made available as part of the JSON from spotify using the `currently_playing()` method. That information is extracted and image downloaded. The image location is placed in the same location as what `pidi` would have done. i.e. the `current.jpg` file defined in the cache folder location. This way, `fswatch`, as setup above, will pick up the same file and also serve the image over the same path.

However, as `shairport` and Mopidy cannot run at the same time (due to conflict on hardware), Mopidy-Local will not be avialable. So a small server is written in python to help serve this image on the same expected path. The same server also is used to serve the Spotify song information to the ESP32 firmware. The server will format the output as retrived from Spotify and mimick the output of an MPD server. This increases code reuseability on the ESP32 end of things.

i.e. querying `http://10.0.0.18/local/current.jpg` will always retrieve the album art of the currently playing track

### Unicode (Python Unidecode Server Service)

As the fonts for songs of other languages cannot display on the ESP32 due to lack of memory, all song track information is unidecoded into ascii before being sent for display. This leads to the creation of the `Python Unidecode Server Service`. This service is python webserver that connects to the MPD server independently and decodes all track information upon request. 

The ESP32 connects directly to the MPD to get the playback state, but queries this decode service to obtain the song information. If this decode service fails for any reason, the ESP32 will fall back and connect directly to the MPD server to obtain the same informaiton. The disadvantage is that any unicode character will end up being displayed as gibberish. The unidecode server ensures that information displayed is at least phonetically corrected. This was tested to work with Chinese, Cantonese, Japanese song titles.


### Querying Spotify Information (Spotify Server Service)

AS it was determined to be inefficient to do too much processing on the ESP32, connection, authentication parsing of Spotify playback state and information is handled by thie `Spotify Server Service`. It will authenticate, handle token renewals and query state and track information and hand the results off to the ESP. It also handles artwork as described above.

As Spotify will throttle queries, the service will also cache information and compute the predicted playback state and send the output in MPD format to the ESP32. Playback state is provided and also requries the track information if the playing track is guessed to have ended.

## Putting it all together.

2 server components and 1 piece of firmware code makes this work. the server components are installed and ran on the headless Mopidy audio server. Upon boot, the ESP32 will do its work as described and continue to display the playing track as efficiently as possible. The ESP32 will automatically enter deepsleep mode after a pre-defined timeout in order to save power, if the playback state stops. 

The inital idea was to run this off a small Li-ion battery but as the battery port is broken on the unit, the USB-C port had to be used instead.

A simple 3D case was printed and an acrylic sheet attached to protect the display.

*Spotiify does not have a way to determine the next track, unlike MPD. So static text had to be used to fill the blank space instead*

- Some pictures of the result

# References

Main graphics library
- https://www.arduino.cc/reference/en/libraries/gfx-library-for-arduino/

MPD connection
- https://github.com/nopnop2002/esp8266-mpd-client

JPEG download and display
- https://github.com/moononournation/Arduino_GFX/tree/master/examples/WiFiPhotoFrame

ESP32 sleep modes
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html

Spotipy
- https://spotipy.readthedocs.io/en/master/

Pidi
- https://github.com/pimoroni/pidi

Thanks to @teastain for assisting in the trouble shooting of the battery port!
His project with the same hardware avialable here.
https://github.com/teastainGit/LillyGO-T-display-S3-setup-and-examples
