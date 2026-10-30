.pragma library
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A pretend live chat for the video player. Nobody in it is real and nothing is streamed: every
// viewer, message and viewer count is calculated on the spot from (seed, second of the video), in the
// spirit of Populated mode in the author's Anti-Social project. The same video always gives the same
// chat at the same moment, so seeking just recalculates it, and nothing is ever saved.
//
// The chatters are friendly ("sane mode"): they react to what is on screen's details that the user filled
// in on the movie's page (title, year, genre, cast, director, studio, tagline, tags) and, when the user
// types something, they answer it. No network, no model, only word lists and templates.

/* ---------- seeded randomness ---------- */
function hash32(str) {
    var h = 2166136261 >>> 0;
    for (var k = 0; k < str.length; k++) { h ^= str.charCodeAt(k); h = Math.imul(h, 16777619); }
    h ^= h >>> 13; h = Math.imul(h, 0x5bd1e995); h ^= h >>> 15;
    return h >>> 0;
}
function mulberry32(a) {
    return function () {
        a = (a + 0x6D2B79F5) | 0;
        var t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}
function rng(seed) {
    var key = String(seed);
    for (var i = 1; i < arguments.length; i++) key += '|' + arguments[i];
    return mulberry32(hash32(key));
}
function pick(r, arr) { return arr[Math.floor(r() * arr.length)]; }
function cap(s) { return s.charAt(0).toUpperCase() + s.slice(1); }

/* ---------- viewers ---------- */
var ONSET = ['b', 'br', 'c', 'ch', 'd', 'dr', 'f', 'g', 'gr', 'h', 'j', 'k', 'kl', 'l', 'm', 'n', 'p', 'pr', 'r', 's', 'sh', 'st', 't', 'tr', 'v', 'w', 'z', 'm', 'l', 'n', 'r', 's', 't'];
var VOWEL = ['a', 'e', 'i', 'o', 'u', 'a', 'e', 'i', 'o', 'ai', 'ea', 'ia', 'ou'];
var CODA = ['', '', 'n', 'l', 's', 'a', 'y', 'el', 'ia', 'o', 'er', 'ie', 'in'];
var WORDS = ['moth', 'toast', 'pixel', 'cloud', 'lamp', 'otter', 'maple', 'comet', 'mango', 'fern', 'echo', 'biscuit', 'rocket', 'pebble', 'cocoa', 'orbit', 'badger', 'noodle', 'velvet', 'sunny'];
var TAILS = ['tv', 'irl', 'plays', 'xo', 'hq', 'daily', 'official', 'live', 'gg'];

function hsl2hex(h, s, l) {
    s /= 100; l /= 100;
    var a = s * Math.min(l, 1 - l);
    function f(n) {
        var k = (n + h / 30) % 12;
        var c = l - a * Math.max(-1, Math.min(k - 3, Math.min(9 - k, 1)));
        var x = Math.round(255 * c).toString(16);
        return x.length < 2 ? '0' + x : x;
    }
    return '#' + f(0) + f(8) + f(4);
}

// A person in the crowd. Same (seed, i), same person. Badges: mod, member, vip, or none.
function viewer(seed, i) {
    var r = rng(seed, 'viewer', i);
    var first = pick(r, ONSET) + pick(r, VOWEL) + (r() < 0.5 ? pick(r, ONSET) + pick(r, VOWEL) : '') + pick(r, CODA);
    var kind = r();
    var name;
    if (kind < 0.45) name = first;
    else if (kind < 0.7) name = first + (10 + Math.floor(r() * 990));
    else if (kind < 0.85) name = pick(r, WORDS) + '_' + first;
    else name = first + '_' + pick(r, TAILS);
    name = name.toLowerCase().replace(/[^a-z0-9_]/g, '').slice(0, 15);
    var b = r();
    var badge = b < 0.015 ? 'mod' : b < 0.04 ? 'vip' : b < 0.2 ? 'member' : '';
    return { id: i, name: name, color: hsl2hex(Math.floor(r() * 360), 55 + Math.floor(r() * 25), 62 + Math.floor(r() * 10)), badge: badge };
}
var CROWD = 600;   // different people who can show up for one video

// How many are "watching". Wobbles slowly; always at least a couple.
function viewerCount(seed, sec) {
    var r = rng(seed, 'count');
    var base = 18 + Math.floor(Math.pow(10, 1 + r() * 2.2));
    var ph = r() * 6.28;
    return Math.max(2, Math.round(base * (1 + 0.07 * Math.sin(sec / 95 + ph) + 0.03 * Math.sin(sec / 17 + ph * 2))));
}

/* ---------- what the movie page says, tidied for use in chat ---------- */
function list(s) { return String(s || '').split(/[,;]+/).map(function (x) { return x.trim(); }).filter(function (x) { return x.length > 1 && x.length < 40; }); }
function context(info, durationMs) {
    var meta = (info && info.meta) || {};
    var cast = (info && info.cast) || [];
    var actors = [], roles = [];
    for (var i = 0; i < cast.length; i++) {
        if (cast[i].name && cast[i].name.trim()) actors.push(cast[i].name.trim());
        if (cast[i].role && cast[i].role.trim()) roles.push(cast[i].role.trim());
    }
    var tag = String(meta.tagline || '').trim();
    return {
        title: String(meta.title || '').trim().slice(0, 60),
        year: /^\d{4}$/.test(String(meta.year || '').trim()) ? String(meta.year).trim() : '',
        genres: list(meta.genre),
        directors: list(meta.director),
        studios: list(meta.studio),
        tags: list(meta.tags),
        actors: actors,
        roles: roles,
        tagline: tag.length > 0 && tag.length <= 70 ? tag : '',
        durationMs: durationMs > 0 ? durationMs : 0
    };
}

/* ---------- message text ---------- */
var REACT = ['lol', 'lmao', 'haha', 'no way', 'wait what', 'oh no', 'here we go', 'this part', 'nice', 'love this', 'yesss', 'wow', 'ok that was good', 'hahaha', 'fair', 'true', 'same', 'mood', 'so good', 'classic', 'ha', 'ohh', 'okay okay', 'amazing', 'good one'];
var GREET = ['hi chat', 'hello everyone', 'good evening all', 'hey hey', 'first time watching this', 'watching with snacks', 'late night watch gang', 'back for another rewatch', 'hi from the couch', 'is the volume ok for everyone?', 'who else is watching right now?'];
var START = ['just got here, what did i miss', 'popcorn is ready', 'here we go', 'let us begin', 'starting strong'];
var ENDING = ['already almost over', 'credits soon', 'that was great', 'what a ride', 'thanks for the watch party', 'gg', 'that ending though', 'one more after this?'];
var EMOTES = ['smile', 'laugh', 'wow', 'heart', 'fire', 'clap', 'gg', 'cry'];
var NOTICE = ['{n} joined the chat', 'welcome in, {n}', '{n} is here', 'slow mode is on, take your time', 'be kind in chat, everyone is just here to watch'];
var CLOSERS = ['', '', '', '', ' :smile:', ' :heart:', ' :laugh:', '!!', '!', ' :clap:'];

function t(template, r, c) {
    // a few places in a template need something from the movie page
    return template;
}

// Lines that use the movie page. Each function gets (r, c) and returns text, or '' when the page lacks what it needs.
var MOVIE_LINES = [
    function (r, c) { return c.title ? pick(r, [c.title + ' never gets old', c.title + ' is such a vibe', c.title + '!! love this one', 'this is why i like ' + c.title, 'every time i watch ' + c.title + ' i notice something new']) : ''; },
    function (r, c) { return c.year ? pick(r, [c.year + ' and it still holds up', 'they do not make them like this anymore (' + c.year + ')', c.year + ' was a good year for movies']) : ''; },
    function (r, c) { return c.genres.length ? pick(r, [pick(r, c.genres).toLowerCase() + ' night', 'good ' + pick(r, c.genres).toLowerCase() + ' hits different', pick(r, c.genres).toLowerCase() + ' is my favorite']) : ''; },
    function (r, c) { return c.directors.length ? pick(r, [pick(r, c.directors) + ' knew what they were doing', 'love how ' + pick(r, c.directors) + ' shot this', 'thank you ' + pick(r, c.directors)]) : ''; },
    function (r, c) { return c.actors.length ? pick(r, [pick(r, c.actors) + ' is so good in this', pick(r, c.actors) + ' :laugh:', pick(r, c.actors) + ' stealing every scene', 'we love ' + pick(r, c.actors) + ' :heart:']) : ''; },
    function (r, c) { return c.roles.length ? pick(r, [pick(r, c.roles) + '!! :wow:', 'best part is ' + pick(r, c.roles), pick(r, c.roles) + ' is my favorite']) : ''; },
    function (r, c) { return c.studios.length ? pick(r, [pick(r, c.studios) + ' was cooking back then', 'classic ' + pick(r, c.studios) + ' energy']) : ''; },
    function (r, c) { return c.tagline ? '"' + c.tagline + '" :heart:' : ''; },
    function (r, c) { return c.tags.length ? pick(r, [pick(r, c.tags).toLowerCase() + ' vibes', 'this has big ' + pick(r, c.tags).toLowerCase() + ' energy']) : ''; }
];

// One line about this movie from the page, or '' when the page has nothing to say.
function movieLine(r, c) {
    for (var tries = 0; tries < 4; tries++) { var m = MOVIE_LINES[Math.floor(r() * MOVIE_LINES.length)](r, c); if (m) return m; }
    return '';
}

function spam(r) { var e = ':' + pick(r, EMOTES) + ':'; var n = 2 + Math.floor(r() * 2), s = []; for (var i = 0; i < n; i++) s.push(e); return s.join(' '); }

// One chat line. `at` is the position in the video in ms, `kind` is chosen by the caller.
function lineText(r, c, at) {
    var x = r();
    var inFirst = at < 25000;
    var inLast = c.durationMs > 60000 && at > c.durationMs - 30000;
    if (inFirst && x < 0.35) return pick(r, x < 0.18 ? GREET : START);
    if (inLast && x < 0.4) return pick(r, ENDING);
    if (x < 0.34) {
        // something about this movie, if the page says anything
        for (var tries = 0; tries < 4; tries++) { var m = MOVIE_LINES[Math.floor(r() * MOVIE_LINES.length)](r, c); if (m) return m + pick(r, CLOSERS); }
    }
    var y = r();   // not about the movie (or the page is empty): everyday chat
    if (y < 0.14) return spam(r);
    if (y < 0.24) return pick(r, GREET);
    return pick(r, REACT) + pick(r, CLOSERS);
}

// Splits "hello :laugh: @bob" into [{t:'text'|'emote'|'mention', v}].
function parts(text) {
    var out = [], re = /(:[a-z]+:|@[a-z0-9_]+)/g, last = 0, m;
    while ((m = re.exec(text)) !== null) {
        if (m.index > last) out.push({ t: 'text', v: text.slice(last, m.index) });
        out.push(m[0].charAt(0) === ':' ? { t: 'emote', v: m[0].slice(1, -1) } : { t: 'mention', v: m[0] });
        last = m.index + m[0].length;
    }
    if (last < text.length) out.push({ t: 'text', v: text.slice(last) });
    return out;
}

/* ---------- the schedule ---------- */
// How many messages land in this second: a slow wave, plus a burst now and then.
function rate(seed, sec) {
    var r = rng(seed, 'rate');
    var ph = r() * 6.28, base = 0.7 + r() * 0.6;
    var minute = Math.floor(sec / 60), rb = rng(seed, 'burst', minute);
    var burstAt = Math.floor(rb() * 55), burst = rb() < 0.45 && sec % 60 >= burstAt && sec % 60 < burstAt + 5;
    return Math.max(0.05, base + 0.3 * Math.sin(sec / 41 + ph) + (burst ? 1.4 : 0));
}

// Everything said in one second of the video, in order. A pure function of (seed, sec, the movie page).
function messagesAt(seed, sec, c) {
    var r = rng(seed, 'sec', sec);
    var lam = rate(seed, sec);
    var n = Math.floor(lam) + (r() < lam - Math.floor(lam) ? 1 : 0);
    var out = [];
    for (var j = 0; j < n; j++) {
        var offset = Math.floor(r() * 1000);
        var at = sec * 1000 + offset;
        var v = viewer(seed, Math.floor(r() * CROWD));
        var kindRoll = r();
        var text, kind = 'chat';
        if (kindRoll < 0.04) { text = pick(r, NOTICE).replace('{n}', v.name); kind = 'notice'; }
        else {
            text = lineText(r, c, at);
            if (v.badge === 'member' && r() < 0.18 && text.length > 14) kind = 'highlight';
        }
        out.push({ id: seed + ':' + sec + ':' + j, at: at, name: v.name, color: v.color, badge: v.badge, kind: kind, parts: parts(text), me: false });
    }
    out.sort(function (a, b) { return a.at - b.at; });
    return out;
}

/* ---------- answering the user ---------- */
var HI = /\b(hi|hello|hey|yo|howdy)\b/i, LAUGH = /\b(lol|lmao|haha+|hehe|rofl)\b/i, LOVE = /\b(love|great|best|good|amazing|awesome|favorite|favourite|classic|nice)\b/i,
    THANKS = /\b(thanks|thank you|thx)\b/i;

function replyText(r, user, text, c) {
    var u = '@' + user, low = text.toLowerCase();
    var pickCast = c.actors.filter(function (a) { return low.indexOf(a.toLowerCase().split(' ')[0]) >= 0; })[0];
    if (HI.test(text)) return pick(r, [u + ' hi!! welcome :smile:', 'hey ' + u + ', glad you are here', 'hello ' + u + ' :heart:']);
    if (THANKS.test(text)) return pick(r, ['anytime ' + u, u + ' of course :smile:', 'you are welcome ' + u]);
    if (pickCast) return pick(r, [u + ' yes!! ' + pickCast + ' is the best :heart:', 'agreed ' + u + ', ' + pickCast + ' is great in this', u + ' ' + pickCast + ' :clap:']);
    if (text.indexOf('?') >= 0) {
        // Answer only from what the movie page says, and only the thing that was asked.
        var unsure = [u + ' good question, honestly we are all just watching', u + ' no idea, but i am enjoying it', 'not sure ' + u + ', chat might know'];
        var ask = function (re, known, say) { return re.test(low) ? (known ? pick(r, say) : pick(r, unsure)) : null; };
        var a = ask(/director|directed/, c.directors.length, [u + ' that would be ' + c.directors[0], u + ' i think ' + c.directors[0] + ' directed it'])
             || ask(/\b(who|actor|actress|cast|stars?|starring)\b/, c.actors.length, [u + ' i think that is ' + c.actors[0], u + ' ' + c.actors[0] + ' is in it :smile:'])
             || ask(/\b(year|when)\b/, c.year, [u + ' it came out in ' + c.year, u + ' i think ' + c.year])
             || ask(/\b(genre|kind of|type of)\b/, c.genres.length, [u + ' ' + (c.genres[0] || '').toLowerCase() + ', i think'])
             || ask(/\b(called|name|title|what.*(movie|film))\b/, c.title, [u + ' it is ' + c.title + ' :smile:', u + ' ' + c.title + '!'])
             || ask(/\b(studio|made by|company)\b/, c.studios.length, [u + ' ' + c.studios[0] + ' i think']);
        return a || pick(r, unsure);
    }
    if (LAUGH.test(text)) return pick(r, [u + ' right :laugh:', 'lol ' + u, u + ' :laugh: :laugh:']);
    if (LOVE.test(text)) {
        var g = [u + ' right?? you get it', 'agreed ' + u + ' :heart:', u + ' same, this is so good', 'so true ' + u];
        if (c.title) g.push(u + ' ' + c.title + ' is a good one :clap:');
        return pick(r, g);
    }
    var generic = [u + ' same', 'true ' + u, u + ' ha', 'yeah ' + u + ' :smile:', u + ' fair point', u + ' welcome to the watch party', u + ' :clap:'];
    if (c.title && r() < 0.4) generic.push(u + ' ' + c.title + ' fan club :heart:');
    if (c.genres.length && r() < 0.3) generic.push(u + ' ' + pick(r, c.genres).toLowerCase() + ' fans unite');
    return pick(r, generic);
}

// One or two replies to something the user typed. `nonce` is a counter, so the same sentence typed twice
// can still get different answers. Each reply says how long to wait before it appears.
function replies(seed, user, text, nonce, c) {
    var r = rng(seed, 'reply', hash32(text), nonce);
    var n = r() < 0.4 ? 2 : 1, out = [], used = {}, delay = 1200 + Math.floor(r() * 2200);
    for (var i = 0; i < n; i++) {
        var v = viewer(seed, Math.floor(r() * CROWD));
        if (used[v.id]) continue;
        used[v.id] = true;
        out.push({ delayMs: delay, name: v.name, color: v.color, badge: v.badge, parts: parts(replyText(r, user, text, c)) });
        delay += 1500 + Math.floor(r() * 2800);
    }
    return out;
}

// Only what is safe to show as rich text.
function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }
