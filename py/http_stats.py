#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import matplotlib.pyplot as plt
import json
import getopt, sys
import os

html = ''
odir = './'
mongo = 'localhost:27017'

def top_n_ref_trunc_uri(db, n):
    total = 0
    ref   = {}
    ret   = []

    for i in db.graph_trunc_host.find():
        ref[i['_id']] = i['value']
        total += len(i['value'])

    i = 0
    for k, v in sorted(ref.items(), key = lambda x:len(x[1]), reverse = True):
        if i >= n:
            break

        i += 1

        ret.append([k, v.keys()])

    return ret

def print_top_50_ref(db):
    global html

    best_ref = top_n_ref_trunc_uri(db, 50)

    max_ref = len(best_ref[0][1])

    html += '<h2>Top 50 most referred domains</h2>'
    html += '<div class="content">'

    html += """
<script language="JavaScript">
<!--

function set_hosts(id, hosts) {
   var div = document.getElementById(id);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    for (var i = 0; i < hosts.length; i++) {
        var text = document.createTextNode(hosts[i] + "\u00A0\u00A0 ");
        var span = document.createElement('span');

        span.appendChild(text)
        div.appendChild(span);
    }
}

function show_stats(refs, trds) {
    set_hosts("referred", refs);
    set_hosts("truncated", trds);
}
-->
</script>
"""
    tr_uri = []

    best_ref.sort()

    html += '<div>'

    for dst, src in best_ref:
        w = 150 - int(150.0 * len(src) / max_ref)        

        trd_hosts = []

        for trd in db.trunc_hosts.find({"value": dst}):
            trd_hosts.append(trd['_id'])

        html += '<span style="font-size: %d%%;">' % (70.0 + 200.0 * len(src) / max_ref)
        html += '<a style="text-decoration: none;  color: rgb(%d, %d, %d);" href="javascript:void(0);" onclick=\'show_stats(%s, %s)\'>' % (w, w, w, json.dumps(src), json.dumps(trd_hosts))
        html += '%s(%d)</a>&nbsp;' % (dst, len(src))
        html += '</span>\n'

    html += '</div>'

    html += """
<hr>
<div class="stats">
  referred from:
  <div id="referred" class="in_stats"></div>
  truncated:
  <div id="truncated" class="in_stats"></div>
</div>
"""

    html += '</div>'

def print_degree(db):
    global html
    global odir

    dot = 'digraph http_referre{\ngraph [rankdir = LR];\n'
    dist = {}

    for i in db.graph_trunc_host.find():
        n = len(i['value'])

        if n in dist:
            dist[n] += 1
        else:
            dist[n] = 1

        for j in i['value'].keys():
            dot += '"%s" -> "%s";\n' % (j, i['_id'])

    dot += '}'
    open(os.path.join(odir, 'http_graph.dot'), 'w').write(dot)

    plt.loglog(dist.keys(), dist.values())
    plt.xlabel('incoming degree')
    plt.ylabel('count')
    plt.savefig(os.path.join(odir, 'http_dist.png'))

    html += """
<h2>Degree distribution of the referred graph</h2>
<div class="content">
  <div style="text-align: center;"><img src="http_dist.png"></div>
  <a href="http_graph.dot">download GraphViz dot file</a>
</div>
"""

def print_html():
    global html

    html += """
<html>
  <head>
    <style type="text/css">
      body { margin: 20px; }
      a:hover {color: red}

      div.content { margin: 20px; margin-top: 0px; }
      div.in_stats { margin: 10px; margin-top: 2px; }
      div.stats { margin: 10px; }
    </style>
    <title>HTTP Statistics</title>
  </head>
  <h1>HTTP Statistics</h1>
  <body>
"""

    con = pymongo.Connection(mongo)
    db  = con.HTTP

    print_top_50_ref(db)

    html += '<hr>'

    print_degree(db)

    html += """
  </body>
</html>
"""

def usage():
    print sys.argv[0], '-m mongo_server -o ouput_dir'

def main():
    global html
    global odir

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
            odir = a
        if o in ("-m", "--mongo"):
            mongo = a

    print_html()

    open(os.path.join(odir, 'http_stats.html'), 'w').write(html)

main()
