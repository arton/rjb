Rjb is Ruby-Java bridge using Java Native Interface.

How to install

you need to install Java2 sdk, and setup JAVA_HOME enviromental varible except for OS X.
I assume that OS X's JAVA_HOME is fixed as '/System/Library/Frameworks/JavaVM.framework'.

then,

ruby setup.rb config
ruby setup.rb setup

(in Unix)
sudo ruby setup.rb install
or
(in win32)
ruby setup.rb install

How to test
in Win32
cd test
ruby test.rb

in Unix
see test/readme.unix 
you must set LD_LIBRARY_PATH environmental variable to run rjb.

artonx@yahoo.co.jp
