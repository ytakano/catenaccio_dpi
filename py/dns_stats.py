#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import os, sys, getopt
import socket

class dns_stats:
    def __init__(self, server, outdir):
        self._con    = pymongo.Connection(server)
        self._outdir = outdir

        self._html = """
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja" lang="ja">
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <style type="text/css">
      body { margin: 20px; }
      a:hover {color: red}

      div.content { margin: 20px; margin-top: 0px; text-align: center; }
      table { text-align: right; width: 80%%; border-collapse: collapse; margin-left: auto; margin-right: auto;}
      td, th { border: 2px #c0c0c0 solid; }
    </style>

    <title>Cattenacio DPI: DNS Statistics</title>
  </head>
  <body>
    <h1>DNS Statistics</h1>
%(type_dist)s
%(top_n)s
  </body>
</html>
"""

        self._html_type_dist = """
    <h2>Types of DNS Servers</h2>
    <div class="content">
%(type_dist_table)s
    </div>
"""

        self._html_top_n = """
    <h2>Top 50 Most Accessed DNS Servers</h2>
    <div class="content">
%(top_n_table)s
    </div>
"""

    def print_type_dist(self):
        db = self._con.DNS

        dist_all     = {'total': 0}
        dist_apnic   = {'total': 0}
        dist_ripe    = {'total': 0}
        dist_arin    = {'total': 0}
        dist_lacnic  = {'total': 0}
        dist_afrinic = {'total': 0}
        dist_other   = {'total': 0}

        for data in db.type_dist_all.find():
            dist_all[data['_id']] = data['value']

        for data in db.type_dist_apnic.find():
            dist_apnic[data['_id']] = data['value']

        for data in db.type_dist_ripe.find():
            dist_ripe[data['_id']] = data['value']

        for data in db.type_dist_arin.find():
            dist_arin[data['_id']] = data['value']

        for data in db.type_dist_lacnic.find():
            dist_lacnic[data['_id']] = data['value']

        for data in db.type_dist_afrinic.find():
            dist_afrinic[data['_id']] = data['value']

        for data in db.type_dist_other.find():
            dist_other[data['_id']] = data['value']

        html = """
      <table class="type_dist">
        <tr>
          <td></td>
          <th>All</th>
          <th>APNIC</th>
          <th>RIPE</th>
          <th>ARIN</th>
          <th>LACNIC</th>
          <th>AFRINIC</th>
          <th>other</th>
        </tr>
%s
      </table>
"""

        elm = ''

        for k, v in sorted(dist_all.items(), key=lambda x:x[1], reverse=True):
            if k == 'total':
                continue

            row = '        <tr><th>' + k + '</th>'

            row += '<td>' + str(int(v)) + '</td>'

            if k in dist_apnic:
                row += '<td>' + str(int(dist_apnic[k])) + '</td>'
            else:
                row += '<td>-</td>'

            if k in dist_ripe:
                row += '<td>' + str(int(dist_ripe[k])) + '</td>'
            else:
                row += '<td>-</td>'

            if k in dist_arin:
                row += '<td>' + str(int(dist_arin[k])) + '</td>'
            else:
                row += '<td>-</td>'

            if k in dist_lacnic:
                row += '<td>' + str(int(dist_lacnic[k])) + '</td>'
            else:
                row += '<td>-</td>'

            if k in dist_afrinic:
                row += '<td>' + str(int(dist_afrinic[k])) + '</td>'
            else:
                row += '<td>-</td>'

            if k in dist_other:
                row += '<td>' + str(int(dist_other[k])) + '</td>'
            else:
                row += '<td>-</td>'

            row += '</tr>\n'

            elm += row

        elm += '        <tr><th>Total</th><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>' % (dist_all['total'], dist_apnic['total'], dist_ripe['total'], dist_arin['total'], dist_lacnic['total'], dist_afrinic['total'], dist_other['total'])

        html = html % elm

        self._html_type_dist = self._html_type_dist % {'type_dist_table': html}

    def print_top_n(self, num):
        db = self._con.DNS

        n = 0

        html = """
      <table>
        <tr>
          <th>IP Address</th>
          <th>FQDN</th>
          <th>Count</th>
          <th>Version</th>
        </tr>
%s
      </table>
"""

        elm = ''

        for data in db.servers.find().sort('counter', -1):
            try:
                addr = socket.gethostbyaddr(data['_id'])[0]
            except:
                addr = ''

            if 'ver' in data:
                elm += '        <tr><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>\n' % (data['_id'], addr, data['counter'], data['ver'])
            else:
                elm += '        <tr><td>%s</td><td>%s</td><td>%d</td><td></td></tr>\n' % (data['_id'], addr, data['counter'])

            n += 1

            if n >= num:
                break

        html = html % elm

        self._html_top_n = self._html_top_n % {'top_n_table': html}

    def print_html(self):
        self.print_type_dist()
        self.print_top_n(50)

        print self._html % {'type_dist': self._html_type_dist,
                            'top_n': self._html_top_n}


def usage():
    print sys.argv[0], '-m mongo_server -o ouput_dir'

if __name__ == '__main__':
    server = 'localhost:27017'
    outdir = os.getcwd()

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hm:o:", ["help", "mongo=", "output="])
    except getopt.GetoptError:
        usage()
        sys.exit(2)

    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        if o in ("-o", "--output"):
            outdir = a
        if o in ("-m", "--mongo"):
            server = a

    stats = dns_stats(server, outdir)
    stats.print_html()
