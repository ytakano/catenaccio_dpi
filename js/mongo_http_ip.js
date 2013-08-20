function map() {
    if (! 'client ip' in this) {
        return;
    }

    emit(this['client ip'], 1);
}

function reduce(key, values) {
    var n = 0;

    values.forEach(function(value) {
        n += value;
    });

    return n;
}

var res = db.requests.mapReduce(map, reduce, {out: {replace: 'ip'}});

shellPrint(res);
