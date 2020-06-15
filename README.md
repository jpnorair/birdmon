# BirdMon (birdmon)

"birdmon" is short for "Bird Monitor." It is a POSIX-C daemon that monitors the "birds" -- i.e. the GNSS satellite constellations -- and provides a request-response protocol for clients looking to get some information about them.

The information it provides (at present version):

* List of satellites in visible sky
* Generic Assisted GNSS information
* Assisted GNSS information block for UBlox receivers

## Building BirdMon

### External Dependencies

There are some external dependencies for BirdMon.  These are relatively common libraries, and they are usually available via your favorite package manager.

* libcurl
* libtalloc (2.1.4 - 2.1.17 are tested)

### Building via HB Distribution

BirdMon is part of the [HB Distribution](https://github.com/jpnorair/hbdist).  The easiest way to build it is via that distribution, which will include all the dependencies which are not readily available in mainstream package managers.

1. Install external dependencies.
2. Clone/Download [hbdist](https://github.com/jpnorair/hbdist) repository, and `cd` into it.
3. Do the normal: `make all; sudo make install` 
4. Everything will be installed into a `/opt/` directory tree.  Make sure your `$PATH` has `/opt/bin` in it.

### Building without hbdist

If you want to build BirdMon outside of the hbdist repository framework, you'll need to clone/download the following repositories.  You should have all these repo directories stored flat inside a root directory.

* _hbsys
* argtable
* **birdmon**
* bintex
* cJSON
* clithread
* cmdtab
* hbutils
* otvar

From this point:

```
$ cd birdmon
$ make pkg
```

You can find the binary inside `birdmon/bin/.../birdmon`

## Using Birdmon (Quickstart)

A full description of features and usage is available in the USERGUIDE.md document.  This section provides a synopsis, particularly suited to starting `birdmon` in a terminal and running some commands in interactive mode.

TODO

## Version History

### 0.1.0 : 30 August 2019

Blah Blah Blah
