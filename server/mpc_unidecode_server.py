#!/usr/bin/python3
from http.server import BaseHTTPRequestHandler, HTTPServer
import subprocess
import mpd
from unidecode import unidecode

hostName = "xxx.xxx.xxx.xxx"
serverPort = 5555
mpd_host = "xxxxx"

class MyServer(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain; charset=ascii")
        self.end_headers()
        # self.wfile.write(bytes("Request: %s" % self.path, "ascii"))
        client = mpd.MPDClient()
        client.connect(mpd_host, 6600)
        if (self.path == "/title"):
            output = unidecode(client.playlistid(client.status()['songid'])[0]['title'])
        elif (self.path == "/artist"):
            output = unidecode(client.playlistid(client.status()['songid'])[0]['artist'])
        elif (self.path == "/album"):
            output = unidecode(client.playlistid(client.status()['songid'])[0]['album'])
        elif (self.path == "/next_title"):
            output = unidecode(client.playlistid(client.status()['nextsongid'])[0]['title'])
        elif (self.path == "/next_artist"):
            output = unidecode(client.playlistid(client.status()['nextsongid'])[0]['artist'])
        elif (self.path == "/next_album"):
            output = unidecode(client.playlistid(client.status()['nextsongid'])[0]['album'])
        elif (self.path == "/currentsong"):
            output = unidecode("Title: " + client.playlistid(client.status()['songid'])[0]['title'] + "\nArtist: " + client.playlistid(client.status()['songid'])[0]['artist'] + "\nAlbum: " + client.playlistid(client.status()['songid'])[0]['album'])
        elif (self.path == "/nextsong"):
            output = unidecode("Title: " + client.playlistid(client.status()['nextsongid'])[0]['title'] + "\nArtist: " + client.playlistid(client.status()['nextsongid'])[0]['artist'] + "\nAlbum: " + client.playlistid(client.status()['nextsongid'])[0]['album'])
        elif (self.path == "/currentnext"):
            output = "";
            if client.status().get('songid'):
                output = "Title: " + client.playlistid(client.status()['songid'])[0]['title'] + "\nArtist: " + client.playlistid(client.status()['songid'])[0]['artist'] + "\nAlbum: " + client.playlistid(client.status()['songid'])[0]['album']
            else:
                output += "\nTitle:\nArtist:\nAlbum:"
            if client.status().get('nextsongid'):
                output += "\n" + "NextTitle: " + client.playlistid(client.status()['nextsongid'])[0]['title'] + "\nNextArtist: " + client.playlistid(client.status()['nextsongid'])[0]['artist'] + "\nNextAlbum: " + client.playlistid(client.status()['nextsongid'])[0]['album']
            else:
                output += "\nNextTitle:\nNextArtist:\nNextAlbum:"
            output = unidecode(output)
        else:
            output = ""
        print("Decoded: " + output)
        self.wfile.write(bytes("%s" % output,"ascii"))
        client.disconnect()

if __name__ == "__main__":        
    webServer = HTTPServer((hostName, serverPort), MyServer)
    print("MPC Unidecode Service started http://%s:%s" % (hostName, serverPort))

    try:
        webServer.serve_forever()
    except KeyboardInterrupt:
        pass

    webServer.server_close()
    print("Service stopped.")
