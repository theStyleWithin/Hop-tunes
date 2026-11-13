# Signing in to Kick (optional)

Hop-tunes never needs a Kick account: browsing, searching, watching, downloading and recording all work without one. The sign-in is optional,
uses **Kick's official OAuth sign-in** and, for now, only shows who is signed in (scope `user:read`). It cannot import your follows: Kick's
official API has no follow list (use **add channels** on the streams page for that).

Kick's sign-in needs an app ID **and a client secret**, so nothing is built into Hop-tunes: each person makes their own free Kick app.

## Set it up (once)

1. On kick.com turn on two-factor authentication, then open **Account settings > Developer** and create an app
   (see Kick's [App Setup](https://docs.kick.com/getting-started/kick-apps-setup)).
2. As the app's **redirect URL** give exactly the address Hop-tunes shows on its account page, normally
   `http://localhost:38573/callback` (the copy button is next to it).
3. Copy the app's **client ID** and **client secret** into the account page (streams page > **sign in** or **account**) and press **save**.
4. Press **sign in with Kick**. Your browser opens kick.com's own sign-in page; approve it there and go back to Hop-tunes.

## What happens, and what is kept

- Your browser goes to `id.kick.com` (Kick). Kick sends it back to `http://localhost:38573/callback`, where Hop-tunes listens on this
  computer only (127.0.0.1), for at most five minutes, takes the code once, checks the `state` it made up, and closes. Hop-tunes never sees your password.
- The code is exchanged for tokens with PKCE (S256). The tokens, the client ID and the client secret are kept **in your profile on this
  device, as plain text** (a guest keeps them until the session ends). The secret only ever goes to `id.kick.com`, the access token only to
  `api.kick.com/public/v1/users`.
- Hop-tunes contacts `id.kick.com` and `api.kick.com` only while you sign in, sign out, or when the app opens and you are signed in
  (to refresh the token and read your name). The network gateway lets exactly three requests carry a body or a token: the token request,
  the revoke request and the user request; anything else is refused.
- **sign out** asks Kick to revoke both tokens and forgets them here (the app's ID and secret stay for next time).

## Good to know

- If the sign-in port (38573) is in use by something else, Hop-tunes says so; close that program and try again.
- On Android the same address is used while Hop-tunes waits in the background for the browser; this has not been tried on a phone yet.
- Kick's API and rules can change; see Kick's [developer documentation](https://docs.kick.com/) and its developer terms.
