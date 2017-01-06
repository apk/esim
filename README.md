# esim
This is retro-ware, a railway simulator I wrote a long time ago,
and then gradually added more tracks and trains to.
It runs theoretically unattended, and on an X display, but
it will run into some deadlock situations of varied ease
of being fixable.

It used a proprietary comm lib whose use has been ripped out,
and a similarly proprietary build system which is the reason
nothing but the newest commits will build at all.

## build

To build, execute the `compile-command` in `src/esim.c`
inside the `src` dir, after cloning https://github.com/apk/jfdt
besides this project, and building that first.

## Run

`cd ../layout && ../src/esim -a v5.in` runs the monstrous layout.
Use the space key on a train's box to select it and have the
window auto-scroll to follow it.
