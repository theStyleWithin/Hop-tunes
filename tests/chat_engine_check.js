// Checks for src/ui/qml/ChatGen.js, evaluated by tests/test_ui.cpp (chatEngine) after ChatGen.js itself.
// It must evaluate to an object of results.
(function () {
  var out = {};
  var info = { meta: { title: 'The Waterboy', year: '1998', genre: 'Comedy', director: 'Frank Coraci',
                       studio: 'Touchstone', tagline: 'Mess with him, not his water.' },
               cast: [{ name: 'Adam Sandler', role: 'Bobby' }] };
  var c = context(info, 5400000), empty = context({}, 0);
  function txt(m) { return m.parts.map(function (p) { return p.v; }).join(' '); }

  // A pure function of (seed, second): the same twice, different for another video.
  out.same = JSON.stringify(messagesAt('a', 61, c)) === JSON.stringify(messagesAt('a', 61, c));
  out.differs = JSON.stringify(messagesAt('a', 61, c)) !== JSON.stringify(messagesAt('b', 61, c));

  // It talks, in time order within each second, and uses the movie page now and then.
  var total = 0, movie = 0, ordered = true, bad = 0;
  for (var s = 0; s < 300; s++) {
    var list = messagesAt('a', s, c), last = -1;
    for (var i = 0; i < list.length; i++) {
      total++;
      var t = txt(list[i]);
      if (/Waterboy|1998|Comedy|Coraci|Touchstone|Sandler|Bobby/.test(t)) movie++;
      if (list[i].at < last || list[i].at < s * 1000 || list[i].at >= (s + 1) * 1000) ordered = false;
      last = list[i].at;
      if (t.trim() === '') bad++;
    }
  }
  out.total = total; out.movie = movie; out.ordered = ordered; out.bad = bad;

  // An empty movie page never leads to invented details.
  var invented = 0;
  for (var s2 = 0; s2 < 300; s2++) {
    var l2 = messagesAt('a', s2, empty);
    for (var j = 0; j < l2.length; j++) if (/Waterboy|1998|Sandler|undefined|null/.test(txt(l2[j]))) invented++;
  }
  out.invented = invented;

  // Answers come from what the page says, and only to the thing that was asked.
  function ask(q, ctx) { return replies('a', 'me', q, 1, ctx).map(txt).join(' | '); }
  out.director = /Frank Coraci/.test(ask('who is the director?', c));
  out.year = /1998/.test(ask('what year is this?', c));
  out.noDirector = !/Coraci/.test(ask('who is the director?', empty)) && ask('who is the director?', empty).length > 0;
  out.mentionsUser = /@me/.test(ask('hello', c));
  out.delays = replies('a', 'me', 'lol', 1, c).every(function (r) { return r.delayMs >= 1000 && r.delayMs < 12000; });
  out.count = viewerCount('a', 0) >= 2;
  out.esc = esc('<b>&"') === '&lt;b&gt;&amp;&quot;';
  var ps = parts('hi :laugh: @bob ok');
  out.parts = ps.length === 5 && ps[1].t === 'emote' && ps[3].t === 'mention';
  return out;
})()
