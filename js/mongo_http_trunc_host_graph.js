function map() {
    if ('referer' in this) {
        var referers   = {};
        var uri        = this.uri.split('/')[2];
        var referer    = this.referer.split('/')[2];
        var tr_uri     = db.trunc_hosts.findOne({_id: uri});
        var tr_referer = db.trunc_hosts.findOne({_id: referer});

        if (tr_uri != null)
            uri = tr_uri['value'];

        if (tr_referer != null)
            referer = tr_referer['value'];

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

var res = db.requests.mapReduce(map, reduce, {out: {replace: 'trunc_host_graph'}});

shellPrint(res);
