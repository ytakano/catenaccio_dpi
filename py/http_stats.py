#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import json
import getopt, sys
import os
import subprocess

try:
    import matplotlib.pyplot as plt
    is_plt = True
except ImportError:
    is_plt = False

class http_stats:
    def __init__(self, server, outdir):
        self._con    = pymongo.Connection(server)
        self._outdir = outdir

        self._soa_rname = {}
        self._in_graph  = {}
        self._out_graph = {}
        self._max_ref   = 0.0
        self._top_n     = set()
        self._total_ref = 0

        self._dot_full   = 'http_full_graph.dot'
        self._dot_pruned = 'http_pruned_graph.dot'

        self._html = """
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja" lang="ja">
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <style type="text/css">
      body { margin: 20px; }
      a:hover {color: red}

      div.content { margin: 20px; margin-top: 0px; }

      h3.soa { margin-bottom: 5px;
               margin-top: 0px;
               padding-left: 3px;
               border-left: 7px solid #000000; }
      div.refs { border-left: 1px solid #000000;
                 border-bottom: 1px solid #000000;
                 padding: 5px;
                 margin-bottom: 10px; }
      span.dst { margin-right : 10px; }
      div.small { font-size: small; margin-bottom: 5px; }
      div.bold { font-weight: bold;
                 padding-left: 3px;
                 border-left: 4px solid #000000;
                 border-bottom: 1px solid #000000; }

      span.src { margin-right: 8px; }
      div.srcs { line-height: 1.5em; }
    </style>

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

function show_stats(uri, hosts, trds, id_host, id_refered, id_truncated) {
    var div  = document.getElementById(id_host);
    var text = document.createTextNode(uri);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    div.appendChild(text);

    set_hosts(id_refered, hosts);
    set_hosts(id_truncated, trds);
}

function show_stats_soa(uri, hosts, trds, soa, id_host, id_refered, id_truncated, id_soa) {
    var div  = document.getElementById(id_soa);
    var text = document.createTextNode(soa);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    div.appendChild(text);

    show_stats(uri, hosts, trds, id_host, id_refered, id_truncated);
}
-->
</script>

    <title>Cattenacio DPI: HTTP Statistics</title>
  </head>
  <body>
    <h1>HTTP Statistics</h1>

    <h2>Most Refered URLs</h2>
    <div class="content">
      <table>
        <tr valign="top">
          <td width=65%%>%(top_n)s</td>
          <td width=30%%>
            <div class="bold">host</div>
            <div id="host_dst" class="small"></div>
            <div class="bold">refered by</div>
            <div id="refered_dst" class="small"></div>
            <div class="bold">truncated URLs</div>
            <div id="truncated_dst" class="small"></div>
          </td>
        </tr>
      </table>
    </div>

    <hr>

    <h2>Sites Refering Most Refered URLs</h2>
    <div class="content">

      <table>
        <tr valign="top">
          <td width=65%%>%(refs)s</td>
          <td width=30%%>
            <div class="bold">host</div>
            <div id="host_src" class="small"></div>
            <div class="bold">SOA RNAME</div>
            <div id="soa_src" class="small"></div>
            <div class="bold">refering</div>
            <div id="refered_src" class="small"></div>
            <div class="bold">truncated URLs</div>
            <div id="truncated_src" class="small"></div>
          </td>
        </tr>
      </table>
    </div>

    <hr>

    <h2>HTTP Refered Graph</h2>
    <div class="content">
      <a href="http_full_graph.dot">download Graphviz dot file</a>
    </div>
  </body>
</html>
"""

    def _get_full_dot(self):
        dot = 'digraph http_referre{\ngraph [rankdir = LR];\n'

        for dst, srcs in self._in_graph.items():
            for src in srcs:
                dot += '"%s" -> "%s";\n' % (src, dst)

        dot += '}'

        return dot

    def _get_graph(self):
        db = self._con.HTTP

        for i in db.graph_trunc_host.find():
            dst  = i['_id']
            srcs = i['value']

            self._in_graph[dst] = srcs
            self._total_ref += len(srcs)

            for src in srcs:
                if src in self._out_graph:
                    self._out_graph[src].append(dst)
                else:
                    self._out_graph[src] = [dst]

    def _get_soa_rname(self):
        db = self._con.HTTP

        for dst in self._in_graph.keys():
            soa = db.soa.find_one({'_id': dst})

            if soa == None:
                continue

            rname = soa['rname']
            elm   = {'dst': dst, 'srcs': self._in_graph[dst]}

            if rname in self._soa_rname:
                self._soa_rname[rname].append(elm)
            else:
                self._soa_rname[rname] = [elm]

    def _get_top_n_refered(self, n):
        soa = sorted(self._soa_rname.items(),
                     key = lambda x: sum([len(refs['srcs']) for refs in x[1]]),
                     reverse = True)

        return soa[0:n]

    def _get_top_n_leaking(self, n):
        html = '<div class="srcs">'
        uri  = {}

        for k, v in self._out_graph.items():
            val = 0
            for dst in v:
                try:
                    score = (float(len(self._in_graph[dst])) / self._total_ref) ** 2
                    val += score
                except:
                    continue

            uri[k] = val

        sites = sorted(uri.items(), key = lambda x: x[1], reverse = True)[0:n]

        if len(sites) == 0:
            return ''

        i = 0
        for k, v in sites:
            score = round(v * 100, 2)

            db = self._con.HTTP
            trd_hosts = []

            for trd in db.trunc_hosts.find({"value": k}):
                trd_hosts.append(trd['_id'])

            soa   = db.soa.find_one({'_id': k})
            rname = ''

            if soa != None:
                rname = soa['rname']

            params = {'src':    k,
                      'weight': 125 - i * 1,
                      'score':  score,
                      'uris':   json.dumps(self._out_graph[k]),
                      'truncated': json.dumps(trd_hosts),
                      'soa': rname}

            i += 1

            html += '<span class="src"><a style="text-decoration: none; color: rgb(0, 0, 0);" href="javascript:void(0);" onclick=\'show_stats_soa("%(src)s", %(uris)s, %(truncated)s, "%(soa)s", "host_src", "refered_src", "truncated_src", "soa_src")\'>%(src)s(%(score)2.2f)</a></span> ' % params

        html += '</div>'

        return html

    def _get_top_n_html(self, n):
        html = ''

        top_n = self._get_top_n_refered(n)

        max_ref = 0
        for (soa, refs) in top_n:
            for ref in refs:
                if max_ref < len(ref['srcs']):
                    max_ref = len(ref['srcs'])

        for (soa, refs) in top_n:
            html += '<div class="refs"><h3 class="soa">' + soa + '</h3>'

            for ref in refs:
                len_src = len(ref['srcs'])

                db = self._con.HTTP
                trd_hosts = []

                for trd in db.trunc_hosts.find({"value": ref['dst']}):
                    trd_hosts.append(trd['_id'])

                params = {'dst': ref['dst'],
                          'len': len(ref['srcs']),
                          'uris': json.dumps(ref['srcs'].keys()),
                          'truncated': json.dumps(trd_hosts),
                          'color': 150 - int(150.0 * len_src / max_ref),
                          'weight': 80.0 + 150.0 * len_src / max_ref}

                html += '<span class="dst" style="font-size: %(weight)d%%;"><a style="text-decoration: none; color: rgb(%(color)d, %(color)d, %(color)d);" href="javascript:void(0);" onclick=\'show_stats("%(dst)s", %(uris)s, %(truncated)s, "host_dst", "refered_dst", "truncated_dst")\'>%(dst)s(%(len)d)</a></span> ' % params

            html += '</div>'

        return html

    def print_html(self):
        self._get_graph()
        self._get_soa_rname()

        dot = self._get_full_dot()
        open(os.path.join(self._outdir, self._dot_full), 'w').write(dot)

        print self._html % {'top_n': self._get_top_n_html(10),
                            'refs' : self._get_top_n_leaking(50) }
        

html = ''
odir = './'
mongo = 'localhost:27017'
ref_graph = {}
out_graph = {}

def get_graph(db):
    global ref_graph
    global out_graph

    for i in db.graph_trunc_host.find():
        dst  = i['_id']
        srcs = i['value'].keys()

        ref_graph[dst] = srcs

        for src in srcs:
            if src in out_graph:
                out_graph[src].append(dst)
            else:
                out_graph[src] = [dst]

def top_n_uri(graph, num):
    total = 0
    ret   = []

    return [(k, v) for k, v in sorted(graph.items(), key = lambda x: len(x[1]),
                                      reverse = True)[0:num]]

def print_tld(db):
    global odir
    global is_plt
    global html

    if not is_plt:
        return

    total = 0
    tlds  = {}

    for tld in db.tld.find():
        total += tld['value']
        tlds[tld['_id']] = tld['value']

    total = float(total)

    tlds['others'] = 0

    for k in tlds.keys():
        if k == 'others':
            continue

        per = tlds[k] / total
        if per < 0.1:
            tlds['others'] += tlds[k]
            del tlds[k]

    labels = [k for k, v in sorted(tlds.items(), key = lambda x:x[1],
                                   reverse = True)]
    values = [tlds[k] for k in labels]

    plt.clf()
    plt.figure(figsize=(6,6))

    plt.pie(values, labels = labels, autopct = '%1.1f%%', shadow = True,
            colors = [(1.0, 1.0, 1.0)] * len(labels))
    plt.savefig(os.path.join(odir, 'tld_dist.png'), dpi = 80)

    html += """
<hr>
<h2>TLD Distribution</h2>
<div class="content">
  <div style="text-align: center;"><img src="tld_dist.png"></div>
</div>
"""

def print_top_50(db):
    global html
    global ref_graph
    global out_graph

    best_ref = top_n_uri(ref_graph, 50)
    best_out = top_n_uri(out_graph, 50)

    max_ref = len(best_ref[0][1])
    max_out = len(best_out[0][1])

    best_ref.sort()
    best_out.sort()

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

function show_stats(uri, hosts, trds) {
    var div  = document.getElementById("host");
    var text = document.createTextNode(uri);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    div.appendChild(text);

    set_hosts("referred", hosts);
    set_hosts("truncated", trds);
}
-->
</script>
"""

    html += '<div class="top50">'
    html += '<h2>Top 50 most referred domains</h2>'
    html += '<div class="content">'

    html += '<div>'

    for dst, src in best_ref:
        w = 150 - int(150.0 * len(src) / max_ref)

        trd_hosts = []

        for trd in db.trunc_hosts.find({"value": dst}):
            trd_hosts.append(trd['_id'])

        html += '<span style="font-size: %d%%;">' % (70.0 + 200.0 * len(src) / max_ref)
        html += '<a style="text-decoration: none;  color: rgb(%d, %d, %d);" href="javascript:void(0);" onclick=\'show_stats("%s", %s, %s)\'>' % (w, w, w, dst, json.dumps(src), json.dumps(trd_hosts))
        html += '%s(%d)</a>&nbsp;' % (dst, len(src))
        html += '</span>\n'

    html += '</div></div><hr>'

    html += '<h2>Top 50 mont originated domains</h2>'
    html += '<div class="content">'

    html += '<div>'

    for src, dst in best_out:
        w = 150 - int(150.0 * len(dst) / max_out)

        trd_hosts = []

        for trd in db.trunc_hosts.find({"value": src}):
            trd_hosts.append(trd['_id'])

        html += '<span style="font-size: %d%%;">' % (70.0 + 200.0 * len(dst) / max_out)
        html += '<a style="text-decoration: none;  color: rgb(%d, %d, %d);" href="javascript:void(0);" onclick=\'show_stats("%s", %s, %s)\'>' % (w, w, w, src, json.dumps(dst), json.dumps(trd_hosts))
        html += '%s(%d)</a>&nbsp;' % (src, len(dst))
        html += '</span>\n'

    html += '</div></div></div>'

    html += """
<div class="stats">
  URI:
  <div id="host" class="in_stats"></div>
  referre(d):
  <div id="referred" class="in_stats"></div>
  truncated URIs:
  <div id="truncated" class="in_stats"></div>
</div>
<div style="clear: both;"></div>
"""

def print_degree():
    global html
    global odir
    global is_plt
    global ref_graph
    global out_graph

    dot = 'digraph http_referre{\ngraph [rankdir = LR];\n'
    in_dist  = {}
    out_dist = {}

    nodes = set()
    total_links = 0

    for dst, srcs in ref_graph.items():
        n = len(srcs)

        total_links += n
        nodes.add(dst)

        if n in in_dist:
            in_dist[n] += 1
        else:
            in_dist[n] = 1

        for src in srcs:
            dot += '"%s" -> "%s";\n' % (src, dst)
            nodes.add(src)

    for src, dsts in out_graph.items():
        n = len(dsts)

        if n in out_dist:
            out_dist[n] += 1
        else:
            out_dist[n] = 1

    dot += '}'
    open(os.path.join(odir, 'http_graph.dot'), 'w').write(dot)

    in_dist_x = sorted(in_dist.keys())
    in_dist_y = [in_dist[x] for x in in_dist_x]

    out_dist_x = sorted(out_dist.keys())
    out_dist_y = [out_dist[x] for x in out_dist_x]

    if is_plt:
        plt.loglog(in_dist_x, in_dist_y, 'ro-')
        plt.loglog(out_dist_x, out_dist_y, 'bv:')
        plt.xlabel('degree')
        plt.ylabel('count')
        plt.legend(('incoming', 'outgoing'), loc='upper right')
        plt.savefig(os.path.join(odir, 'http_dist.png'), dpi = 80)

    html += """
<h2>Referred graph and degree distribution</h2>
<div class="content">
"""

    try:
        cmd = 'fdp -Tpng %s -o %s' % (os.path.join(odir, 'http_graph.dot'),
                                          os.path.join(odir, 'http_graph.png'))

        subprocess.call(cmd, shell=True)
        html += '<p><a href="http_graph.png">download HTTP graph</a></p>'
    except:
        pass

    html += """
<p><a href="http_graph.dot">download GraphViz dot file</a></p>
<table>
  <tr>
    <td>Total nodes: </td>
    <td>%d</td>
  </tr>
  <tr>
    <td>Total links: </td>
    <td>%d</td>
  </tr>
</table>
""" % (len(nodes), total_links)

    if is_plt:
        html += """
<div style="text-align: center;">
  <img src="http_dist.png"><br>
  Degree Distribution
</div>
"""

    html += '</div>'


def print_html():
    global html

    html += """
<html>
  <head>
    <style type="text/css">
      body { margin: 20px; }
      a:hover {color: red}

      div.content { margin: 20px; margin-top: 0px; }
      div.in_stats { margin: 10px; margin-top: 2px; font-size: 80%;
                     color: gray; }
      div.stats { padding: 20px; }
      div.top50 { float: left; width: 65%; padding-right: 10px; }
    </style>
    <title>HTTP Statistics</title>
  </head>
  <body>
  <h1>HTTP Statistics</h1>
"""

    con = pymongo.Connection(mongo)
    db  = con.HTTP

    get_graph(db)

    print_top_50(db)

    html += '<hr>'

    print_degree()
    print_tld(db)

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

    stats = http_stats(server, outdir)
    stats.print_html()
