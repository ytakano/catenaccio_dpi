#!/bin/sh

js_files="mongo_http_hosts.js mongo_http_graph_trunc_host.js mongo_http_tld.js"

js_path=`dirname $0`

if [ $# -ge 1 ]; then
    host=$1/HTTP
else
    host=HTTP
fi

for i in ${js_files}
do
    echo "${host} ${js_path}/${i}"
    mongo ${host} ${js_path}/${i}
done
