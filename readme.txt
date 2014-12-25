Rjb is Ruby-Java bridge using Java Native Interface.

http://www.slideshare.net/artonx/j-ruby-kaigi-2010

How to install

you need to install Java2 sdk, and setup JAVA_HOME enviromental varible except for OS X.
I assume that OS X's JAVA_HOME is reported by calling /usr/libexec/java_home.

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

-- Notice for opening non-ASCII 7bit filename
If you'll plan to open the non-ascii character named file by Java class through Rjb, it may require to set LC_ALL environment variable in you sciprt.
For example in Rails, set above line in production.rb as your environment.
ENV['LC_ALL'] = 'en_us.utf8' # or ja_JP.utf8 etc.
 cf: http://bugs.sun.com/view_bug.do?bug_id=4733494
   (Thanks Paul for this information).

artonx@yahoo.co.jp
