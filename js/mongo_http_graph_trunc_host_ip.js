function map() {
    if ('referer' in this) {
        var referers = {};
        var uri;
        var referer;
        var tr_uri;
        var tr_referer;

        try {
            uri     = this.uri.split('/')[2].split(':')[0];
            referer = this.referer.split('/')[2].split(':')[0];
        } catch (e) {
            return;
        }

        tr_uri     = db.trunc_hosts.findOne({_id: uri});
        tr_referer = db.trunc_hosts.findOne({_id: referer});

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

db.requests.ensureIndex({'client ip': 1});

var cur = db.ip.find().addOption(16);

cur.forEach(function(doc) {
    var res = db.requests.mapReduce(map, reduce,
                                    {out: {replace: doc._id},
                                     query: {'client ip': doc._id}});
    shellPrint(res);
});
