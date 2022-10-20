#!/usr/bin/python3

from http.server import BaseHTTPRequestHandler, HTTPServer
from unidecode import unidecode
import requests
import shutil
import spotipy
import spotipy.util as util
import time
import json

hostName = "xxx.xxx.xxx.xxx"
serverPort = 5566

CLIENT_ID="xxxxx"
CLIENT_SECRET="xxxxx"

username = "xxxx"
scope = "user-read-currently-playing user-read-playback-state playlist-read-private"
redirect_uri = "http://xxxx.xxxx.xxxx.xxx/callback/"

cache_timeout = 45

old_song_name = ""
last_request = 0
last_output = ""
last_state = ""
last_duration = 0
last_progress = 0
is_playing = False;

# this is the default cache file name
token_file = ".cache-" + username

# take note that the spotify token cache file is where the python file is executed from
token = util.prompt_for_user_token(username, scope, CLIENT_ID, CLIENT_SECRET, redirect_uri)
sp = spotipy.Spotify(auth=token)

def getTokenData():
	global token_file
	f = open(token_file)
	tdata = json.load(f)
	f.close()
	return tdata

class MyServer(BaseHTTPRequestHandler):
	def do_GET(self):
		global old_song_name, last_output
		global token, sp
		global last_request, last_state
		global last_duration, last_progress, is_playing
		# get the token information from cache file
		tdata = getTokenData()
		# calculate the cache expiry time
		print("Token time: " + str(tdata['expires_at'] - int(time.time())))
		if ((time.time() - tdata['expires_at']) >= 0):
			print("Refreshing token")
			token = util.prompt_for_user_token(username, scope, CLIENT_ID, CLIENT_SECRET, redirect_uri)
			tdata = getTokenData()
		try:
			sp = spotipy.Spotify(auth=token)
		except:
			token = util.prompt_for_user_token(username, scope, CLIENT_ID, CLIENT_SECRET, redirect_uri)
			if token:
				print("Token exception: Token refresh ok")
				tdata = getTokenData()
			else:
				print("Token exception: Token refresh failed")
				exit()

		if sp:
			if (self.path == "/status"):
				self.send_response(200)
				self.send_header("Content-type", "text/plain; charset=ascii")
				self.end_headers()
				# if last request is within 3s, use cached output. rate limit is about 2000/hour?
				if ((time.time() - last_request) > cache_timeout):
					currentsong = sp.current_playback()
					if currentsong:
						if currentsong['is_playing']:
							song_name = currentsong['item']['name']
							song_artist = currentsong['item']['artists'][0]['name']
							song_album = currentsong['item']['album']['name']
							artwork = currentsong['item']['album']['images'][1]['url']
							if (old_song_name != song_name):
								if artwork:
									r = requests.get(artwork, stream = True)
									if r.status_code == 200:
										r.raw.decode_content = True
										print("Artwork fetched: " + artwork)
										with open("/mnt/INTERNAL/cache/bum/current.jpg","wb") as f:
											shutil.copyfileobj(r.raw, f)
											print("Artwork saved: " + artwork)
									else:
										print("Artwork failed: " + artwork)
							else:
								print("No change in song name. Assuming no new artwork.")	
							if song_name:
								last_state = "state: play\nTitle: " + song_name + "\nArtist: " + song_artist + "\nAlbum: " + song_album + "\nArtwork: " + artwork+ "\n"
								time_output = "time: " + str(int(int(currentsong['progress_ms']) / 1000)) + ":" + str(int(int(currentsong['item']['duration_ms']) / 1000)) + "\n"
								last_progress = int(int(currentsong['progress_ms']) / 1000)
								last_duration = int(int(currentsong['item']['duration_ms']) / 1000)
								output = last_state + time_output
								old_song_name = song_name
								is_playing = True
							else:
								output = "state: play\nTitle:\nArtist:\nAlbum:\nArtwork:\ntime:\n"
								is_playing = False
						else:
							output = "state: stop\nTitle:\nArtist:\nAlbum:\nArtwork:\ntime:\n"
							is_playing = False
					else:
						output = "state: stop\nTitle:\nArtist:\nAlbum:\nArtwork:\ntime:\n"
						is_playing = False
					output = unidecode(output)
					print("Sent output:\n" + output)
					last_output = output
					last_request = int(time.time())
				else:
					print("Using last result < " + str(cache_timeout) + "s, update in " + str(cache_timeout - (int(time.time()) - last_request)) + "s")
					if is_playing:
						# reformats output to guess playing time progress
						guess_progress = (int(time.time()) - last_request) + last_progress
						if (guess_progress > last_duration):
							guess_progress = last_duration
							# invalidate the cache for song information
							last_request = int(time.time() - cache_timeout) - 2
						time_output =  "time: " + str(guess_progress) + ":" + str(last_duration) + "\n"
						print("Sending Time: " + time_output);
						output = last_state + time_output
					else:
						# use last state as it wasn't playing before
						output = last_output
				# sends output to client
				self.wfile.write(bytes("%s" % output,"ascii"))
			elif (self.path == "/local/current.jpg"):
				file_size = os.path.getsize("/mnt/INTERNAL/cache/bum/current_tn.jpg")
				self.send_response(200)
				self.send_header("Content-type", "image/jpg")
				self.send_header("Content-length", file_size)
				self.end_headers()
				f = open("/mnt/INTERNAL/cache/bum/current_tn.jpg","rb")
				self.wfile.write(f.read())
				f.close()
				print("Sending image: /mnt/INTERNAL/cache/bum/current_tn.jpg");
			else:
				self.send_response(200)
				self.send_header("Content-type", "text/plain; charset=ascii")
				self.end_headers()
				output = unidecode("state: unknown\nTitle:\nArtist:\nAlbum:\nArtwork:\ntime:\n")
				print("Decoded: " + output)
				self.wfile.write(bytes("%s" % output,"ascii"))
		else:
			token = util.prompt_for_user_token(username, scope, CLIENT_ID, CLIENT_SECRET, redirect_uri)
			print("Reauthenticated")

if __name__ == "__main__":        
	webServer = HTTPServer((hostName, serverPort), MyServer)
	print("Spotify Status Service started http://%s:%s" % (hostName, serverPort))

	try:
		webServer.serve_forever()
	except KeyboardInterrupt:
 		pass

webServer.server_close()
print("Service stopped.")
