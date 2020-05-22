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

### Building via hbgw_middleware

BirdMon is part of the HBuilder Middleware group, so the easiest way to build it is via the `hbgw_middleware` repository.  

1. Install external dependencies.
2. Clone/Download hbgw_middleware repository, and `cd` into it.
3. Do the normal: `make all; sudo make install` 
4. Everything will be installed into a `/opt/` directory tree.  Make sure your `$PATH` has `/opt/bin` in it.

### Building without hbgw_middleware

If you want to build BirdMon outside of the hbgw_middleware repository framework, you'll need to clone/download the following HBuilder repositories.  You should have all these repo directories stored flat inside a root directory.

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