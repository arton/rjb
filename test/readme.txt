how to run the test

you should set lD_LIBRARY_PATH environment variable to point the JVM.

for example)

If you use Linux Sun Java2 Standard Edition,

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$JAVA_HOME/jre/lib/i386:$JAVA_HOME/jre/lib/i386/client
ruby test.rb

