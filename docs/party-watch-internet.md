# Party Watch (beta): connecting from outside your home

Hop-tunes sharing works on your home network as soon as you switch it on. To let a guest connect from **outside**
your home, three things have to be true:

1. Hop-tunes accepts outside connections (Settings › sharing › *who can connect* › *also from outside*).
2. Your **router** passes one port to this computer (port forwarding).
3. This computer's **firewall** lets that port in.

There are no accounts, no relay and no third-party service: your guest connects straight to your home, encrypted with
their own private key. That is also why the router and the firewall are your job. The steps below cover the usual cases.

> Sharing is **off every time you start Hop-tunes**, and "also from outside" is off every time you switch sharing on.
> Switch them off when you are not using them.

## 0. Find the port and this computer's address

Open Settings › sharing and switch sharing on. It shows, for example:

> you are sharing. on this network your address is **192.168.1.20**, port **47800**.

Hop-tunes tries port **47800** first; if something else uses it, it picks another free one and shows that. Use the
number it shows. Everything below uses 47800 as the example.

## 1. Give this computer a fixed address on your home network

Routers hand out addresses that can change. Tell yours to always give this computer the same one (the setting is
called *DHCP reservation*, *static lease* or *address reservation*, usually under LAN or DHCP). Pick this computer by
its name or its hardware (MAC) address and keep the address Hop-tunes shows (for example 192.168.1.20).

## 2. Forward the port on your router

Open your router's settings page in a browser (often `192.168.1.1` or `192.168.0.1`; the address is on a sticker on
the router). Find **Port forwarding** (also called *virtual server*, *NAT* or *applications and gaming*) and add a rule:

| Setting | Value |
|---|---|
| Name | Hop-tunes sharing |
| Protocol | **TCP** |
| Outside / external port | 47800 |
| Inside / internal port | 47800 |
| Send to (internal address) | this computer, e.g. 192.168.1.20 |

Save, and restart the router if it asks.

## 3. Let the port through this computer's firewall

**Linux, ufw** (Ubuntu, Mint and others):

```
sudo ufw allow 47800/tcp
```

To undo it later: `sudo ufw delete allow 47800/tcp`.

**Linux, firewalld** (Fedora, openSUSE, and others):

```
sudo firewall-cmd --permanent --add-port=47800/tcp
sudo firewall-cmd --reload
```

Undo: `sudo firewall-cmd --permanent --remove-port=47800/tcp` then `--reload`.

**Windows:** when Hop-tunes first starts sharing, Windows asks whether to allow it; tick **Private** and **Public**.
Or add the rule yourself, in an administrator PowerShell:

```
New-NetFirewallRule -DisplayName "Hop-tunes sharing" -Direction Inbound -Protocol TCP -LocalPort 47800 -Action Allow
```

If your Linux has no firewall turned on, there is nothing to do here. The Flatpak version runs in a sandbox that does
not allow network access, so sharing cannot start there yet; use the AppImage or a regular package.

## 4. Find your public address

Your router's status page shows the address your internet provider gave you (often labelled **WAN** or **Internet**).
That is your public address.

**If it starts with** `100.64` to `100.127`, or with `10.`, `172.16` to `172.31` or `192.168`, your provider shares one
public address between many customers (called carrier-grade NAT) and **port forwarding cannot work**. Ask the provider
for a public address (some do it for free on request), or use IPv6 if you have it: with IPv6 there is often no router
in the way and only the firewall step is needed.

## 5. Put it in Hop-tunes and send the invite

In Settings › sharing, under *also from outside*, type your public address (an address like `203.0.113.5`, or a name if
you have one) into **your public address**. Make an invite: the link now carries that address, for example
`hop://203.0.113.5:47800/ABCD-EFGH-JKMN-PQRS`. Send it to your guest yourself.

## 6. Test it from outside

Ask the guest to join. Many routers will not let you reach your own public address from inside your home, so a test
from your own computer often fails even when everything is right.

## When your address changes

Home internet addresses change now and then. If a guest cannot connect any more, send them your new address. On their
side the shared tab shows "can't reach …" with a **change address** button: they type the new `address:port` and carry on.
If your provider changes yours often, a dynamic-DNS name from a service you choose works in place of the number: type
that name instead.

## If it does not work

* **"could not connect"** on the guest's side: the port is not reaching you. Recheck step 2 (TCP, the right port, the
  right inside address) and step 3 (firewall), and that sharing and *also from outside* are on.
* It works at home but not from outside: the router rule, or carrier-grade NAT (step 4).
* It worked yesterday: your public address probably changed; look it up again.
* **"no longer lets you in"**: you removed that guest. Make a new invite.

## What an outside connection can and cannot do

Only people who hold a guest's private key get past the first handshake, a wrong key is refused before any request is
read, an invite is single-use and is cancelled after five wrong tries, and a guest can only stream what you ticked for
them. They cannot download it, list your files or see your profile. You can remove a guest at any moment.
