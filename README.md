LEDBAT support for WebRTC
================

This is a version of the [WebRTC Native Code Package](http://www.webrtc.org/webrtc-native-code-package) with the added funcitonality of [LEDBAT](http://tools.ietf.org/html/rfc6817) as an alternative transport protocol for DataChannels.

This has only been compiled and tested on the linux platform!

## Download, compile and run (linux only)
This repository is a copy of revision 5844 of the WebRTC Native Code Package from http://webrtc.googlecode.com/svn/trunk with added LEDBAT functionality. To compile and run all prerequisites and install steps of the original WebRTC Native Code Package are needed. To get all the original WebRTC code and run the install hooks the project is downloaded through gclient as usual, but at a specific revision, and then changes from this git repo are applied.

### Step 1: Install prerequisites
Follow the guide for installing all prerequisites at: http://www.webrtc.org/reference/getting-started/prerequisite-sw

### Step 2: Download all the sources from svn
```
# Enter a folder where you want your project to live and:
$ gclient config http://webrtc.googlecode.com/svn/trunk
$ gclient sync --force --revision 5844
```

### Step 3: Download an apply changes from this git repository
```
$ cd trunk/

# Create a repository, link it to github and overwrite local files with files from git repo
$ git init
$ git remote add --track master origin https://github.com/Peerialism/browser-tranport.git
$ git fetch
$ git pull # Gives an error, but that's ok
$ git reset --hard

# Run gclient hooks to generate correct build scripts (hiding git from gclient)
$ mv .git .git.bak  # gclient tries to use our git instead of svn when running hooks, hide it!
$ gclient runhooks
$ mv .git.bak .git  # restore our git repo again
```

### Step 4: Compile
```
# The documentation on prerequisites on the WebRTC site is outdated.
# You will need to install a million packages that are not specified 
# in the install instructions that causes the compilation to fail.
$ ninja -C out/Debug 
```

### Step 5: Unit test (optional)
```
$ out/Debug/libjingle_media_unittest --gtest_filter="LedbatDataEngineTest.*:UTPTest.*"
```

### Step 6: Experiment
```
# Enter the folder where scripts for experimentation and measurments live
$ cd measure/
$ mkdir d/  # Create a folder for garbage testing data
$ cat /dev/urandom > d/garbage  # Create a garbage file for testing. Ctrl+C when big enough

# To send a file we need a server, a sender and a receiver. 
# Everyone blocks the terminal, so we need to start each script in a separate one (or detatch)
$ ./server

# In new terminal:
$ ./recv

# In yet another terminal:
$ ./send  # By default sends the previously created file d/garbage

# When the sending is complete, check that it worked
$ md5sum d/garbage out
```

### Hack away!
