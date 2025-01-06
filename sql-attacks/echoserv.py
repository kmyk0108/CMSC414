#! /usr/bin/env python3

import http.server
import socketserver
import sys

PORT = 5555

class Q4RequestHandler(http.server.SimpleHTTPRequestHandler):
   def do_GET(self):
      print(self.path)
      self.send_response(200)
      self.end_headers()
      self.wfile.write(self.path.encode())

class absorber(object):
   def write(self, *args, **kwargs):
      pass

Handler = Q4RequestHandler

httpd = socketserver.TCPServer(("", PORT), Handler)

print("serving at port", PORT)
sys.stderr = absorber()
httpd.serve_forever()

