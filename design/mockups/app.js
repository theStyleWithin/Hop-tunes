// Hop-tunes mockups: shared data + tiny render helpers. Plain JS, no build step.
// All artists, albums and songs below are invented placeholders.
"use strict";

const HT = (() => {
  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  const art = (a, b, deg = 135) => `background: linear-gradient(${deg}deg, ${a} 50%, ${b} 50%)`;

  const icon = {
    prev: '<svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true"><path d="M18 5L8 12l10 7zM6 5v14" fill="none" stroke="#e9e9ec" stroke-width="2" stroke-linejoin="round"/></svg>',
    next: '<svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true"><path d="M6 5l10 7-10 7zM18 5v14" fill="none" stroke="#e9e9ec" stroke-width="2" stroke-linejoin="round"/></svg>',
    pause: '<svg width="16" height="16" viewBox="0 0 24 24" aria-hidden="true"><path d="M7 4v16M17 4v16" stroke="#e9e9ec" stroke-width="3"/></svg>',
    play: '<svg width="16" height="16" viewBox="0 0 24 24" aria-hidden="true"><path d="M7 4l14 8-14 8z" fill="#e9e9ec"/></svg>',
    playInk: '<svg width="12" height="12" viewBox="0 0 24 24" aria-hidden="true"><path d="M6 4l14 8-14 8z" fill="#111113"/></svg>',
    back: '<svg width="18" height="18" viewBox="0 0 24 24" aria-hidden="true"><path d="M15 5l-7 7 7 7" fill="none" stroke="#e9e9ec" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
    lock: '<svg width="16" height="16" viewBox="0 0 24 24" aria-hidden="true"><rect x="5" y="11" width="14" height="10" rx="2" fill="none" stroke="#e9e9ec" stroke-width="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3" fill="none" stroke="#e9e9ec" stroke-width="2"/></svg>',
    search: '<svg width="16" height="16" viewBox="0 0 24 24" aria-hidden="true"><circle cx="11" cy="11" r="7" fill="none" stroke="#a3a3ab" stroke-width="2"/><path d="M20 20l-4-4" stroke="#a3a3ab" stroke-width="2" stroke-linecap="round"/></svg>',
    logo: (c) => `<svg width="28" height="28" viewBox="0 0 28 28" aria-hidden="true"><circle cx="9" cy="19" r="5" fill="none" stroke="${c}" stroke-width="2.5"/><path d="M14 19V5l9 3v10" fill="none" stroke="${c}" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/><circle cx="20" cy="18" r="3" fill="${c}"/></svg>`,
  };

  const PALETTE = [["#2f6f73","#f2c94c"],["#3b2f5c","#ff8a4c"],["#7a3341","#e9d8c4"],["#1f3b57","#7cc4ff"],["#4a5a2a","#d9e3a5"],["#5a2f2f","#ffb38a"],["#2b2b3a","#b69cff"],["#6b4f2a","#f7e3b0"],["#203a33","#8fe0c9"],["#264653","#e76f51"],["#6b3e26","#f4a261"]];

  const LIBRARY = [
    { name: "Ada Lumen", albums: [["Glass Garden", "2019", "#284b63", "#d9ed92"]] },
    { name: "Copperline", albums: [["Rust Belt Hymns", "2016", "#6b3e26", "#f4a261"], ["Low Sun", "2021", "#3d2b1f", "#e9c46a"]] },
    { name: "Harbor Lights Collective", albums: [["Tidewater", "2022", "#2f6f73", "#f2c94c"], ["Fog Signals", "2018", "#1f3b57", "#a8dadc"], ["Pier 9", "2014", "#264653", "#e76f51"]] },
    { name: "Ines Valdera", albums: [["Copper Season", "2020", "#7a3341", "#e9d8c4"]] },
    { name: "Juno Okafor", albums: [["Paper Satellites", "2023", "#4a5a2a", "#d9e3a5"]] },
    { name: "Mateo Sand", albums: [["Ferro", "2017", "#6b4f2a", "#f7e3b0"]] },
    { name: "Rook & Wren", albums: [["Small Hours", "2021", "#2b2b3a", "#b69cff"]] },
    { name: "The Lanternists", albums: [["Night Market", "2019", "#5a2f2f", "#ffb38a"]] },
    { name: "Velvet Static", albums: [["Hum", "2015", "#203a33", "#8fe0c9"]] },
  ];
  const SONGS = [["Opening Tide","3:42","♥"],["Salt on the Window","4:17",""],["Lantern Keeper","3:05","♥"],["Breakwater","5:11",""],["Gulls at Noon","2:58",""],["Undertow","4:36","♥"],["Slack Water","3:49",""],["Last Ferry","6:02",""]];

  function header(active) {
    const tabs = [["collection","collection.html"],["now playing","now-playing.html"],["quickplay","quickplay.html"],["settings","quickplay.html"]];
    return `<header class="top">
      <nav class="pivots" aria-label="Main">${tabs.map(([t, h]) => `<a href="${h}"${t === active ? ' aria-current="page"' : ""}>${t}</a>`).join("")}</nav>
      <a class="me" href="index.html" aria-label="Switch profile (local profile: maya)"><span style="font-size:18px">maya</span><span class="av">m</span></a>
    </header>`;
  }

  function miniPlayer() {
    return `<footer class="mini">
      <div class="art" style="${art("#2f6f73", "#f2c94c")}" aria-hidden="true"></div>
      <div class="meta"><span style="font-size:17px">Tidewater</span><span>Harbor Lights Collective</span></div>
      <div class="ctrls">
        <button class="ib" type="button" aria-label="Previous">${icon.prev}</button>
        <button class="ib round" type="button" aria-label="Pause">${icon.pause}</button>
        <button class="ib" type="button" aria-label="Next">${icon.next}</button>
      </div>
      <div class="seek"><span>1:48</span><div class="bar"><i></i></div><span>4:17</span></div>
      <span style="font-size:13px;color:var(--muted)">gapless</span>
    </footer>`;
  }

  function mount(sel, html) { document.querySelector(sel).innerHTML = html; }

  return { esc, art, icon, PALETTE, LIBRARY, SONGS, header, miniPlayer, mount };
})();
