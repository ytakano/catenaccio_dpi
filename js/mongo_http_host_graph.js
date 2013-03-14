function map() {
    if ('referer' in this) {
        var referers = {};
        var uri = this.uri.split('/')[2];
        var referer = this.referer.split('/')[2];

        if (uri == referer)
            return;

        referers[referer] = 1;
        emit(uri, referers);
    }
}

function reduce(key, values) {
    var referers = {};

    values.forEach(function(value) {
        for (var i in value) {
            if (i in referers)
                referers[i] += value[i];
            else
                referers[i] = value[i];
        }
    });

    return referers;
}

var res = db.requests.mapReduce(map, reduce, {out: {replace: 'host_graph'}});

shellPrint(res);
