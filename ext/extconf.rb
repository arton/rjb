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
  p = Path.new
  inc = p.include(javahome, 'include')
  Dir.open(inc).each do |d|
    next if d[0] == ?.
    if File.directory?(p.joint(inc, d))
      p.include(inc, d)
      break
    end
  end
end

def create_rjb_makefile
  if have_header("jni.h")
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
