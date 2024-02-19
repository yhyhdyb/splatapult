
Splatapult
----------------------------------------------
![Splatapult logo](/splatapult.png)

[<img src="https://i.ytimg.com/vi/18DuNJRZbzQ/maxresdefault.jpg" width="50%">](https://www.youtube.com/watch?v=18DuNJRZbzQ "Splatapult Demo")

A program to display 3d gaussian splats files
support multiple files
splatapult [OPTIONS] ply_filename1 ply2 .....

Options
-------------
-v, --openxr
    launch app in vr mode, using openxr runtime

-f, --fullscreen
    launch window in fullscreen

-d, --debug
    enable debug logging

-h, --help
    show help

Desktop Controls
--------------------
* wasd - move
* q, e - roll
* t, g - up, down
* arrow keys - look
* right mouse button - hold down for mouse look.
* gamepad - if present, right stick to rotate, left stick to move, bumpers to roll
* c - toggle between initial SfM point cloud (if present) and gaussian splats.
* n - jump to next camera
* p - jump to previous camera

VR Controls
---------------
* c - toggle between initial SfM point cloud (if present) and gaussian splats.
* left stick - move
* right stick - snap turn
* f - show hide floor carpet.
* single grab - translate the world.
* double grab - rotate and translate the world.
* triple grab - (double grab while trigger is depressed) scale, rotate and translate the world.
* return - save the current position and orientation/scale of the world into a vr.json file.

Config Files
----------------------
If a "_vr.json" file is found, it will be used to determine the proper starting position, scale and orienation for vr mode.
You can create your own _vr.json file by manipulating the scene via grab in vr mode, then press return to save.

Splatapult supports the same dir structure that [Gaussian Splatting](https://github.com/graphdeco-inria/gaussian-splatting) code will output.
Which is as follows:

```
dir/
    point_cloud/
        iteration_30000/
            point_cloud.py
            point_cloud_vr.json
    input.ply
    cameras.json
```

input.ply contains the point cloud and cameras.json will contain the camera orientations from the SfM stage.

If the "cameras.json" file is found in the same dir as the ply_filename or it's parent dirs, it will be loaded.
The 'n' and 'p' keys can then be used to cycle thru the camera viewpoints.

It will also support files downloaded from lumalabs.ai, but in this case there will be no point clouds or cameras.

```
dir/
    mycapture.ply
    mycapture_vr.json
```


