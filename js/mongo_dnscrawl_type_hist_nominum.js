function map() {
    try {
        if ('ver' in this && 'type' in this && this.type == 'Nominum Vantio') {
            emit(this.type_ver, 1);
        }
    } catch (e) {
    }
}

function reduce(key, values) {
    var count = 0;

    values.forEach(function(value) {
        count += value;
    });

    return count;
}

var res = db.servers.mapReduce(map, reduce, {out: {replace: "type_hist_nominum"}});

shellPrint(res);
