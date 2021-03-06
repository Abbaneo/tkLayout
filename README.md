# Prerequisites

tkLayout requires:

    ROOT version > 5.34.09 (root and root-config should be in user's path)
    BOOST version > 1.55
    gcc version > 4.7

If you are on lxplus, you can just run a bash shell and then:

     source setup_slc6.sh
     
     
# Getting the code

     git clone https://github.com/tkLayout/tkLayout.git
     cd tkLayout

The latest official version is at tkLayout/master branch, the latest devolpment version at tkLayout/dev branch.

# Compilation

Compilation using make:

    make

This will build the needed programs and put them in the ./bin directory.

Compilation using cmake (NEW):

    mkdir build     (all object files, help files, libs, ... will be kept here)
    cd build
    cmake ..        (generate makefile)
    make install    (or write make all + make install)
    make doc        (generate Doxygen-based documentation in doc directory)

    make uninstall  (if cleaning needed)
    rm *            (clean all content in build directory & restart if needed)

This will build the needed programs, copy the executables into tkLayout/bin directory and create symlink in ${HOME}/bin directory.


# Install
  If the make command runs properly you can install the program with the script

     make install

### First-time install
If this is the first time that you install tkLayout, a few questions will be asked. You will need to provide:

1. The directory where you want to store the tkLayout executable.
2. The destination directory for the web output of the program (for example: /afs/cern.ch/user/y/yourname/www/layouts). This directory needs to be writable by you and readable by the web server.    
  The style files will be copied during the installation to that directory. If the style files get changed during the development, a new ./install.sh will be needed to get this propagated to the output.   
  Alternatively, you can choose to make a symbolic link between the tkLayout/style directory and
  the www/layouts directory. This avoids the need of repeating the ./install.sh at every update of the style files
  (but in order to do this, the tkLayout/style directory should also be within the reach of the web server).
3. The path to your tkLayout source directory (for example: /afs/cern.ch/user/y/yourname/tkLayout).
4. The set of momentum values to be used for the simulation.

tkLayout stores the installation configuration in the file "~/.tkgeometryrc". 
So in case wrong data is entered, you can just directly modify information there, and rerun "make install".


# Command line options

Once tklayout is built and installed, you can of course see the command line options with:

     tklayout --help


# Have fun!
