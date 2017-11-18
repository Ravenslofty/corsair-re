Attacking the Corsair Fleet
===========================

I thought I'd put this one on GitHub due to my own personal website being a) down and 
b) not able to handle the influx that this could generate.

I've been affiliated with the ckb-next project (if you could call it a project - it's
basically myself, @tatokis and @light2yellow at the moment) for about six months now,
and so far the project has been essentially on maintenance mode of the original ckb by
@ccMSC. The team inherited a barely commented codebase filled with a lot of magic
numbers and little explanation and tried their best to document it. It was hard work.

Not that Corsair have been much help. With no need to worry about backwards
compatibility, they have broken the firmware protocol with every major revision, and
some devices, such as the original M95 use an entirely different protocol at all. With
version 3 around the corner (we have firmware files for it), another protocol change
is incoming, and we needed help.

So, I took it upon myself to reverse engineer the version 2 protocol. While Corsair's
changes have been breaking, they've not been a complete revamp; the rough outlines are
the same. Having a guide to V2 would be a benefit to V3 as well.

Prior knowledge
---------------

The team didn't have much prior knowledge of how the protocol worked, other than that
learned through reading the ckb source code and through CUE wireshark dumps. But we
did know that `0e 01` was the firmware initialisation packet, so that was a [start].

`0e` - The "easy" command
-------------------------

`0e` commands are read commands; the device will respond to them when you poll for an
answer if they are valid, and that makes checking for valid `0e XX` commands easy
enough. You just walk through all values of XX and see what the device responds to.

From this, I got `0e 00`, `0e 01`, `0e 0e`, `0e 11`, `0e 13`, `0e 15`, `0e 16`,
`0e 20` and `0e 40`. At first glance, `0e 00`, `0e 01` and `0e 0e` all seemed to do
similar, if not the same thing. (We would later learn that because the device reuses
the same 64-byte buffer, we were seeing the remnants of the reply to the `0e 01`
packet.) I therefore decided to throw random numbers at the rest of the packet and
found that the device echoes the first four bytes of the command. From this we
assumed the first four bytes of the command were the most important ones; we had to
draw the line *somewhere*.

I asked @tatokis to run through the commands on his K70 RGB, and pruned `0e 11` as
something seemingly specific to my device. `0e 13` was dropped from his keyboard too, but ckb references it in mouse-specific code, so I kept that and instead decided to
explore it.`

Through poking the 3rd and 4th bytes, I found that the Corsair protocol will reply to
an invalid byte 3/4 by echoing it, but otherwise not changing the reponse payload.
Running through all permutations of `0e 13 XX`, I uncovered a [fair chunk] of 
commands, some of which were in the ckb codebase (and thus could be documented from
that), and others which were not. Of particular note is `0e 13 11`, which I discovered
returned the RGB value of the logo light on my mouse. 

Next on my list was `0e 15`, which the ckb code always sent alongside `0e 16`. Walking
through the `0e 15 XX` bytes showed that only `0e 15 01` was a valid command, and
likewise for `0e 16 01`. I need to investigate those more.

Armed with the knowledge that I had learned from `0e 13` and friends, I went back
through the others - `0e 00`, `0e 20` and `0e 40` did not reply with anything, meaning
it was possibly a bug to be accepted, or that it toggled some internal state. `0e 01`
was scanned and found to respond the same regardless of what the 3rd and 4th bytes
were. `0e 0e` was found to reply with a single zero byte (I don't know what it means
either).

With `0e` cleared, I turned to one of the other packets I could find in the ckb code.

`07` - Unlucky seven
--------------------

`07` commands are write commands; they change internal state, and don't reply. The
lack of a reply makes it difficult to distinguish a valid packet from an invalid
packet, so I just naively scanned through all the `07 XX` commands.

And my mouse promptly reset.

Stepping through the commands manually, I found that `07 02` could reliably make my
mouse reset itself. By sending the command twice and checking if the second one fails
due to the device not existing, I could check if the command resets the device.

Cycling through the third byte, I found `07 02 00` was a reset, `07 02 01` reset
faster than that, and most of the packets were ignored until `07 02 aa`, which made my
mouse unresponsive (I initially nicknamed it the rat poison packet, until @tatokis
pointed to `dmesg` output showing the mouse had mounted itself as a USB storage
device, indicating we were in bootloader/firmware update mode). That sweep put me to
[here], which is more or less where I am at at the moment.

I want to help!
---------------

Join #ckb-next on Freenode - we'd all appreciate the company.

[start]: https://github.com/mattanger/ckb-next/wiki/Corsair-Protocol/35e69186
[fair chunk]: https://github.com/mattanger/ckb-next/wiki/Corsair-Protocol/_compare/35e69186...4d984380
[here]: https://github.com/mattanger/ckb-next/wiki/Corsair-Protocol/_compare/814bf8c48...59de68fb 
