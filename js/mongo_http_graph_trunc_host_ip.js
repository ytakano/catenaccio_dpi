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

var ae_second_domains = { 'net':  true,
                          'gov':  true,
                          'ac':   true,
                          'sch':  true,
                          'org':  true,
                          'mil':  true,
                          'pro':  true,
                          'name': true };

var au_second_domains = { 'com':      true,
                          'net':      true,
                          'org':      true,
                          'edu':      true,
                          'gov':      true,
                          'csiro':    true,
                          'asn':      true,
                          'id':       true,
                          'act':      true,
                          'nsw':      true,
                          'nt':       true,
                          'qld':      true,
                          'sa':       true,
                          'tas':      true,
                          'vic':      true,
                          'wa':       true,
                          'archie':   true,
                          'conf':     true,
                          'gw':       true,
                          'info':     true,
                          'otc':      true,
                          'oz':       true,
                          'telememo': true };

var co_second_domains = { 'com': true,
                          'org': true,
                          'edu': true,
                          'gov': true,
                          'net': true,
                          'mil': true,
                          'nom': true };

var es_second_domains = { 'com': true,
                          'nom': true,
                          'org': true,
                          'gob': true,
                          'edu': true };

var fr_second_domains = { 'tm':     true,
                          'asso':   true,
                          'nom':    true,
                          'prd':    true,
                          'presse': true,
                          'com':    true,
                          'gouv':   true };

var it_second_domains = { 'gov': true,
                          'edu': true };

var ly_second_domains = { 'com': true,
                          'net': true,
                          'gov': true,
                          'plc': true,
                          'edu': true,
                          'sch': true,
                          'med': true,
                          'org': true,
                          'id':  true };

var uk_second_domains = { 'ac':     true,
                          'co':     true,
                          'gov':    true,
                          'ltd':    true,
                          'me':     true,
                          'mod':    true,
                          'net':    true,
                          'nic':    true,
                          'nhs':    true,
                          'org':    true,
                          'plc':    true,
                          'police': true,
                          'sch':    true };

var st_second_domains = { 'nic':       true,
                          'gov':       true,
                          'saotome':   true,
                          'principle': true,
                          'consulado': true,
                          'embaixada': true,
                          'org':       true,
                          'edu':       true,
                          'net':       true,
                          'com':       true,
                          'sotre':     true,
                          'mil':       true,
                          'co':        true };

var trunc_domain = function(domain) {
    var sp  = domain.split('.').reverse();
    var tld;
    var sld;
    var result;

    if (sp.length < 2)
        return false;

    tld = sp[0];

    if (tld in gtlds) {
        return sp.slice(0, 2).reverse().join('.');
    } else if (tld == 'jp') {
        sld = sp[1];

        if (sld in jp_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'ae') {
        sld = sp[1];

        if (sld in ae_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'au') {
        sld = sp[1];

        if (sld in au_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'co') {
        sld = sp[1];

        if (sld in co_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'es') {
        sld = sp[1];

        if (sld in es_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'fr') {
        sld = sp[1];

        if (sld in fr_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'it') {
        sld = sp[1];

        if (sld in it_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'ly') {
        sld = sp[1];

        if (sld in ly_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'uk') {
        sld = sp[1];

        if (sld in uk_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (tld == 'st') {
        sld = sp[1];

        if (sld in st_second_domains) {
            return sp.slice(0, 3).reverse().join('.');
        } else {
            return sp.slice(0, 2).reverse().join('.');
        }
    } else if (parseInt(tld, 10) != NaN) {
        return domain;
    }

    if (sp.length > 3) {
        return sp.slice(0, 3).reverse().join('.');
    }

    return domain;
}

db.system.js.save({"_id": "trunc_domain", "value": trunc_domain});
db.system.js.save({"_id": "gtlds", "value": gtlds});
db.system.js.save({"_id": "jp_second_domains", "value": jp_second_domains});
db.system.js.save({"_id": "ae_second_domains", "value": ae_second_domains});
db.system.js.save({"_id": "fr_second_domains", "value": fr_second_domains});
db.system.js.save({"_id": "co_second_domains", "value": co_second_domains});
db.system.js.save({"_id": "ly_second_domains", "value": ly_second_domains});
db.system.js.save({"_id": "uk_second_domains", "value": uk_second_domains});

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

        uri     = trunc_domain(uri);
        referer = trunc_domain(referer);

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
