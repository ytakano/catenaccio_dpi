#!/bin/sh

js_http_files="mongo_http_hosts.js mongo_http_graph_trunc_host.js mongo_http_tld.js"

js_path=`dirname $0`

if [ $# -ge 1 ]; then
    host_http=$1/HTTP
else
    host_http=HTTP
fi

for i in ${js_http_files}
do
    echo "${host_http} ${js_path}/${i}"
    mongo ${host_http} ${js_path}/${i}
done
