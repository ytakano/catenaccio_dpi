function map() {
    var host = this.uri.split('/')[2].split('.');
    var tld  = host[host.length - 1].split(':')[0];

    if (! isNaN(parseInt(tld)))
        return;

    emit(tld, 1);
}

function reduce(key, values) {
    var n = 0;

    values.forEach(function(value) {
        n += value;
    });

    return n;
}

var res = db.requests.mapReduce(map, reduce, {out: {replace: "tld"}});

shellPrint(res);
