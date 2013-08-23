#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os

def application(environ, start_response):
	start_response('200 OK', [('Content-type', 'text/html')])
	return "<html><head><title>Test</title></head><body>AAA" + os.getcwd() + "<div>Hello</div></body></hetml>"
