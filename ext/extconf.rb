#----------------------------------
# extconf.rb
# $Revision: $
# $Date: $
#----------------------------------
require 'mkmf'

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
    $INCFLAGS += " -I#{inc}"
    $CFLAGS += " -I#{inc}"
    inc
  end

  def joint(parent, child)
    parent + @file_separator + child
  end

end

javahome = ENV['JAVA_HOME']
if !javahome.nil?
  raise "JAVA_HOME is not directory." unless File.directory?(javahome)
  p = Path.new
  inc = p.include(javahome, 'include')
  Dir.open(inc).each do |d|
    next if d[0] == ?.
    if File.directory?(p.joint(inc, d))
      p.include(inc, d)
      break
    end
  end
else
	raise "JAVA_HOME is not setted."
end


def create_rjb_makefile
  if have_header("jni.h") && (have_header("dl.h") || have_header("ruby/dl.h")) #for ruby_1_9
    have_func("locale_charset", "iconv.h")
    have_func("nl_langinfo", "langinfo.h")
    have_func("setlocale", "locale.h")
    have_func("getenv")
    $defs << "-DRJB_RUBY_VERSION_CODE="+RUBY_VERSION.gsub(/\./, '')
    create_header
    create_makefile("rjbcore")
  end
end

case RUBY_PLATFORM
when /mswin32/
  $CFLAGS += ' /W3'
when /cygwin/, /mingw/
  $defs << '-DNONAMELESSUNION'
end
create_rjb_makefile
