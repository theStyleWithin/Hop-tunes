// Checks for src/ui/qml/CommentGen.js, evaluated by tests/test_ui.cpp (commentEngine) after ChatGen.js and CommentGen.js.
(function () {
  var out = {};
  var info = { meta: { title: 'The Waterboy', year: '1998', genre: 'Comedy', director: 'Frank Coraci' }, cast: [{ name: 'Adam Sandler', role: 'Bobby' }] };
  var c = C.context(info, 5400000), empty = C.context({}, 0);

  out.same = JSON.stringify(comments('a', 0, 12, c)) === JSON.stringify(comments('a', 0, 12, c));
  out.differs = JSON.stringify(comments('a', 0, 12, c)) !== JSON.stringify(comments('b', 0, 12, c));
  // endless: any later page exists, and asking for more never changes what was already shown
  out.endless = comments('a', 5000, 5, c).length === 5 && JSON.stringify(comments('a', 0, 20, c).slice(0, 10)) === JSON.stringify(comments('a', 0, 10, c));

  var list = comments('a', 0, 200, c), movie = 0, bad = 0, withReplies = 0, emote = 0;
  for (var i = 0; i < list.length; i++) {
    var m = list[i];
    if (!m.text || !m.name || m.likes < 0 || !m.ago) bad++;
    if (/:[a-z]+:/.test(m.text)) emote++;
    if (/Waterboy|1998|Comedy|Coraci|Sandler|Bobby/i.test(m.text)) movie++;
    if (m.replies.length) withReplies++;
    for (var k = 0; k < m.replies.length; k++) if (!m.replies[k].text || /:[a-z]+:/.test(m.replies[k].text)) bad++;
  }
  out.bad = bad; out.emote = emote; out.movie = movie; out.withReplies = withReplies;

  // an empty page never gets invented details
  var inv = 0, el = comments('a', 0, 200, empty);
  for (var j = 0; j < el.length; j++) if (/Waterboy|1998|Coraci|Sandler/.test(el[j].text)) inv++;
  out.invented = inv;

  // the crowd answers a user's comment, later, with plain text
  var ans = answers('a', 7, 'who is the director?', c);
  out.answers = ans.length >= 1 && ans.every(function (a) { return a.afterMs >= 20000 && a.text.length > 0 && !/:[a-z]+:/.test(a.text); });
  out.answerDirector = ans.some(function (a) { return /Coraci/.test(a.text); });
  out.ago = ago(0) === 'today' && ago(3) === '3 days ago' && ago(400) === '1 year ago';
  return out;
})()
