.pragma library
.import "ChatGen.js" as C

// Pretend comments under a video. Like the live chat: nobody is real, every comment is a pure
// function of (seed, index, the movie page) and is never stored.

var OPEN = ['honestly', 'okay so', 'not gonna lie,', 'rewatched this last night and', 'i keep coming back to this one,', 'my family put this on and', 'unpopular opinion:', 'watching this again and', 'just finished and', 'no one talks about it but'];
var TAKE = ['the pacing is perfect', 'the soundtrack carries so many scenes', 'i laughed way more than i expected', 'the ending still gets me', 'it holds up better than i remembered', 'the middle drags a little but it is worth it', 'every scene feels like it belongs', 'the whole thing is just comfortable to watch', 'the last ten minutes are the best part', 'the opening is a great hook'];
var PLAIN = ['This never gets old.', 'Came here after my third rewatch.', 'Who else is watching this in the dark with snacks?', 'Needed this today. Thank you for sharing.', 'My favorite thing to put on when the week has been long.', 'Still quoting this with friends.', 'The quality on this copy is great.', 'Anyone else notice something new every time?', 'This is the one I show people when they ask what to watch.', 'Simple, warm, and fun. That is all I wanted.'];
var REPLY = ['Same here!', 'Agreed, completely.', 'This. Exactly this.', 'Ha, I thought I was the only one.', 'Good point, never noticed that.', 'Honestly yes.', 'Fair enough, I see it differently but I get it.', 'Thank you for saying it.', 'Underrated take.', 'I need to rewatch it now.'];

function context(info, durationMs) { return C.context(info, durationMs); }

function ago(days) {
    if (days < 1) return 'today';
    if (days < 2) return '1 day ago';
    if (days < 7) return Math.floor(days) + ' days ago';
    if (days < 30) { var w = Math.floor(days / 7); return w + (w === 1 ? ' week ago' : ' weeks ago'); }
    if (days < 365) { var m = Math.floor(days / 30); return m + (m === 1 ? ' month ago' : ' months ago'); }
    var y = Math.floor(days / 365); return y + (y === 1 ? ' year ago' : ' years ago');
}

function plain(text) { return String(text).replace(/\s*:[a-z]+:/g, '').trim(); }   // comments have no emote codes

function body(r, c) {
    var x = r();
    if (x < 0.4) { var m = plain(C.movieLine(r, c)); if (m) return m.charAt(0).toUpperCase() + m.slice(1) + (/[.!?"]$/.test(m) ? '' : '.'); }
    if (x < 0.7) return C.pick(r, OPEN) + ' ' + C.pick(r, TAKE) + '.';
    return C.pick(r, PLAIN);
}

// The i-th comment (0 is the first). `likes` fall off with i, `days` grow, so "top" and "newest" both read naturally.
function comment(seed, i, c) {
    var r = C.rng(seed, 'comment', i);
    var v = C.viewer(seed, 1000 + i);
    var days = i * (1 + r() * 4) + r() * 3;
    var likes = Math.floor(Math.pow(r(), 3) * 4000 / (1 + i * 0.15));
    var nr = r() < 0.55 ? 0 : 1 + Math.floor(r() * 3), replies = [];
    for (var k = 0; k < nr; k++) {
        var rv = C.viewer(seed, 5000 + i * 4 + k);
        var rr = C.rng(seed, 'creply', i, k);
        var txt = rr() < 0.5 ? C.pick(rr, REPLY) : plain(body(rr, c));
        replies.push({ id: 'g' + i + '.' + k, name: rv.name, color: rv.color, text: (rr() < 0.4 ? '@' + v.name + ' ' : '') + txt,
                       likes: Math.floor(Math.pow(rr(), 3) * 120), ago: ago(Math.max(0, days - rr() * days * 0.5)) });
    }
    return { id: 'g' + i, name: v.name, color: v.color, text: body(r, c), likes: likes, days: days, ago: ago(days), replies: replies };
}

function comments(seed, from, count, c) {
    var out = [];
    for (var i = from; i < from + count; i++) out.push(comment(seed, i, c));
    return out;
}

// What the pretend crowd answers to a comment the user wrote. Each is shown once it has been
// "sitting there" for `afterMs` — on a reopened video the older answers are simply already there.
function answers(seed, id, text, c) {
    var rs = C.replies(seed, 'you', text, id, c), out = [];
    for (var i = 0; i < rs.length; i++) {
        var parts = rs[i].parts, t = '';
        for (var k = 0; k < parts.length; k++) if (parts[k].t !== 'emote') t += parts[k].v;
        out.push({ afterMs: 20000 + i * 45000 + rs[i].delayMs * 10, name: rs[i].name, color: rs[i].color, text: plain(t).replace(/\s+/g, ' ') });
    }
    return out;
}
