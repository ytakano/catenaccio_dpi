var gtlds ={ 'com':  true,
             'net':  true,
             'org':  true,
             'info': true,
             'biz':  true,
             'name': true,
             'coop': true,
             'edu':  true,
             'gov':  true,
             'aero': true,
             'asia': true,
             'cat':  true,
             'int':  true,
             'jobs': true,
             'mil':  true,
             'mobi': true,
             'xxx':  true,
             'tel':  true,
             'museum': true,
             'travel': true };

var jp_second_domains = { 'ac': true,
                          'ad': true,
                          'co': true,
                          'ed': true,
                          'go': true,
                          'gr': true,
                          'lg': true,
                          'ne': true,
                          'or': true };

function map() {
    var uri     = this.uri.split('/')[2];

    emit(uri, 1);

    if (this.referer) {
        var referer = this.referer;

        shellPrint(referer);

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

function trunc_domain(domain) {
    var sp  = domain.split('.').reverse();
    var tld;
    var result;

    if (sp.length < 2)
        return false;

    tld = sp[0];

    if (tld in gtlds) {
        return sp.slice(0, 2).reverse().join('.');
    } else if (tld == 'jp') {
        var sld = sp[1];

        if (sld in jp_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    }

    if (sp.length > 3) {
        return sp.slice(0, 3).reverse().join('.');
    }

    return domain;
}

function trunc_hosts() {
    db.trunc_hosts.remove();
    db.trunc_hosts.ensureIndex({value: 1});

    for (var c = db.hosts.find(); c.hasNext(); ) {
        var domain = c.next()['_id'];
        var tr_domain = trunc_domain(domain);

        shellPrint([domain, tr_domain]);

        if (domain != tr_domain) {
            db.trunc_hosts.save({_id: domain, value: tr_domain});
        }
    }
}

var res = db.requests.mapReduce(map, reduce, {out: {replace: "hosts"}});

shellPrint(res);

trunc_hosts();
