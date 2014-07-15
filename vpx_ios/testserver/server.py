#!/usr/bin/env python
import json
import os
import SimpleHTTPServer
import SocketServer

def ListFilesWithSuffixAsJSONArray(file_suffixes):
  raw_dir_list = []
  for top, dirs, files in os.walk('./'):
    for nm in files:       
      raw_dir_list.append(os.path.join(top, nm))

  dir_list = []
  for path in raw_dir_list:
    if path.endswith(file_suffixes):
      dir_list.append(path.replace('./', ''))

  json_dir_list = json.dumps(dir_list)
  return json_dir_list

class VPXTestServer(SimpleHTTPServer.SimpleHTTPRequestHandler):
  def do_GET(self):
    if self.path == "/ivf":
      json_dir_list = ListFilesWithSuffixAsJSONArray(("ivf"))
    elif self.path == "/webm":
      json_dir_list = ListFilesWithSuffixAsJSONArray(("webm"))
    elif self.path == "/allvpx":
      json_dir_list = ListFilesWithSuffixAsJSONArray(("ivf", "webm"))
    else:
      SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)
      return

    self.send_response(200)
    self.send_header('Content-type', 'application/json')
    self.end_headers()
    self.wfile.write(json_dir_list)

def main():
  try:
    port = 8000
    httpd = SocketServer.TCPServer(("", port), VPXTestServer)
    print "Started VPXTestServer on port {}.".format(port)
    httpd.serve_forever()
  except KeyboardInterrupt:
    print 'stopping server.'
    httpd.socket.close()

if __name__ == '__main__':
  main()
