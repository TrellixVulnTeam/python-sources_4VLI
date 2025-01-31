Building Python using VC++ 6.0 or 5.0
-------------------------------------
This directory is used to build Python for Win32 platforms, e.g. Windows
2000 and XP.  It requires Microsoft Visual C++ 6.x or 5.x and Platform
SDK February 2003 Edition (Core SDK). You can download this SDK from
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/psdk-full.htm.
(For other Windows platforms and compilers, see ../readme.txt.)

All you need to do is open the workspace "pcbuild.dsw" in MSVC++, select
the Debug or Release setting (using Build -> Set Active Configuration...),
and build the projects.

The proper order to build subprojects:

1) pythoncore (this builds the main Python DLL and library files,
               python30.{dll, lib} in Release mode)

2) python (this builds the main Python executable,
           python.exe in Release mode)

3) the other subprojects, as desired or needed (note:  you probably don't
   want to build most of the other subprojects, unless you're building an
   entire Python distribution from scratch, or specifically making changes
   to the subsystems they implement; see SUBPROJECTS below)

When using the Debug setting, the output files have a _d added to
their name:  python30_d.dll, python_d.exe, pyexpat_d.pyd, and so on.

SUBPROJECTS
-----------
These subprojects should build out of the box.  Subprojects other than the
main ones (pythoncore, python, pythonw) generally build a DLL (renamed to
.pyd) from a specific module so that users don't have to load the code
supporting that module unless they import the module.

pythoncore
    .dll and .lib
python
    .exe
pythonw
    pythonw.exe, a variant of python.exe that doesn't pop up a DOS box
_msi
    _msi.c. You need to install Windows Installer SDK to build this module.
_socket
    socketmodule.c
_testcapi
    tests of the Python C API, run via Lib/test/test_capi.py, and
    implemented by module Modules/_testcapimodule.c
pyexpat
    Python wrapper for accelerated XML parsing, which incorporates stable
    code from the Expat project:  http://sourceforge.net/projects/expat/
select
    selectmodule.c
unicodedata
    large tables of Unicode data
winsound
    play sounds (typically .wav files) under Windows

The following subprojects will generally NOT build out of the box.  They
wrap code Python doesn't control, and you'll need to download the base
packages first and unpack them into siblings of PCbuilds's parent
directory; for example, if your PCbuild is  .......\dist\src\PCbuild\,
unpack into new subdirectories of dist\.

_tkinter
    Python wrapper for the Tk windowing system.  Requires building
    Tcl/Tk first.  Following are instructions for Tcl/Tk 8.4.12.

    Get source
    ----------
    In the dist directory, run
    svn export http://svn.python.org/projects/external/tcl8.4.12
    svn export http://svn.python.org/projects/external/tk8.4.12
    svn export http://svn.python.org/projects/external/tix-8.4.0

    Build Tcl first (done here w/ MSVC 6 on Win2K)
    ---------------
    cd dist\tcl8.4.12\win
    run vcvars32.bat
    nmake -f makefile.vc
    nmake -f makefile.vc INSTALLDIR=..\..\tcltk install

    XXX Should we compile with OPTS=threads?

    Optional:  run tests, via
        nmake -f makefile.vc test

        all.tcl:        Total   10835   Passed  10096   Skipped 732     Failed  7
        Sourced 129 Test Files.
        Files with failing tests: exec.test expr.test io.test main.test string.test stri
        ngObj.test

    Build Tk
    --------
    cd dist\tk8.4.12\win
    nmake -f makefile.vc TCLDIR=..\..\tcl8.4.12
    nmake -f makefile.vc TCLDIR=..\..\tcl8.4.12 INSTALLDIR=..\..\tcltk install

    XXX Should we compile with OPTS=threads?

    XXX I have no idea whether "nmake -f makefile.vc test" passed or
    XXX failed.  It popped up tons of little windows, and did lots of
    XXX stuff, and nothing blew up.

   Built Tix
   ---------
   cd dist\tix-8.4.0\win
   nmake -f python.mak
   nmake -f python.mak install

bz2
    Python wrapper for the libbz2 compression library.  Homepage
        http://sources.redhat.com/bzip2/
    Download the source from the python.org copy into the dist
    directory:

    svn export http://svn.python.org/projects/external/bzip2-1.0.3

    And requires building bz2 first.

    cd dist\bzip2-1.0.3
    nmake -f makefile.msc

    All of this managed to build bzip2-1.0.3\libbz2.lib, which the Python
    project links in.


_sqlite3
    Python wrapper for SQLite library.
    
    Get the source code through
    
    svn export http://svn.python.org/projects/external/sqlite-source-3.3.4
    
    To use the extension module in a Python build tree, copy sqlite3.dll into
    the PC/VC6 folder.


_ssl
    Python wrapper for the secure sockets library.

    Get the latest source code for OpenSSL from
        http://www.openssl.org

    You (probably) don't want the "engine" code.  For example, get
        openssl-0.9.6g.tar.gz
    not
        openssl-engine-0.9.6g.tar.gz

    Unpack into the "dist" directory, retaining the folder name from
    the archive - for example, the latest stable OpenSSL will install as
        dist/openssl-0.9.6g

    You can (theoretically) use any version of OpenSSL you like - the
    build process will automatically select the latest version.

    You must also install ActivePerl from
        http://www.activestate.com/Products/ActivePerl/
    as this is used by the OpenSSL build process.  Complain to them <wink>.

    The MSVC project simply invokes PC/VC6/build_ssl.py to perform
    the build.  This Python script locates and builds your OpenSSL
    installation, then invokes a simple makefile to build the final .pyd.

    build_ssl.py attempts to catch the most common errors (such as not
    being able to find OpenSSL sources, or not being able to find a Perl
    that works with OpenSSL) and give a reasonable error message.
    If you have a problem that doesn't seem to be handled correctly
    (eg, you know you have ActivePerl but we can't find it), please take
    a peek at build_ssl.py and suggest patches.  Note that build_ssl.py
    should be able to be run directly from the command-line.

    build_ssl.py/MSVC isn't clever enough to clean OpenSSL - you must do
    this by hand.


YOUR OWN EXTENSION DLLs
-----------------------
If you want to create your own extension module DLL, there's an example
with easy-to-follow instructions in ../PC/example/; read the file
readme.txt there first.
