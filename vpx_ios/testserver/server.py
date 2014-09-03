#!/usr/bin/python
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
"""
VPXTestServer - A SimpleHTTPServer based web server which provides file listings
                in JSON to clients, and allows download of those files.
"""
import BaseHTTPServer
import json
import os
import SimpleHTTPServer
import SocketServer
import sys

def list_files_with_suffix_as_json(file_suffixes):
    """
    Returns JSON object containing all filenames ending with the specified
    suffix in the current directory.
    """
    raw_dir_list = []
    for (top, dirs, files) in os.walk('./'):
        for name in files:
            raw_dir_list.append(os.path.join(top, name))

    dir_list = []
    for path in raw_dir_list:
        if path.endswith(file_suffixes):
            dir_list.append(path.replace('./', ''))

    json_dir_list = json.dumps(dir_list)
    return json_dir_list


class ThreadedHTTPServer(BaseHTTPServer.HTTPServer,
                         SocketServer.ThreadingMixIn):
    """
    BaseHTTPServer with concurrency support.
    """
    pass

class VPXTestServer(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """
    VPXTestServer - SimpleHTTPRequestHandler that implements do_GET to provide
    file listings and the files in the listings to clients.
    """
    def do_GET(self):
        if self.path == '/ivf':
            json_dir_list = list_files_with_suffix_as_json('ivf')
        elif self.path == '/webm':
            json_dir_list = list_files_with_suffix_as_json('webm')
        elif self.path == '/allvpx':
            json_dir_list = list_files_with_suffix_as_json(('ivf', 'webm'))
        else:
            SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)
            return

        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json_dir_list)


def main():
    """
    Parses command line for a single argument: The port on which the HTTP server
    will listen for incoming requests. Then kicks off the server and runs until
    it dies or there's a keyboard interrupt.
    """
    try:
        if len(sys.argv) > 1:
            port = int(sys.argv[1])
        else:
            port = 8000

        httpd = ThreadedHTTPServer(('', port), VPXTestServer)
        print 'Started VPXTestServer on port {}.'.format(port)
        httpd.serve_forever()
    except KeyboardInterrupt:
        print 'stopping server.'
        httpd.socket.close()


if __name__ == '__main__':
    main()
