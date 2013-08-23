#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import hashlib

def application(environ, start_response):
	start_response('200 OK', [('Content-type', 'text/html')])

        m = hashlib.sha1()
        m.update(environ['REMOTE_ADDR'])
        digest = m.hexdigest()

        p = os.path.join(os.getcwd(), 'data', digest, 'http_stats.html')

        try:
            html = open(p).read()
        except:
            return """
<html>
  <head>
    <title>Access Later Please</title>
  </head>
  <body>
    Your statistics isn't calculated yet.<br>
    Please access again later.<br>
    %s
  </body>
</html>""" % (p)

        return html
