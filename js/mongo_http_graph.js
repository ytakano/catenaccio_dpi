function map() {
    if ('referer' in this) {
        var referers = {};

        referers[this.referer] = 1;
        emit(this.uri, referers);
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

var res = db.requests.mapReduce(map, reduce, {out: {replace: "graph"}});

shellPrint(res);
