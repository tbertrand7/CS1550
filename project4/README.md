# FUSE: Filesystem in Userspace

## Fast Build Script (build.sh)
A script for building and testing FUSE only on thoth. For more information see project details at [here](http://people.cs.pitt.edu/~jmisurda/teaching/cs1550/2174/cs1550-2174-project4.htm).

Login to thoth
 * ssh username@thoth.cs.pitt.edu

Clone the repository and navigate to project 4 directory
 * git clone https://github.com/hmofrad/CS1550 /u/OSLab/username/CS1550
 * cd /u/OSLab/username/CS1550/project4
 
Give execute permission to _build.sh_ script
 * chmod +x build.sh

Run _build.sh_ script to build FUSE
 * ./build.sh
  
Run _build.sh_ script to test hello world example or cs1550 code skeleton
 * ./build.sh &lt;hello|cs1550&gt;
