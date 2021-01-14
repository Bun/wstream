# wstream

Stream interactive video from the command-line.
Currently in an experimental state, so do not expect sensible code.

This project has the following goals:

* Stream dynamic content from a headless browser client using common protocols
  such as RTMP
* Application integration via a control API


Roadmap:

* Audio support
* Add controller API (change URL, etc)
* Flexible enough to compile-time support different rendering backends (CEF,
  Ultralight)
* I've never used cmake before, so improvements are expected


Potential future goals:

* Ability to swap out rendering backend on the fly while keeping the stream up;
  for example, by having a rendering subprocess that streams H.264 video and
  and RTMP forwarder that can stitch together H.264 streams.


## Building

To build using cmake:

    mkdir build && cd build
    cmake ..
    cmake --build . --config Release
