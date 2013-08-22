function map() {
    var uri     = this.uri.split('/')[2].split(':')[0];

    emit(uri, 1);

    if (this.referer) {
        try {
            var referer = this.referer.split('/')[2].split(':')[0];
        } catch (e) {
            return;
        }

        emit(referer, 1);
    }
}

function reduce(key, values) {
    var n = 0;

    values.forEach(function(value) {
        n += value;
    });

    return n;
}

var res = db.requests.mapReduce(map, reduce, {out: {replace: "hosts"}});

shellPrint(res);

