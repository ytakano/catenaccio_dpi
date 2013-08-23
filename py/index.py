#!/usr/bin/env python
# -*- coding: utf-8 -*-

def application(environ, start_response):
	start_response('200 OK', [('Content-type', 'text/html')])
	return "<html><head><title>Test</title></head><body><div>Hello</div></body></hetml>"
