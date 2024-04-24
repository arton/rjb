#----------------------------------
# extconf.rb
# $Revision: $
# $Date: $
#----------------------------------
require 'mkmf'
require 'erb'

class Path

  def initialize()
    if File::ALT_SEPARATOR.nil?
      @file_separator = File::SEPARATOR
    else
      @file_separator = File::ALT_SEPARATOR
    end
  end

  def include(parent, child)
    inc = joint(parent, child)
    $INCFLAGS += " -I\"#{inc}\""
    $CFLAGS += " -I\"#{inc}\""
    inc
  end

  def joint(parent, child)
    parent + @file_separator + child
  end

end

javahome = ENV['JAVA_HOME']
if javahome.nil? && RUBY_PLATFORM =~ /darwin/
  javahome = `/usr/libexec/java_home`.strip
end
unless javahome.nil?
  if javahome[0] == ?" && javahome[-1] == ?"
    javahome = javahome[1..-2]
  end
  raise "JAVA_HOME is not directory." unless File.directory?(javahome)
  pt = Path.new
  inc = pt.include(javahome, 'include')
  if !File.exist?(inc) && RUBY_PLATFORM =~ /darwin/
    inc = pt.include('/System/Library/Frameworks/JavaVM.framework', 'Headers')
  end
  Dir.open(inc).each do |d|
    next if d[0] == ?.
    if File.directory?(pt.joint(inc, d))
      pt.include(inc, d)
      break
    end
  end
else
  raise "JAVA_HOME is not set."
end


def create_rjb_makefile
  if have_header("jni.h")
    have_func("locale_charset", "iconv.h")
    have_func("nl_langinfo", "langinfo.h")
    have_func("setlocale", "locale.h")
    have_func("getenv")
    $defs << "-DRJB_RUBY_VERSION_CODE="+RUBY_VERSION.gsub(/\./, '')
    create_header
    create_makefile("rjbcore")
  else
    raise "no jni.h in " + $INCFLAGS
  end
end

case RUBY_PLATFORM
when /mswin32/
  $CFLAGS += ' /W3'
when /cygwin/, /mingw/
  $defs << '-DNONAMELESSUNION'
end

JAVAH_COMMAND = 'javac -h . -classpath ../data/rjb RBridge.java'.freeze

if find_executable('javah')
  if defined?(Encoding) && ''.respond_to?(:force_encoding)
    cversion = (`javac -version`.force_encoding(Encoding.locale_charmap).encode('utf-8') =~ /\d+\.\d+\.\d+/ ) ? $& : nil
    hversion = (`javah -version`.force_encoding(Encoding.locale_charmap).encode('utf-8') =~ /\d+\.\d+\.\d+/ )  ? $& : nil
  else
    cversion = (`javac -version` =~ /\d+\.\d+\.\d+/ ) ? $& : nil
    hversion = (`javah -version` =~ /\d+\.\d+\.\d+/ )  ? $& : nil
  end
  if cversion == hversion || cversion.nil?
    javah = 'javah -classpath ../data/rjb jp.co.infoseek.hp.arton.rjb.RBridge'
  else
    $stderr.puts "warning: javac and javah version unmatch => javah: #{hversion}, javac: #{cversion}"
    javah = JAVAH_COMMAND
  end
else
  javah = JAVAH_COMMAND
end
File.open('depend', 'w') do |fout|
  fout.write ERB.new(IO::read('depend.erb')).result
end
create_rjb_makefile
